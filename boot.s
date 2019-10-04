; Copyright (c) 2018 Charles E. Youse (charles@gnuless.org). 
; All rights reserved.
; 
; Redistribution and use in source and binary forms, with or without
; modification, are permitted provided that the following conditions are met:
; 
; * Redistributions of source code must retain the above copyright notice, this
;   list of conditions and the following disclaimer.
; 
; * Redistributions in binary form must reproduce the above copyright notice,
;   this list of conditions and the following disclaimer in the documentation
;   and/or other materials provided with the distribution.
; 
; THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
; AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
; IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
; DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
; FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
; DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
; SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
; CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
; OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
; OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

; os/64 boot block
; this loader uses 32-bit LBNs, so it won't handle volumes > 16TB.

.bits 16

BIOS_SEG=0x07C0         ; where the BIOS loads us
BOOT_SEG=0x9000         ; where we relocate and run
BOOT_ADDR=0x90000       ; BOOT_SEG*16
KERN_SEG=0x0100         ; kernel loaded at 4K
MAX_KERNEL=585728       ; can load 0x01000 - 0x8FFFFF
DIRSIZ=28               ; filename length (from dir.h)
TIMEOUT_TICKS=182       ; boot default_name after ~ 10 seconds
LOCORE_CMDLINE=0x540    ; must agree with locore.s!
CMDLINE_SZ=128          ; in bytes

; the BIOS loads the first 512 of this code to 0x7C00,
; but we are linked at BOOT_SEG:0, so let's move there.

entry:          mov cx, 0x200       ; 256 words
                mov ax, BOOT_SEG
                mov es, ax
                mov ax, BIOS_SEG
                mov ds, ax
                xor si, si
                xor di, di
                cld
                rep
                movsw
                jmpf reloc, BOOT_SEG

;
; error - report an error to the user and hang
;
; In:   DS:SI = pointer to NUL-terminated error message
;

error:          push si
                mov si, error_msg
                call puts
                pop si
                call puts
                mov si, error_msg_tail
                call puts
stop:           jmp stop

error_msg:      .ascii "error: "
                .byte 0

error_msg_tail: .byte 13, 10, 0

; now we're running at the right place. normalize the
; segment registers and put the stack in a known place.

reloc:          mov ax, BOOT_SEG
                mov es, ax
                mov ds, ax
                cli
                mov ss, ax          ; stack goes at the
                mov sp, 0           ; top of our segment
                sti
                mov byte [drive], dl    ; save BIOS-supplied drive #

; check that the BIOS DAP extensions are present. this is
; probably overkill - do any 64-bit systems not support DAP?

                mov ah, 0x41
                mov bx, 0x55aa
                int 0x13
                jc no_dap
                cmp bx, 0xaa55
                jne no_dap
                and cx, 1
                jnz dap_ok

no_dap:         mov si, no_dap_msg          
                jmp error
no_dap_msg:     .ascii 'unsupported BIOS'
                .byte 0

dap_ok:

; now, use DAP to read the rest of the boot block. note that
; this involves overwriting ourselves with an identical copy.

                mov eax, 0
                mov di, 0
                call read_lbn
                jmp second      ; off to "second stage"

; 
; read_lbn - read a logical block
;
; In:   DI  = 4K target buffer
;       EAX = logical block number
;

DAP_SIZE=0      ; (byte) size of packet (NR_DAP_BYTES)
DAP_ZERO=1      ; (byte) always 0
DAP_COUNT=2     ; (word) sector count
DAP_OFS=4       ; (word) buffer offset
DAP_SEG=6       ; (word) buffer segment
DAP_LSN=8       ; (qword) logical sector number

NR_DAP_BYTES=0x10

read_lbn:       push si
                push edx
                push eax

                mov si, dap
                mov byte [si,DAP_SIZE], NR_DAP_BYTES
                mov byte [si,DAP_ZERO], 0
                mov word [si,DAP_COUNT], 8  ; 8 sectors = 4K
                mov word [si,DAP_OFS], di
                mov word [si,DAP_SEG], BOOT_SEG

                mov edx, eax        ; EDX:EAX = EAX << 3
                shr edx, 29
                shl eax, 3
                mov dword [si,DAP_LSN], eax
                mov dword [si,DAP_LSN+4], edx

                mov dl, byte [drive]
                mov ah, 0x42
                int 0x13
                jnc read_lbn_ok

                mov si, read_lbn_msg
                jmp error
read_lbn_msg:   .ascii "read_lbn"
                .byte 0

read_lbn_ok:    pop eax              
                pop edx
                pop si
                ret

; 
; puts - print a string on the console
;
; In:   SI = null-terminated string to print
;  

puts:           push ax
                push bx
                push si

                cld
                xor bx, bx
                mov ah, 0x0e
puts_loop:      lodsb
                cmp al, 0
                je puts_done
                int 0x10
                jmp puts_loop

puts_done:      pop si
                pop bx
                pop ax
                ret

; ============================================================================
;
; the superblock occupies 0x180 - 0x1FF. we define locations
; here, but the data is actually managed by the filesystem.
;
; if struct fs_super in fs.h changes, we need to update these.

BLOCK_SIZE=4096
BLOCK_SHIFT=12
SUPER_MAGIC=0xABE01E50
SUPER_MAGIC2=0x87CD

                        .org 0x180

super_magic:            .dword 0
super_flags:            .dword 0
super_ctime:            .qword 0
super_mtime:            .qword 0
super_nr_bmap_blocks:   .dword 0
super_nr_imap_blocks:   .dword 0
super_nr_ino_blocks:    .dword 0
super_reserved0:        .dword 0
super_nr_blocks:        .qword 0
super_nr_free_blocks:   .qword 0
super_nr_inodes:        .dword 0
super_nr_free_inodes:   .dword 0
super_reserved1:        .fill 60, 0
super_magic2:           .word 0
super_bios_magic:       .word 0

; ============================================================================

; REMINDER: this portion of the boot code is inaccessible until 
; the complete boot block is loaded by the first sector.

                .org 0x200

bad_fs:         mov si, bad_fs_msg
                jmp error
bad_fs_msg:     .ascii "not an os/64 filesystem"
                .byte 0

; do some sanity checks to see if we're on an os/64 volume.

second:         mov eax, dword [super_magic]
                cmp eax, SUPER_MAGIC
                jne bad_fs
                mov ax, word [super_magic2]
                cmp ax, SUPER_MAGIC2
                jne bad_fs

; looks ok. compute fs_ino_start for file routines.

                mov eax, 1
                add eax, dword [super_nr_bmap_blocks]
                add eax, dword [super_nr_imap_blocks]
                mov dword [fs_ino_start], eax

; the first time the prompt is displayed, we timeout after
; TIMEOUT_TICKS ticks and try to boot the default cmdline.

                mov si, banner_msg
                call puts

                call reset_timer
timeout_loop:   call get_timer
                cmp eax, TIMEOUT_TICKS
                jge boot
                mov ah, 1               ; key pressed?
                int 0x16
                jz timeout_loop         
                mov ah, 0               ; yes, read key
                int 0x16                ; to ignore it
                jmp manual
              
banner_msg:     .byte 13, 10, 10
                .ascii "os/64 boot block"
                .byte 13, 10, 10
                .ascii "auto-boot will begin in 10 seconds."
                .byte 13, 10
                .ascii "press any key for the boot prompt."
                .byte 13, 10
                .byte 0

manual_msg:     .byte 13, 10
                .ascii "default: "
                .byte 0

manual_msg_2:   .byte 13, 10, 10, 0

manual:         mov si, manual_msg
                call puts
                mov si, cmdline
                call puts
                mov si, manual_msg_2
                call puts
                jmp prompt

; prompt loop. display prompt, get user input, process, repeat.

SPC=0x20
CR=0x0D
BS=0x08
DEL=0x7F

prompt_msg:     .byte 13, 10
                .ascii "boot: "
                .byte 0

prompt:         mov si, prompt_msg
                call puts
            
                mov cx, CMDLINE_SZ  ; clear out cmdline
                mov di, cmdline
                xor ax, ax
                cld
                rep
                stosb

                xor di, di          ; DI = name length
                xor bx, bx          ; zero for INT 10H AH=0EH

input:          mov ah, 0           ; get key into AL
                int 0x16

                cmp al, CR          ; CR = end of input
                jne input_10       
                test di, di
                jz prompt
                jmp boot    
           
input_10:       cmp al, BS
                jne input_20

                test di, di
                jz input
            
                dec di

                mov ah, 0x0e            ; echo destructive BS
                int 0x10
                mov al, SPC
                int 0x10
                mov al, BS
                int 0x10
                jmp input         

input_20:       cmp al, SPC             ; ignore illegal chars
                jb input
                cmp al, DEL
                jnb input
                cmp di, CMDLINE_SZ-1    ; too long
                jae input

                mov byte [di,cmdline], al 
                inc di
                mov ah, 0x0e            ; echo
                int 0x10
        
                jmp input
               
; attempt to boot using the current cmdline. 

boot:           mov cx, DIRSIZ+1    ; clear 'name' (and NUL terminator)
                mov di, name
                xor ax, ax
                cld
                rep
                stosb

                mov cx, DIRSIZ      ; copy cmdline word to 'name'
                mov si, cmdline 
                mov di, name
boot_05:        lodsb
                test al, al         ; if zero,
                jz boot_10          ; then end of cmdline
                cmp al, 0x20        ; if space,
                jz boot_10          ; then end of kernel name
                stosb
                loop boot_05
            
boot_10:        mov si, boot_msg_1
                call puts
                mov si, name
                call puts
                mov si, boot_msg_2
                call puts

                call lookup
                cmp eax, 0
                jne boot_20
            
                mov si, not_found_msg
                call puts
                jmp prompt

not_found_msg:  .ascii "not found"
                .byte 0

boot_20:        call open_file                  ; open kernel file
                mov eax, dword [inode_size]
                call putn
                mov si, boot_msg_3
                call puts
                cmp eax, MAX_KERNEL
                jb boot_25
                mov si, too_big_msg
                call puts
                jmp prompt

too_big_msg:    .ascii ", too big."
                .byte 0
  
boot_25:        mov edx, eax                    ; EDX = bytes to go
                xor eax, eax                    ; EAX = current offset
                mov bx, KERN_SEG                ; BX = target segment 

boot_30:        cmp edx, 0          ; if EDX <= 0,
                jle boot_40         ; we're done

                mov di, kernel_buffer
                mov cx, BLOCK_SIZE
                call read_file

                push es             ; move from bounce buffer
                mov es, bx             
                mov si, kernel_buffer
                xor di, di
                cld
                rep 
                movsb
                pop es

                add bx, 0x100           ; 4K in paragraphs
                add eax, BLOCK_SIZE
                sub edx, BLOCK_SIZE
                jmp boot_30

boot_40:        mov si, boot_msg_4
                call puts

                xor ax, ax              ; copy cmdline where locore wants it
                mov es, ax
                mov si, cmdline
                mov di, LOCORE_CMDLINE
                cld
                mov cx, CMDLINE_SZ
                rep
                movsb

                jmpf 0x1000, 0          ; start kernel ... !

boot_msg_1:     .byte 13, 10
                .ascii "loading '"
                .byte 0
boot_msg_2:     .ascii "' ... "
                .byte 0
boot_msg_3:     .ascii " bytes"
                .byte 0
boot_msg_4:     .ascii ", done."
                .byte 13, 10, 10, 10, 0

        
;
; reset_timer - reset the BIOS tick counter
;

reset_timer:    push cx
                push dx
                push ax

                mov ah, 1
                mov cx, 0
                mov dx, 0
                int 0x1a

                pop ax
                pop dx
                pop cx
                ret

;
; get_timer - return the BIOS tick counter
;
; Out:  EAX = ticks
;

get_timer:      push cx
                push dx
                
                mov ah, 0
                int 0x1a
                mov ax, cx
                shl eax, 16
                mov ax, dx

                pop dx
                pop cx
                ret

;
; open_file - prepare to read a file (fill in inode buffer)
;
; In:   EAX = inode number of file
;

NR_INO_BYTES=128        ; bytes in an inode
INO_SHIFT=7             ; log2(NR_INO_BYTES)

open_file:      push eax                  
                push esi
                push di
                push cx

                mov esi, eax            ; SI = offset of inode
                shl esi, INO_SHIFT      ; within block
                and si, BLOCK_SIZE-1
                add si, buffer          ; and now within buffer

                shr eax, BLOCK_SHIFT-INO_SHIFT  ; EAX = block number
                add eax, dword [fs_ino_start]   ; where inode lives
                mov di, buffer
                call read_lbn

                mov cx, NR_INO_BYTES
                mov di, inode
                cld
                rep
                movsb                

                pop cx
                pop di
                pop esi
                pop eax
                ret

; read_file - read data from the file
;             (file last opened by open_file)

; In:   EAX = offset in file to read
;       CX = length to read
;       DI = destination buffer
;
; this isn't smart enough to read across block
; boundaries, because it doesn't have to be.

DIRECT_BYTES=32768      ; number of bytes in direct inode blocks

read_file:      push eax
                push si       
                push di
                push bx
                push cx
                
                mov bx, di              ; save caller's target buffer
                mov di, buffer          ; our target buffer   
    
                mov si, ax              ; SI = offset of data in block
                and si, BLOCK_SIZE-1
                add si, buffer          ; now offset into buffer

                cmp eax, DIRECT_BYTES   ; need to read indirect block?
                jae read_file_10 

                shr eax, BLOCK_SHIFT    ; compute direct block
                mov eax, dword [,eax*8, inode_direct]
                jmp read_file_20        ; go read it

read_file_10:   sub eax, DIRECT_BYTES   ; index into indirect block
                shr eax, BLOCK_SHIFT
                push eax                ; save

                mov eax, dword [inode_indirect] 
                call read_lbn

                pop eax                 ; recover index
                mov eax, dword [,eax*8, buffer]

read_file_20:   call read_lbn           ; read data block
                mov di, bx              ; recover caller's target buffer
                cld                     ; SI was computed above
                rep                     ; CX came from caller
                movsb                   ; copy to caller

                pop cx
                pop bx
                pop di
                pop si
                pop eax
                ret

;
; lookup - find 'name' in the root directory
;
; Out:  EAX = inode number (0 = not found)
;

NR_DIRENT_BYTES=32      ; bytes per directory entry

lookup:         push ebx
                push cx
                push si
                push di
    
                mov eax, 1      ; open root directory
                xor ebx, ebx    ; start at offset 0
                call open_file

lookup_10:      cmp ebx, dword [inode_size]     ; end of directory?
                jb lookup_20                   
                mov eax, 0                      ; yes, return not found
                jmp lookup_done

lookup_20:      mov eax, ebx                ; read directory entry
                mov cx, NR_DIRENT_BYTES
                mov di, dirent
                call read_file

                cmp dword [d_ino], 0        ; unused entry?
                jz lookup_50                ; yes, skip

                mov si, d_name
                mov di, name
                mov cx, DIRSIZ
                cld

lookup_30:      lodsb                   ; compare names
                cmp al, byte [di]
                jne lookup_50
                inc di
                test al, al
                jz lookup_40
                loop lookup_30

lookup_40:      mov eax, dword [d_ino]  ; match, return inode
                jmp lookup_done
             
lookup_50:      add ebx, NR_DIRENT_BYTES    ; next entry
                jmp lookup_10               ; try again
                
lookup_done:    pop di
                pop si
                pop cx
                pop ebx
                ret

; 
; putn - print number in decimal
;
; In:   EAX = number
;

putn:           push eax
                push ebx
                push ecx
                push edx
                
                push 0          ; terminator
                mov ecx, 10

putn_10:        xor edx, edx    ; divide EDX:EAX by 10
                div ecx
                add dx, 0x30    ; make remainder a digit
                push dx         ; push onto stack
                test eax, eax   ; loop until eax = 0
                jnz putn_10

putn_20:        pop ax
                test ax, ax
                jz putn_done
                mov ah, 0x0e
                xor bx, bx
                int 0x10         
                jmp putn_20

putn_done:      pop edx
                pop ecx
                pop ebx
                pop eax
                ret

; ============================================================================

; like the superblock data, the command line is subject to manipulation with
; the 'mkboot' utility, so what we see here is just the default.

                .org 0x1000-CMDLINE_SZ

cmdline:        .ascii 'kernel'
                .byte 0
               
                .org 0x1000     ; boot block is exactly 4K

; ============================================================================

; REMINDER: this data storage area is NOT LOADED as part of the boot 
; block, and as such is for UNINITIALIZED data only.

drive:          .byte 0                 ; BIOS drive
name:           .fill DIRSIZ+1,0        ; user-entered kernel name

.align 4
fs_ino_start:   .dword 0                ; first block of inodes

.align 8
dap:            .fill NR_DAP_BYTES, 0   ; BIOS DAP

.align 8
dirent:                                 ; struct dirent
d_ino:          .dword 0
d_name:         .fill DIRSIZ,0          

.align 8
inode:                                  ; struct fs_inode  
inode_mode:     .word 0        
inode_links:    .word 0
inode_uid:      .word 0
inode_gid:      .word 0
inode_size:     .qword 0
inode_atime:    .qword 0
inode_ctime:    .qword 0
inode_mtime:    .qword 0

inode_direct:   .fill 64, 0     ; 8 direct blocks
inode_indirect: .qword 0
inode_double:   .qword 0
inode_triple:   .qword 0

                .org 0x2000 

buffer:         .fill 4096, 0       ; scratch buffer for file functions
kernel_buffer:  .fill 4096, 0       ; bounce buffer for kernel load
