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

#include "../include/stddef.h"
#include "../include/sys/types.h"
#include "../include/sys/queue.h"
#include "../include/sys/page.h"
#include "../include/sys/sched.h"
#include "../include/sys/proc.h"
#include "../include/sys/seg.h"

/* the scheduler is a simple strict-priority scheduler.

   processes that are waiting to be scheduled are on a runq[]. these
   queues are indexed by process priority. 'runqs' is a bit set, with
   a bit associated with each runq, which is set when that runq[] is
   not empty. this is simply to make scheduling decisions faster.

   processes that are waiting on a channel are on a sleepq[]. the channel
   (traditionally the address of an object of interest) is hashed by the
   macro SLEEPQ() to give an index into sleepq[].

   processes currently executing are this()->curproc on some CPU. there must
   ALWAYS be a process executing on a processor, and there are idle processes
   for every CPU at PRIORITY_IDLE to ensure this is always possible. */

static token_t tokens = TOKEN_ALL;      /* all tokens held by proc0 */
static unsigned long runqs;             /* bit set for non-empty runq[] */
static TAILQ_HEAD(,proc) runq[NR_RUNQS];        /* index by proc->priority */
static TAILQ_HEAD(,proc) sleepq[NR_SLEEPQS];    /* index by SLEEPQ() */

/* simple hash function for sleep channels */

#define SLEEPQ(channel) ((((unsigned) (channel)) >> 3) % NR_SLEEPQS)

/* place 'proc' at the head or tail of the correct runq[] */

#define SETRUNTAIL(proc) \
    do { \
        TAILQ_INSERT_TAIL(&runq[(proc)->priority], (proc), q_links); \
        runqs |= 1L << (proc)->priority; \
    } while (0)

#define SETRUNHEAD(proc) \
    do { \
        TAILQ_INSERT_HEAD(&runq[(proc)->priority], (proc), q_links); \
        runqs |= 1L << (proc)->priority; \
    } while (0)

/* true if a process with a priority higher than the
   specified priority is waiting in a runq[] */

#define WAITING(priority) (runqs && (bsf(runqs) < (priority)))

/* called before scheduling begins. TAILQs require initialization, and
   the spinlock is in non-initialized RAM, so unspin() to set its state.
   as a side effect of unspin(), interrupts will be re-enabled here. */

sched_init()
{
    int q;

    for (q = 0; q < NR_RUNQS; ++q) TAILQ_INIT(&runq[q]);
    for (q = 0; q < NR_SLEEPQS; ++q) TAILQ_INIT(&sleepq[q]);

    unspin();
}

/* LOCKED: select the best process to run and switch into it. "best" means
   the highest-priority process who needs only tokens that are free. */

static
sched()
{
    struct proc *proc;
    unsigned long qs;
    int q;

    qs = runqs;
    for (;;) {
        q = bsf(qs);
        if (q == -1) panic("runq empty");

        proc = TAILQ_FIRST(&runq[q]);
        while (proc) {
            if ((proc->tokens & tokens) == 0) {
                if (save(this()->curproc))
                    return; /* we've been resumed */
                else {
                    TAILQ_REMOVE(&runq[q], proc, q_links);
                    if (TAILQ_EMPTY(&runq[q])) runqs &= ~(1L << q);
                    resume(proc);
                }
            }

            proc = TAILQ_NEXT(proc, q_links);
        }

        qs &= ~(1L << q);
    }
}

/* put us at the tail of our runq and reschedule. the effect of this is to give
   way to another runnable higher- or same-priority process if one exists. */

yield()
{
    struct proc *proc = this()->curproc;
    token_t have = proc->tokens;

    spin();
    SETRUNTAIL(proc);
    tokens &= ~have;
    sched();
    tokens |= have;
    unspin();
}

/* put us at the head of our runq and reschedule. this differs from
   yield() in that we will not yield to same-priority processes. */

preempt()
{
    struct proc *proc = this()->curproc;
    token_t have = proc->tokens;

    spin();

    /* no point in invoking the scheduler if there
       aren't any higher-priority procs in runq[] */

    if (WAITING(proc->priority)) {
        SETRUNHEAD(proc);
        tokens &= ~have;
        sched();
        tokens |= have;
    }

    unspin();
}

/* acquire the specified tokens. returns the set of tokens actually acquired,
   rather than all the tokens- this allows acquire()/release() pairs to nest.
   this is a scheduling point: the caller may be blocked to give way to a
   higher-priority process, whether the tokens wanted are available or not. */

token_t
acquire(wanted)
token_t wanted;
{
    struct proc *proc = this()->curproc;
    token_t have = proc->tokens;

    wanted &= ~have; /* ignore what we already have */
    if (wanted == 0) return 0;

    spin();
    proc->tokens |= wanted;

    if ((tokens & wanted) || WAITING(proc->priority)) {
        /* either the tokens aren't available, or there's
           a higher-priority process ready to run. */

        tokens &= ~have;
        SETRUNHEAD(proc);
        sched();
    }

    tokens |= proc->tokens;
    unspin();
    return wanted;
}

/* release the specified tokens. 'unwanted' is expected to be the return
   value of the most recent call to acquire() - token acquisition nests,
   so the caller might not have actually acquired the tokens it asked for.
   like acquire(), the caller may be blocked if a higher-priority process
   is ready to run- it might be waiting for one of the released tokens. */

release(unwanted)
token_t unwanted;
{
    struct proc *proc = this()->curproc;
    token_t have = proc->tokens;

    if (unwanted == 0) return;
    if ((have & unwanted) != unwanted) panic("release() unheld tokens");

    spin();
    have &= ~unwanted;
    proc->tokens = have;
    tokens &= ~unwanted;

    if (WAITING(proc->priority)) {
        SETRUNHEAD(proc);
        tokens &= ~have;
        sched();
        tokens |= have;
    }

    unspin();
}

/* put the current process to sleep on the specified channel. */

sleep(channel, flags)
char *channel;
{
    struct proc *proc = this()->curproc;
    token_t have = proc->tokens;
    int q = SLEEPQ(channel);

    proc->flags |= flags;
    proc->channel = channel;

    spin();
    TAILQ_INSERT_TAIL(&sleepq[q], proc, q_links);
    tokens &= ~have;
    sched();
    tokens |= have;
    unspin();

    proc->flags &= ~flags;
}

/* wake up all processes sleeping on the specified channel */

wakeup(channel)
char *channel;
{
    int q = SLEEPQ(channel);
    struct proc *proc, *next;

    spin();
    proc = TAILQ_FIRST(&sleepq[q]);

    while (proc) {
        next = TAILQ_NEXT(proc, q_links);

        if (proc->channel == channel) {
            TAILQ_REMOVE(&sleepq[q], proc, q_links);
            SETRUNTAIL(proc);
        }

        proc = next;
    }

    unspin();
}

/* set a new process runnable. */

run(proc)
struct proc *proc;
{
    spin();
    SETRUNTAIL(proc);
    unspin();
}

idle()
{
    for (;;) preempt();
}

/* panic prints a message and halts the system. this is considered part of
   the scheduler because it needs to halt all system activity on all CPUs. */

panic(msg)
char *msg;
{
    printf("panic: %s\n", msg);
    for (;;) ;
}

/* vi: set ts=4 expandtab: */
