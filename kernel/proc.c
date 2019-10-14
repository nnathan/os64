/* Copyright (c) 2018 Charles E. Youse (charles@gnuless.org).
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

#include "../include/sys/param.h"
#include "../include/sys/queue.h"
#include "../include/sys/types.h"
#include "../include/sys/page.h"
#include "../include/sys/proc.h"

/* initialize a new struct proc to a sane state */

proc_init(proc)
struct proc *proc;
{
    /* usually, these fields will be overwritten with data from the parent
       (via a standard fork), but provide values here that apply to early
       procs that are hand-crafted by the kernel (proc0, the idle tasks) */

    proc->rsp = KSTACK_TOP;
    proc->rflags = 0; /* most importantly,, IF=0 */
}

/* allocate and map in the kernel stack for a process */

proc_kstack(proc)
struct proc *proc;
{
    unsigned long addr = KSTACK_TOP;
    int i;
    pgno_t pgno;
    pte_t *pte;

    for (i = 0; i < KSTACK_PAGES; ++i) {
        addr -= PAGE_SIZE;
        pgno = page_alloc();
        bzero(PGNO_TO_ADDR(pgno), PAGE_SIZE);
        pte = page_pte(proc, addr, PTE_P);
        *pte = PGNO_TO_ADDR(pgno) | PTE_P | PTE_W;
    }
}

/* vi: set ts=4 expandtab: */
