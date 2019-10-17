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

#include "../include/stddef.h"
#include "../include/sys/param.h"
#include "../include/sys/queue.h"
#include "../include/sys/types.h"
#include "../include/sys/page.h"
#include "../include/sys/slab.h"
#include "../include/sys/sched.h"
#include "../include/sys/proc.h"
#include "../include/sys/seg.h"

struct slab proc_slab;

/* these globals are protected by TOKEN_PROC */

pid_t last_pid;                 /* last assigned PID */
int nr_procs;                   /* number of processes */

TAILQ_HEAD(,proc) all_procs = TAILQ_HEAD_INITIALIZER(all_procs);

/* initialize a new struct proc to a sane state and add it to the all_procs
   list. this is meant to be called only from two places: early main() and
   proc_alloc(). in the latter case, TOKEN_PROC is held when this is called.
   in the former, there is no need because we're not scheduling yet. */

proc_init(pid, proc)
pid_t pid;
struct proc *proc;
{
    proc->pid = pid;
    proc->flags = 0;
    proc->priority = PRIORITY_IDLE;
    proc->tokens = 0;
    LIST_INIT(&proc->pte_pages);
    TAILQ_INSERT_HEAD(&all_procs, proc, all_links);
    ++nr_procs;

    /* usually, these fields will be overwritten with data from the parent
       (via a standard fork), but provide values here that apply to early
       procs that are hand-crafted by the kernel (proc0, the idle tasks) */

    proc->cpu.rsp = KSTACK_TOP;
    proc->cpu.rflags = 0; /* most importantly, IF=0 */
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
        pgno = page_alloc(PMAP_ANON, addr);
        bzero(PGNO_TO_ADDR(pgno), PAGE_SIZE);
        pte = page_pte(proc, addr, PTE_P);
        *pte = PGNO_TO_ADDR(pgno) | PTE_P | PTE_W;
    }
}

/* allocate a new proc struct. assign a process ID,
   initialize with sane defaults, attach kernel stack. */

struct proc *
proc_alloc()
{
    struct proc *new;
    struct proc *proc;
    token_t tokens;

    new = (struct proc *) slab_alloc(&proc_slab);

    tokens = acquire(TOKEN_PROC);

    /* assign an unused pid. this will loop forever if there are no free
       pids - we never release TOKEN_PROC so none will become free. it is
       up to other parts of the system to ensure we don't try to make a
       ridiculous number of processes. */

    do {
        ++last_pid;
        if (last_pid < 0) last_pid = 0;

        proc = TAILQ_FIRST(&all_procs);
        while (proc != NULL) {
            if (proc->pid == last_pid) break;
            proc = TAILQ_NEXT(proc, all_links);
        }
    } while (proc != NULL);

    proc_init(last_pid, new);
    release(tokens);

    /* allocate and initialize the top-level page tables, then stack.
       if/when we support more than 512GB of physical address space
       (PHYSMAX), we'll need to copy more than the first proto PML4E */

    new->cr3 = pte_alloc(new);
    new->cr3[0] = proto_pml4[0];    /* shared kernel mappings */
    proc_kstack(new);

    return new;
}

/* fork process. returns the pid of the child to the parent, 0 to the child. */

pid_t
fork()
{
    struct proc *parent = this()->curproc;
    struct proc *child;
    unsigned long addr;
    int i;
    pid_t pid;

    child = proc_alloc();
    pid = child->pid;

    child->flags = parent->flags;
    child->priority = parent->priority;
    child->tokens = parent->tokens;

    if (save(parent))
        return 0;   /* child */

    /* copy kernel stack. we'll refactor this when we actually
       have user addresses beyond the kernel stack to copy.. */

    addr = KSTACK_TOP;

    for (i = 0; i < KSTACK_PAGES; ++i) {
        char *src;
        char *dst;

        addr -= PAGE_SIZE;
        src = page_phys(parent, addr);
        dst = page_phys(child, addr);

        bcopy(src, dst, PAGE_SIZE);
    }

    bcopy(&parent->cpu, &child->cpu, sizeof(parent->cpu));
    run(child);

    return pid;
}

/* vi: set ts=4 expandtab: */
