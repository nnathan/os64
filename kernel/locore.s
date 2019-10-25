; the locore, so called because it lives at the bottom of memory.
; this must be the first in link order so that this is the beginning
; of the executable image. note that data items here are purposely
; declared in text (vs data or bss) to ensure that they are < 64K, so
; the real-mode code, running at seg 0, can get to them without tricks.

.text
.bits 16

KERNEL_ADDRESS=0x1000   ; address of a.out header (@ 4K)
.global _exec           ; give it a name so the kernel can find it
_exec=KERNEL_ADDRESS

BOOTCMD_SZ=128          ; length of bootcmd[] buffer
_bootcmd=0x540          ; BOOTCMD_SZ bytes - must agree with boot!

.global _proc0          ; must be 16-byte aligned
_proc0=0x800

.global spin_lock       ; scheduler lock for spin/unspin
spin_lock=0xFC0         ; goes on its own cache line (hopefully)

.global _pmap
_pmap=0x100000          ; pmap[] starts at 1MB mark

; start is the entry point recorded in the a.out header, but the 
; boot code just jumps blindly, so it had better be first in text.

.text
.global start
start:          cli

                xor ax, ax              ; normalize segments and
                mov ds, ax              ; move to trampoline stack.
                mov es, ax
                mov ss, ax
                mov sp, trampoline_stack

                mov ax, cs              ; on the APs, the startup IPI
                shl ax, 4               ; will set CS to KERNEL_ADDRESS/16
                cmp ax, KERNEL_ADDRESS  ; (vs 0x07c0 or 0x0000 from BIOS)
                jnz not_ap              ; so we distinguish the BSP
                jmpf ap_restart, 0      ; from the APs that way.
not_ap:         jmpf bsp_restart, 0

;
; 1. open the A20 gate if necessary. 
;    try a couple of techniques, least-intrusive first.
;
                  
bsp_restart:    sti
                call test_a20           ; might be open already
                jnz check_cpu

                mov ax, 0x2401          ; try BIOS function
                int 0x15
                call test_a20
                jnz check_cpu
                
                in al, 0x92             ; try fast A20 gate
                or al, 2
                and al, 0xfe
                out 0x92, al
                call test_a20
                jnz check_cpu

                mov si, a20_stuck_msg
                jmp error

a20_stuck_msg:  .ascii "A20 stuck"
                .byte 0

;
; 2. be sure we're running on a 64-bit CPU.
;

bad_cpu:        mov si, bad_cpu_msg
                jmp error
bad_cpu_msg:    .ascii "64-bit CPU required"
                .byte 0

check_cpu:      pushfd                  ; check for CPUID support
                pushfd
                xor dword [esp], 0x200000
                popfd
                pushfd
                pop eax
                xor eax, dword [esp]
                popfd
                and eax, 0x200000
                jz bad_cpu

                mov eax, 0x80000000     ; check for 64-bit support
                cpuid
                cmp eax, 0x80000001
                jb bad_cpu
                mov eax, 0x80000001
                cpuid
                test edx, 0x20000000
                jz bad_cpu
                jmp e820_map

;
; 3. read the memory map from the BIOS. we place the map at the bottom
;    of the region reserved for the trampoline stack, which of course 
;    grows downwards from the top. the track must be large enough to
;    ensure that the map data isn't overwritten before we read it later.
;

.align 4
.global _nr_e820
_nr_e820:       .dword 0

.align 8
.global _e820_map
TRAMPOLINE_STACK_SIZE=1024
_e820_map:      .fill TRAMPOLINE_STACK_SIZE, 0
trampoline_stack:

E820_ENTRY_SIZE=24
SMAP=0x534D4150

e820_error:     mov si, e820_error_msg
                jmp error
e820_error_msg: .ascii "can't get BIOS memory map"
                .byte 0

e820_map:       xor ebx, ebx
                mov di, _e820_map
                mov edx, SMAP
e820_loop:      mov eax, 0xe820
                mov ecx, E820_ENTRY_SIZE
                int 0x15
                jc e820_carry
                cmp eax, SMAP
                jne e820_error
                add di, E820_ENTRY_SIZE
                inc dword [_nr_e820]
                test ebx,ebx
                jz warn_bios
                jmp e820_loop
e820_carry:     cmp dword [_nr_e820], 0         ; carry can be set
                jz e820_error                   ; as long as it's not first

;
; 4. warn the BIOS that we're heading for long mode.
;   

warn_bios:      mov ax, 0xec00
                mov bl, 2
                int 0x15

; 
; 5. kill the 8259s: generate out-of-the-way vectors and mask them.
;

kill_pics:      cli

                mov al, 0x11                    ; ICW1 = initialize
                out 0x20, al
                out 0xa0, al

                mov al, 0xE0                    ; ICW2 = vectors 0xE0-0xEF
                out 0x21, al
                mov al, 0xE8
                out 0xA1, al

                mov al, 4                       ; ICW3 = master/slave
                out 0x21, al
                mov al, 2
                out 0xa1, al

                mov al, 1                       ; ICW4 = 8086 mode
                out 0x21, al
                out 0xa1, al

                mov al, 0xff                    ; mask off all interrupts
                out 0x21, al
                out 0xa1, al

;
; 6. turn off NMIs. they usually just mean the computer is exploding. i
;    prefer the "don't tell the public the asteroid is coming" approach.
;

kill_nmi:       in al, 0x70
                or al, 0x80
                out 0x70, al

; 
; 7. move to 32-bit protected mode.
;

ap_restart:     lgdt word [gdt_ptr]
                lidt word [idt_ptr]
                mov ax, 1
                lmsw ax
                jmpf restart32, 0x08
.bits 32
restart32:      mov ax, 0x10                    ; 32-bit data selectors
                mov ds, ax
                mov esp, trampoline_stack

;
; 8. and proceed to 64-bit protected mode.
;

                mov eax, _proto_pml4            ; prototype page tables
                mov cr3, eax
                mov eax, cr4                    ; enable PAE and SSE
                or eax, 0x220
                mov cr4, eax

                mov ecx, 0xc0000080             ; enable long mode in EFER
                rdmsr
                or eax, 0x100
                wrmsr

                mov eax, cr0                    ; enable paging
                or eax, 0x80000000
                mov cr0, eax
                jmpf restart64, 0x18
.bits 64
restart64:      xor eax, eax                    ; data segments are null
                mov ds, ax
                mov ss, ax
                mov es, ax
                mov fs, ax
                mov gs, ax

                ; load per-CPU registers: TSS and GS base registers

                mov ax, word [_boot_tr]
                ltr ax

                mov eax, dword [_boot_tss]
                mov edx, dword [_boot_tss+4]
                mov ecx, IA32_GS_BASE
                wrmsr
                mov ecx, IA32_KERNEL_GS_BASE
                wrmsr

                push qword [_boot_proc]
                call qword [_boot_entry]        ; and enter kernel!
                cli                             ; should never return ..
whoops:         jmp whoops

.global _main
.global _boot_entry
.global _boot_proc
.global _boot_tss
.global _boot_tr
.global _boot_flag
.align 8
_boot_entry:    .qword _main
_boot_proc:     .qword 0
_boot_tss:      .qword _tss0
_boot_tr:       .word 0x38
_boot_flag:     .byte 0

;
; error - report an error and halt.
;
; In:   SI = null-terminated error message
;

.bits 16

error:          push si
                mov si, error_msg
                call puts
                pop si
                call puts
                cli
here:           jmp here

error_msg:      .ascii "ERROR: "
                .byte 0

;
; puts - put string on console using BIOS 10H
;
; In:   SI = null-terminated string
; Out:  SI = one past string terminator
;

.bits 16

puts:           push ax
                push bx

                cld
                xor bx, bx
                mov ah, 0x0e
puts_loop:      lodsb
                cmp al, 0
                je puts_done
                int 0x10
                jmp puts_loop

puts_done:      pop bx
                pop ax
                ret

;
; test_a20 - test A20 line enabled.
;
; In:   None
; Out:  AX destroyed.
;       flag Z=0 if A20 enabled
;

.bits 16

a20_alias:      .word 0 

test_a20:       push es

                mov ax, 0xFFFF
                mov es, ax

                ; basic idea is to see if a20_alias is 
                ; aliased around the 1MB mark. only works 
                ; if a20_alias is in the first ~ 64K, 
                ; which is why it's declared in text above.

                mov word [a20_alias], 0x1234
                seg es
                cmp word [a20_alias+0x10], 0x1234
                jne test_a20_done

                mov word [a20_alias], 0x4321
                seg es
                cmp word [a20_alias+0x10], 0x4321

test_a20_done:  pop es
                ret

;
; global descriptor table
;

.align 8
.global _gdt
.global _gdt_free
.global _gdt_end
_gdt:           .word 0, 0, 0, 0        ; null descriptor

                .word 0xFFFF            ; 0x08 = 32-bit code
                .word 0
                .word 0x9A00
                .word 0x00CF

                .word 0xFFFF            ; 0x10 = 32-bit data
                .word 0
                .word 0x9200
                .word 0x00CF

                .word 0                 ; 0x18 - 64-bit kernel code
                .word 0
                .word 0x9800
                .word 0x0020

                .word 0                 ; 0x20 - 64-bit kernel data (NOT USED)
                .word 0
                .word 0x9200
                .word 0x0000

                .word 0                 ; 0x28 - 64-bit user code (0x2B)
                .word 0
                .word 0xF800
                .word 0x0020

                .word 0                 ; 0x30 - 64-bit user data (0x33)
                .word 0
                .word 0xF200
                .word 0

                .word 0x0FFF            ; 0x38 - 64-bit TSS selector for CPU0
                .word tss0
                .word 0x8900
                .word 0
                .word 0, 0, 0, 0        ; 0x40 - (TSS is double-length)

_gdt_free:      .fill 384, 0            ; 48 empty entries
_gdt_end:

gdt_ptr:        .word gdt_ptr-_gdt-1
                .dword _gdt

.align 8
idt:    ; processor-defined traps

        .word vector_00, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x00 = divide by 0
        .word vector_01, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x01 = debug
        .word vector_02, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x02 = NMI
        .word vector_03, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x03 = int3 breakpt
        .word vector_04, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x04 = into overflow
        .word vector_05, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x05 = BOUND range
        .word vector_06, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x06 = invalid opcode
        .word vector_07, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x07 = x87 not avail
        .word vector_08, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x08 = double fault
        .word vector_09, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x09 = RESERVED
        .word vector_10, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x0A = invalid TSS
        .word vector_11, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x0B = seg not present
        .word vector_12, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x0C = stack fault
        .word vector_13, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x0D = GPF
        .word vector_14, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x0E = page fault
        .word vector_15, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x0F = RESERVED
        .word vector_16, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x10 = x87 fp
        .word vector_17, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x11 = alignment
        .word vector_18, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x12 = machine-check
        .word vector_19, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x13 = SIMD fp
        .word vector_20, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x14 = RESERVED
        .word vector_21, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x15 = RESERVED
        .word vector_22, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x16 = RESERVED
        .word vector_23, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x17 = RESERVED
        .word vector_24, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x18 = RESERVED
        .word vector_25, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x19 = RESERVED
        .word vector_26, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x1A = RESERVED
        .word vector_27, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x1B = RESERVED
        .word vector_28, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x1C = RESERVED
        .word vector_29, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x1D = virtualization
        .word vector_30, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x1E = security
        .word vector_31, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x1F = RESERVED

        ; ISR vectors

        .word vector_32, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x20
        .word vector_33, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x21
        .word vector_34, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x22
        .word vector_35, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x23
        .word vector_36, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x24
        .word vector_37, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x25
        .word vector_38, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x26
        .word vector_39, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x27
        .word vector_40, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x28
        .word vector_41, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x29
        .word vector_42, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x2A
        .word vector_43, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x2B
        .word vector_44, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x2C
        .word vector_45, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x2D
        .word vector_46, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x2E
        .word vector_47, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x2F

        .word vector_48, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x30
        .word vector_49, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x31
        .word vector_50, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x32
        .word vector_51, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x33
        .word vector_52, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x34
        .word vector_53, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x35
        .word vector_54, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x36
        .word vector_55, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x37
        .word vector_56, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x38
        .word vector_57, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x39
        .word vector_58, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x3A
        .word vector_59, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x3B
        .word vector_60, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x3C
        .word vector_61, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x3D
        .word vector_62, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x3E
        .word vector_63, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x3F

        .word vector_64, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x40
        .word vector_65, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x41
        .word vector_66, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x42
        .word vector_67, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x43
        .word vector_68, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x44
        .word vector_69, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x45
        .word vector_70, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x46
        .word vector_71, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x47
        .word vector_72, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x48
        .word vector_73, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x49
        .word vector_74, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x4A
        .word vector_75, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x4B
        .word vector_76, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x4C
        .word vector_77, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x4D
        .word vector_78, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x4E
        .word vector_79, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x4F

        .word vector_80, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x50
        .word vector_81, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x51
        .word vector_82, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x52
        .word vector_83, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x53
        .word vector_84, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x54
        .word vector_85, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x55
        .word vector_86, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x56
        .word vector_87, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x57
        .word vector_88, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x58
        .word vector_89, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x59
        .word vector_90, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x5A
        .word vector_91, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x5B
        .word vector_92, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x5C
        .word vector_93, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x5D
        .word vector_94, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x5E
        .word vector_95, 0x18, 0x8e00, 0, 0, 0, 0, 0  ; 0x5F

        .fill 2048, 0       ; 0x60 - 0xDF unused

        ; legacy 8259 PIC interrupts are vectored here
        ; we shouldn't hear from these guys, except for
        ; spurious interrupts.

        .word spurious, 0x18, 0x8e00, 0, 0, 0, 0, 0     ; 0xE0
        .word spurious, 0x18, 0x8e00, 0, 0, 0, 0, 0     ; 0xE1
        .word spurious, 0x18, 0x8e00, 0, 0, 0, 0, 0     ; 0xE2
        .word spurious, 0x18, 0x8e00, 0, 0, 0, 0, 0     ; 0xE3
        .word spurious, 0x18, 0x8e00, 0, 0, 0, 0, 0     ; 0xE4
        .word spurious, 0x18, 0x8e00, 0, 0, 0, 0, 0     ; 0xE5
        .word spurious, 0x18, 0x8e00, 0, 0, 0, 0, 0     ; 0xE6
        .word spurious, 0x18, 0x8e00, 0, 0, 0, 0, 0     ; 0xE7
        .word spurious, 0x18, 0x8e00, 0, 0, 0, 0, 0     ; 0xE8
        .word spurious, 0x18, 0x8e00, 0, 0, 0, 0, 0     ; 0xE9
        .word spurious, 0x18, 0x8e00, 0, 0, 0, 0, 0     ; 0xEA
        .word spurious, 0x18, 0x8e00, 0, 0, 0, 0, 0     ; 0xEB
        .word spurious, 0x18, 0x8e00, 0, 0, 0, 0, 0     ; 0xEC
        .word spurious, 0x18, 0x8e00, 0, 0, 0, 0, 0     ; 0xED
        .word spurious, 0x18, 0x8e00, 0, 0, 0, 0, 0     ; 0xEE
        .word spurious, 0x18, 0x8e00, 0, 0, 0, 0, 0     ; 0xEF

        ; miscellaneous system vectors here

        .word tick, 0x18, 0x8e00, 0, 0, 0, 0, 0         ; VECTOR_TICK
        .word 0, 0, 0, 0, 0, 0, 0, 0                    ; 0xF1
        .word 0, 0, 0, 0, 0, 0, 0, 0                    ; 0xF2
        .word 0, 0, 0, 0, 0, 0, 0, 0                    ; 0xF3
        .word 0, 0, 0, 0, 0, 0, 0, 0                    ; 0xF4
        .word 0, 0, 0, 0, 0, 0, 0, 0                    ; 0xF5
        .word 0, 0, 0, 0, 0, 0, 0, 0                    ; 0xF6
        .word 0, 0, 0, 0, 0, 0, 0, 0                    ; 0xF7
        .word 0, 0, 0, 0, 0, 0, 0, 0                    ; 0xF8
        .word 0, 0, 0, 0, 0, 0, 0, 0                    ; 0xF9
        .word 0, 0, 0, 0, 0, 0, 0, 0                    ; 0xFA
        .word 0, 0, 0, 0, 0, 0, 0, 0                    ; 0xFB
        .word 0, 0, 0, 0, 0, 0, 0, 0                    ; 0xFC
        .word 0, 0, 0, 0, 0, 0, 0, 0                    ; 0xFD
        .word 0, 0, 0, 0, 0, 0, 0, 0                    ; 0xFE
        .word spurious, 0x18, 0x8e00, 0, 0, 0, 0, 0     ; VECTOR_SPURIOUS

idt_ptr:        .word idt_ptr-idt-1
                .dword idt

.bits 64
.global _trap

vector_00:      push 0                  ; fake code
                push _trap
                push 0                  ; number 0
                jmp vector
vector_01:      push 0
                push _trap
                push 1
                jmp vector
vector_02:      push 0
                push _trap
                push 2
                jmp vector
vector_03:      push 0
                push _trap
                push 3
                jmp vector
vector_04:      push 0
                push _trap
                push 4
                jmp vector
vector_05:      push 0
                push _trap
                push 5
                jmp vector
vector_06:      push 0
                push _trap
                push 6
                jmp vector
vector_07:      push 0
                push _trap
                push 7
                jmp vector
vector_08:      ; CPU-provided
                push _trap
                push 8
                jmp vector
vector_09:      push 0
                push _trap
                push 9
                jmp vector
vector_10:      ; CPU-provided
                push _trap
                push 10
                jmp vector
vector_11:      ; CPU-provided
                push _trap
                push 11
                jmp vector
vector_12:      ; CPU-provided
                push _trap
                push 12
                jmp vector
vector_13:      ; CPU-provided
                push _trap
                push 13
                jmp vector
vector_14:      ; CPU-provided
                push _trap
                push 14
                jmp vector
vector_15:      push 0
                push _trap
                push 15
                jmp vector
vector_16:      push 0
                push _trap
                push 16
                jmp vector
vector_17:      ; CPU-provided
                push _trap
                push 17
                jmp vector
vector_18:      push 0
                push _trap
                push 18
                jmp vector
vector_19:      push 0
                push _trap
                push 19
                jmp vector
vector_20:      push 0
                push _trap
                push 20
                jmp vector
vector_21:      push 0
                push _trap
                push 21
                jmp vector
vector_22:      push 0
                push _trap
                push 22
                jmp vector
vector_23:      push 0
                push _trap
                push 23
                jmp vector
vector_24:      push 0
                push _trap
                push 24
                jmp vector
vector_25:      push 0
                push _trap
                push 25
                jmp vector
vector_26:      push 0
                push _trap
                push 26
                jmp vector
vector_27:      push 0
                push _trap
                push 27
                jmp vector
vector_28:      push 0
                push _trap
                push 28
                jmp vector
vector_29:      push 0
                push _trap
                push 29
                jmp vector
vector_30:      push 0
                push _trap
                push 30
                jmp vector
vector_31:      push 0
                push _trap
                push 31
                jmp vector

.global _irq

vector_32:      push 0
                push _irq
                push 32
                jmp vector
vector_33:      push 0
                push _irq
                push 33
                jmp vector
vector_34:      push 0
                push _irq
                push 34
                jmp vector
vector_35:      push 0
                push _irq
                push 35
                jmp vector
vector_36:      push 0
                push _irq
                push 36
                jmp vector
vector_37:      push 0
                push _irq
                push 37
                jmp vector
vector_38:      push 0
                push _irq
                push 38
                jmp vector
vector_39:      push 0
                push _irq
                push 39
                jmp vector
vector_40:      push 0
                push _irq
                push 40
                jmp vector
vector_41:      push 0
                push _irq
                push 41
                jmp vector
vector_42:      push 0
                push _irq
                push 42
                jmp vector
vector_43:      push 0
                push _irq
                push 43
                jmp vector
vector_44:      push 0
                push _irq
                push 44
                jmp vector
vector_45:      push 0
                push _irq
                push 45
                jmp vector
vector_46:      push 0
                push _irq
                push 46
                jmp vector
vector_47:      push 0
                push _irq
                push 47
                jmp vector
vector_48:      push 0
                push _irq
                push 48
                jmp vector
vector_49:      push 0
                push _irq
                push 49
                jmp vector
vector_50:      push 0
                push _irq
                push 50
                jmp vector
vector_51:      push 0
                push _irq
                push 51
                jmp vector
vector_52:      push 0
                push _irq
                push 52
                jmp vector
vector_53:      push 0
                push _irq
                push 53
                jmp vector
vector_54:      push 0
                push _irq
                push 54
                jmp vector
vector_55:      push 0
                push _irq
                push 55
                jmp vector
vector_56:      push 0
                push _irq
                push 56
                jmp vector
vector_57:      push 0
                push _irq
                push 57
                jmp vector
vector_58:      push 0
                push _irq
                push 58
                jmp vector
vector_59:      push 0
                push _irq
                push 59
                jmp vector
vector_60:      push 0
                push _irq
                push 60
                jmp vector
vector_61:      push 0
                push _irq
                push 61
                jmp vector
vector_62:      push 0
                push _irq
                push 62
                jmp vector
vector_63:      push 0
                push _irq
                push 63
                jmp vector
vector_64:      push 0
                push _irq
                push 64
                jmp vector
vector_65:      push 0
                push _irq
                push 65
                jmp vector
vector_66:      push 0
                push _irq
                push 66
                jmp vector
vector_67:      push 0
                push _irq
                push 67
                jmp vector
vector_68:      push 0
                push _irq
                push 68
                jmp vector
vector_69:      push 0
                push _irq
                push 69
                jmp vector
vector_70:      push 0
                push _irq
                push 70
                jmp vector
vector_71:      push 0
                push _irq
                push 71
                jmp vector
vector_72:      push 0
                push _irq
                push 72
                jmp vector
vector_73:      push 0
                push _irq
                push 73
                jmp vector
vector_74:      push 0
                push _irq
                push 74
                jmp vector
vector_75:      push 0
                push _irq
                push 75
                jmp vector
vector_76:      push 0
                push _irq
                push 76
                jmp vector
vector_77:      push 0
                push _irq
                push 77
                jmp vector
vector_78:      push 0
                push _irq
                push 78
                jmp vector
vector_79:      push 0
                push _irq
                push 79
                jmp vector
vector_80:      push 0
                push _irq
                push 80
                jmp vector
vector_81:      push 0
                push _irq
                push 81
                jmp vector
vector_82:      push 0
                push _irq
                push 82
                jmp vector
vector_83:      push 0
                push _irq
                push 83
                jmp vector
vector_84:      push 0
                push _irq
                push 84
                jmp vector
vector_85:      push 0
                push _irq
                push 85
                jmp vector
vector_86:      push 0
                push _irq
                push 86
                jmp vector
vector_87:      push 0
                push _irq
                push 87
                jmp vector
vector_88:      push 0
                push _irq
                push 88
                jmp vector
vector_89:      push 0
                push _irq
                push 89
                jmp vector
vector_90:      push 0
                push _irq
                push 90
                jmp vector
vector_91:      push 0
                push _irq
                push 91
                jmp vector
vector_92:      push 0
                push _irq
                push 92
                jmp vector
vector_93:      push 0
                push _irq
                push 93
                jmp vector
vector_94:      push 0
                push _irq
                push 94
                jmp vector
vector_95:      push 0
                push _irq
                push 95
                jmp vector

.global _tick
tick:           push 0
                push _tick
                push 0
                jmp vector

.global _exit

vector:         push rcx
                push rdx
                push rax
                push rbx

                mov ecx, IA32_KERNEL_GS_BASE
                rdmsr
                mov ecx, IA32_GS_BASE
                wrmsr

                push rsp                    ; struct vector *
                call qword [rsp, 48]        ; requested handler
                pop rax

                mov rax, qword [rsp, 64]    ; interrupted CS
                and eax, 3                  ; check privilege level
                jz exit                     ; if it was kernel mode, skip

                call _exit  ; do "exiting to user mode" stuff

exit:           pop rbx
                pop rax
                pop rdx
                pop rcx
                add rsp, 24                 ; discard number, handler, code
spurious:       iretq

;
; TSS for CPU0; remember 'struct tss' is offset by 4
;

.global _tss0
.align 8
tss0:           .dword 0
_tss0:          .fill 4092, 0

;
; prototype page tables. identity-map the first 2GB here manually, and leave
; it to the C startup code to do the rest before it touches anything else.
;

.org 0x2FE0     ; can't align to page boundaries with .align

.global _proto_pml4
_proto_pml4:    .qword proto_pdp + 0x03          ; R/W, P
                .fill 4088, 0
proto_pdp:      .qword proto_pgdir + 0x03        ; R/W, P
                .fill 4088, 0
proto_pgdir:    .qword 0x183                     ; G, 2MB, R/W, P
                .fill 4088, 0

; vi: set ts=4 expandtab:
