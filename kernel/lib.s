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

; save(proc) struct proc *proc;
; resume(proc) struct proc *proc;
;
; kernel analogs of setjmp()/longjmp() in userland.
; save() returns 0 to the saver, and 1 when resumed.

.global _save
_save:          pop rcx                 ; RIP
                mov rdx, qword [rsp]    ; 'proc'

                mov qword [rdx, PROC_RIP], rcx

                pushfq
                pop rax
                mov qword [rdx, PROC_RFLAGS], rax

                mov qword [rdx, PROC_RBX], rbx
                mov qword [rdx, PROC_RBP], rbp
                mov qword [rdx, PROC_RSI], rsi
                mov qword [rdx, PROC_RDI], rdi
                mov qword [rdx, PROC_R8], r8
                mov qword [rdx, PROC_R9], r9
                mov qword [rdx, PROC_R10], r10
                mov qword [rdx, PROC_R11], r11
                mov qword [rdx, PROC_R12], r12
                mov qword [rdx, PROC_R13], r13
                mov qword [rdx, PROC_R14], r14
                mov qword [rdx, PROC_R15], r15
                mov qword [rdx, PROC_RSP], rsp

                ; if CR0.TS is set, the FPU state does
                ; not belong to this process: don't save.

                mov rax, cr0
                test rax, 0x08      ; CR0.TS bit
                jnz _save_skipfp
                fxsave qword [rdx, PROC_FXSAVE]

_save_skipfp:   xor eax, eax
                jmp rcx

.global _resume
_resume:        mov rdx, qword [rsp, 8]             ; 'proc'

                mov rbx, qword [rdx, PROC_RBX]
                mov rbp, qword [rdx, PROC_RBP]
                mov rsi, qword [rdx, PROC_RSI]
                mov rdi, qword [rdx, PROC_RDI]
                mov r8, qword [rdx, PROC_R8]
                mov r9, qword [rdx, PROC_R9]
                mov r10, qword [rdx, PROC_R10]
                mov r11, qword [rdx, PROC_R11]
                mov r12, qword [rdx, PROC_R12]
                mov r13, qword [rdx, PROC_R13]
                mov r14, qword [rdx, PROC_R14]
                mov r15, qword [rdx, PROC_R15]

                cli ; atomic context switch

                mov rsp, qword [rdx, PROC_RSP]
                mov rax, qword [rdx, PROC_CR3]
                mov cr3, rax

                seg gs
                mov qword [TSS_CURPROC], rdx

                mov rax, cr0        ; set CR0.TS since
                or rax, 0x08        ; the FPU state is
                mov cr0, rax        ; now unknown.

                mov rax, qword [rdx, PROC_RFLAGS]
                push rax
                popfq ; interrupts re-enabled here (usually)

                mov rdx, qword [rdx, PROC_RIP]
                mov eax, 1
                jmp rdx

; struct tss *this() - return a pointer to this CPU's TSS

.global _this
_this:          seg gs
                mov rax, qword [TSS_THIS]
                ret

; inb(port) - read byte from I/O port

.global _inb
_inb:           xor eax, eax
                mov dx, word [rsp, 8]   ; 'port'
                in al
                ret

; outb(port, byte) - write byte to I/O port

.global _outb
_outb:          mov dx, word [rsp, 8]   ; 'port'
                mov al, byte [rsp, 16]  ; 'byte'
                out al
                ret

; long lock() - disable interrupts
; unlock(flags) long flags - restore previous interrupt state
;
; lock() disables interrupts and returns the previous RFLAGS so
; a subsequent call to unlock() can re-enable them if needed.
;
; unlock() loads RFLAGS with 'flags' previously returned from lock(),
; the effect being to re-enable interrupts if lock() disabled them.

.global _lock
_lock:          pushfq
                pop rax
                cli
                ret

.global _unlock
_unlock:        mov rax, qword [rsp,8]
                push rax
                popfq
                ret

; int bsf(v) long v; return the least significant bit set, or -1 if none

.global _bsf
_bsf:           bsf rax, qword [rsp,8]  ; 'v'
                jz _bsf_none
                ret
_bsf_none:      mov eax, -1
                ret

; spin() - disable interrupts and acquire the scheduler spinlock
; unspin() - release the scheduler spinlock and enable interrupts

.global spin_lock ; in locore.s
.global _spin
_spin:          cli
_spin_1:        lock
                bts dword [spin_lock], 0
                jc _spin_2
                ret
_spin_2:        pause
                test dword [spin_lock], 1
                jnz _spin_2
                jmp _spin_1

.global _unspin
_unspin:        mov dword [spin_lock], 0
                sti
                ret

; vi: set ts=4 expandtab:
