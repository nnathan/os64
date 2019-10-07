; the locore, so called because it lives at the bottom of memory.
; this must be the first in link order so that this is the beginning
; of the executable image. note that data items here are purposely
; declared in text (vs data or bss) to ensure that they are < 64K, so
; the real-mode code, running at seg 0, can get to them without tricks.

.text
.bits 16

KERNEL_ADDRESS=0x1000   ; address of a.out header (@ 4K)
.global _exec		; give it a name so the kernel can find it
_exec=KERNEL_ADDRESS

BOOTCMD_SZ=128          ; length of bootcmd[] buffer
_bootcmd=0x540          ; BOOTCMD_SZ bytes - must agree with boot!

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
.global _e820_count
_e820_count:    .dword 0

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
                inc dword [_e820_count]
                test ebx,ebx
                jz warn_bios
                jmp e820_loop
e820_carry:     cmp dword [_e820_count], 0      ; carry can be set
                jz e820_error          ; as long as it's not first

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

                mov al, 0x20                    ; ICW2 = vectors 0x20-0x2F
                out 0x21, al
                mov al, 0x28
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
restart64:      mov ax, 0x20                    ; kernel data (sort of)
                mov ds, ax
                mov ss, ax
                mov es, ax
                mov fs, ax
                mov gs, ax

		; XXX: determine CPU ID, select appropriate stack
		; and load task and GS registers accordingly...
		; for now, we assume we're on the BSP and GO!

.global _main
		jmp _main

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
gdt:            .word 0, 0, 0, 0        ; null descriptor

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

                .word 0x67              ; 0x38 - 64-bit TSS selector for CPU0
                .word tss0
                .word 0x8900
                .word 0, 0, 0, 0, 0

gdt_ptr:        .word gdt_ptr-gdt-1
                .dword gdt

;
; TSS for CPU0
;

.align 8
tss0:           .dword 0
                .qword 0                        ; RSP0
                .qword 0                        ; RSP1
                .qword 0                        ; RSP2
                .qword 0
                .qword 0                        ; IST1
                .qword 0                        ; IST2
                .qword 0                        ; IST3
                .qword 0                        ; IST4
                .qword 0                        ; IST5
                .qword 0                        ; IST6
                .qword 0                        ; IST7
                .qword 0
                .dword 0

.align 8
idt:    ; processor-defined traps

        .word trap_0, 0x18, 0x8f00, 0, 0, 0, 0, 0   ; 0x00 = divide by 0
        .word trap_1, 0x18, 0x8f00, 0, 0, 0, 0, 0   ; 0x01 = debug
        .word trap_2, 0x18, 0x8f00, 0, 0, 0, 0, 0   ; 0x02 = NMI
        .word trap_3, 0x18, 0x8f00, 0, 0, 0, 0, 0   ; 0x03 = int3 breakpt
        .word trap_4, 0x18, 0x8f00, 0, 0, 0, 0, 0   ; 0x04 = into overflow
        .word trap_5, 0x18, 0x8f00, 0, 0, 0, 0, 0   ; 0x05 = BOUND range
        .word trap_6, 0x18, 0x8f00, 0, 0, 0, 0, 0   ; 0x06 = invalid opcode
        .word trap_7, 0x18, 0x8f00, 0, 0, 0, 0, 0   ; 0x07 = x87 not avail
        .word trap_8, 0x18, 0x8f00, 0, 0, 0, 0, 0   ; 0x08 = double fault
        .word trap_9, 0x18, 0x8f00, 0, 0, 0, 0, 0   ; 0x09 = RESERVED
        .word trap_10, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x0A = invalid TSS
        .word trap_11, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x0B = seg not present
        .word trap_12, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x0C = stack fault
        .word trap_13, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x0D = GPF
        .word trap_14, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x0E = page fault
        .word trap_15, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x0F = RESERVED
        .word trap_16, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x10 = x87 fp
        .word trap_17, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x11 = alignment
        .word trap_18, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x12 = machine-check
        .word trap_19, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x13 = SIMD fp
        .word trap_20, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x14 = RESERVED
        .word trap_21, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x15 = RESERVED
        .word trap_22, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x16 = RESERVED
        .word trap_23, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x17 = RESERVED
        .word trap_24, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x18 = RESERVED
        .word trap_25, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x19 = RESERVED
        .word trap_26, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x1A = RESERVED
        .word trap_27, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x1B = RESERVED
        .word trap_28, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x1C = RESERVED
        .word trap_29, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x1D = virtualization
        .word trap_30, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x1E = security
        .word trap_31, 0x18, 0x8f00, 0, 0, 0, 0, 0  ; 0x1F = RESERVED

        ; legacy 8259 PIC interrupts are vectored here
        ; we shouldn't hear from these guys, except for
        ; spurious interrupts.

        .word spurious, 0x18, 0x8e00, 0, 0, 0, 0, 0     ; 0x20
        .word spurious, 0x18, 0x8e00, 0, 0, 0, 0, 0     ; 0x21
        .word spurious, 0x18, 0x8e00, 0, 0, 0, 0, 0     ; 0x22
        .word spurious, 0x18, 0x8e00, 0, 0, 0, 0, 0     ; 0x23
        .word spurious, 0x18, 0x8e00, 0, 0, 0, 0, 0     ; 0x24
        .word spurious, 0x18, 0x8e00, 0, 0, 0, 0, 0     ; 0x25
        .word spurious, 0x18, 0x8e00, 0, 0, 0, 0, 0     ; 0x26
        .word spurious, 0x18, 0x8e00, 0, 0, 0, 0, 0     ; 0x27
        .word spurious, 0x18, 0x8e00, 0, 0, 0, 0, 0     ; 0x28
        .word spurious, 0x18, 0x8e00, 0, 0, 0, 0, 0     ; 0x29
        .word spurious, 0x18, 0x8e00, 0, 0, 0, 0, 0     ; 0x2A
        .word spurious, 0x18, 0x8e00, 0, 0, 0, 0, 0     ; 0x2B
        .word spurious, 0x18, 0x8e00, 0, 0, 0, 0, 0     ; 0x2C
        .word spurious, 0x18, 0x8e00, 0, 0, 0, 0, 0     ; 0x2D
        .word spurious, 0x18, 0x8e00, 0, 0, 0, 0, 0     ; 0x2E
        .word spurious, 0x18, 0x8e00, 0, 0, 0, 0, 0     ; 0x2F

idt_ptr:        .word idt_ptr-idt-1
                .dword idt

.bits 64

trap_0:         push 0                  ; fake code
                push 0                  ; type 0
                jmp trap
trap_1:         push 0
                push 1
                jmp trap
trap_2:         push 0
                push 2
                jmp trap
trap_3:         push 0
                push 3
                jmp trap
trap_4:         push 0
                push 4
                jmp trap
trap_5:         push 0
                push 5
                jmp trap
trap_6:         push 0
                push 6
                jmp trap
trap_7:         push 0
                push 7
                jmp trap
trap_8:         ; CPU-provided
                push 8
                jmp trap
trap_9:         push 0
                push 9
                jmp trap
trap_10:        ; CPU-provided
                push 10
                jmp trap
trap_11:        ; CPU-provided
                push 11
                jmp trap
trap_12:        ; CPU-provided
                push 12
                jmp trap
trap_13:        ; CPU-provided
                push 13
                jmp trap
trap_14:        ; CPU-provided
                push 14
                jmp trap
trap_15:        push 0
                push 15
                jmp trap
trap_16:        push 0
                push 16
                jmp trap
trap_17:        ; CPU-provided
                push 17
                jmp trap
trap_18:        push 0
                push 18
                jmp trap
trap_19:        push 0
                push 19
                jmp trap
trap_20:        push 0
                push 20
                jmp trap
trap_21:        push 0
                push 21
                jmp trap
trap_22:        push 0
                push 22
                jmp trap
trap_23:        push 0
                push 23
                jmp trap
trap_24:        push 0
                push 24
                jmp trap
trap_25:        push 0
                push 25
                jmp trap
trap_26:        push 0
                push 26
                jmp trap
trap_27:        push 0
                push 27
                jmp trap
trap_28:        push 0
                push 28
                jmp trap
trap_29:        push 0
                push 29
                jmp trap
trap_30:        push 0
                push 30
                jmp trap
trap_31:        push 0
                push 31
                jmp trap

trap:           jmp trap

spurious:       iretq

;
; prototype page tables. identity-map the first 2GB here manually, and leave
; it to the C startup code to do the rest before it touches anything else.
;

.org 0x0FE0	; can't align to page boundaries with .align

_proto_pml4:    .qword proto_pdp + 0x03          ; R/W, P
                .fill 4088, 0
proto_pdp:      .qword proto_pgdir + 0x03        ; R/W, P
                .fill 4088, 0
proto_pgdir:    .qword 0x183                     ; G, 2MB, R/W, P
                .fill 4088, 0
