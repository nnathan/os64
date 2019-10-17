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

#ifndef _SYS_SCHED_H
#define _SYS_SCHED_H

/*
 * serializing tokens (max 64)
 */

typedef unsigned long token_t; 

#define TOKEN(x)        ((x) << 1L)     /* token builder: 0 <= x <= 63 */

#define TOKEN_PMAP      TOKEN(0)        /* page allocation/deallocation */
#define TOKEN_SLAB      TOKEN(1)        /* slab allocation/deallocation */
#define TOKEN_PROC      TOKEN(2)        /* global process information */

#define TOKEN_ALL       (-1L)

/*
 * IDT vector assignments: must match the IDT in locore.s, obviously.
 */

#define VECTOR_SPURIOUS     0xFF        /* APIC was just kidding */

/* Process priorities: lower number means higher priority. */

#define PRIORITY_TTY        0           /* serial device ISRs */
#define PRIORITY_NET        1           /* network device ISRs */
#define PRIORITY_BLOCK      2           /* block device ISRs */
#define PRIORITY_USER       3           /* user processes */
#define PRIORITY_IDLE       4           /* idle processes: always lowest */

#define NR_RUNQS           (PRIORITY_IDLE + 1)

/* Number of sleep queues. This is the number of hash buckets for 'channel'. */

#define NR_SLEEPQS          64          /* max is 64 */

#ifdef _KERNEL

extern token_t acquire();
extern long lock();
extern resume();

#endif /* _KERNEL */

#endif /* _SYS_SCHED_H */

/* vi: set ts=4 expandtab: */
