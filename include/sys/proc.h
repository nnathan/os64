/* Copyright (c) 2019 Charles E. Youse (charles@gnuless.org).
   All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#ifndef _SYS_PROC_H
#define _SYS_PROC_H

struct proc
{
    /* CPU context. these are accessed by save() and resume(), so
       do not move them around without ensuring defs.s is in sync. */

    pte_t *cr3;

    struct {
        unsigned long rsp;

        unsigned long rbx, rbp, rsi, rdi;
        unsigned long r8, r9, r10, r11;
        unsigned long r12, r13, r14, r15;

        unsigned long rflags;
        unsigned long rip;

        char fxsave[512];       /* ..must be 16-byte aligned.. */
    } cpu;

    /* the remaining fields are only accessed from C, so reordering is OK */

    pid_t pid;
    int flags;                          /* PROC_* (currently unused) */
    int priority;                       /* scheduling priority: PRIORITY_* */
    char *channel;                      /* event sleeping on */
    token_t tokens;                     /* all held (or required) tokens */

    LIST_HEAD(,pmap) pte_pages;         /* pages allocated for page tables */
    TAILQ_ENTRY(proc) all_links;        /* all_procs */
    TAILQ_ENTRY(proc) q_links;          /* runq[] or sleepq[] */
};

#ifdef _KERNEL

extern struct proc proc0;
extern struct slab proc_slab;

struct proc *proc_alloc();
extern pid_t fork();

#endif /* _KERNEL */

#endif /* _SYS_PROC_H */

/* vi: set ts=4 expandtab: */
