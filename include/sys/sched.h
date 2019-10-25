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

/* flags for isr() (and 'struct isr' flags in sched.c) */

#define ISR_IOAPIC      0x00000001      /* source is I/O APIC pin */
#define ISR_LEVEL       0x00000002      /* source is level-sensitive */
#define ISR_ACTLOW      0x00000004      /* source is active-low */

/* IDT vector assignments: must match the IDT in locore.s.
   the first 32 vectors are architecturally-defined. */

#define VECTOR_DE           0x00        /* divide-by-zero */
#define VECTOR_DB           0x01        /* debug */
#define VECTOR_NMI          0x02        /* non-maskable interrupt */
#define VECTOR_BP           0x03        /* breakpoint */
#define VECTOR_OF           0x04        /* overflow */
#define VECTOR_BR           0x05        /* bound range */
#define VECTOR_UD           0x06        /* invalid opcode */
#define VECTOR_NM           0x07        /* device not available */
#define VECTOR_DF           0x08        /* double fault */
                                        /* 0x09 RESERVED */
#define VECTOR_TS           0x0A        /* invalid TSS */
#define VECTOR_NP           0x0B        /* segment not present */
#define VECTOR_SS           0x0C        /* stack */
#define VECTOR_GP           0x0D        /* general protection */
#define VECTOR_PF           0x0E        /* page fault */
                                        /* 0x0F RESERVED */
#define VECTOR_MF           0x10        /* x87 floating-point */
#define VECTOR_AC           0x11        /* alignment check */
#define VECTOR_MC           0x12        /* machine check */
#define VECTOR_XF           0x13        /* SIMD floating point */
                                        /* 0x14-0x1C RESERVED */
#define VECTOR_VC           0x1D        /* virtualization event */
#define VECTOR_SX           0x1E        /* security exception */
                                        /* 0x1F RESERVED */

#define VECTOR_ISR_BASE     0x20        /* ISR vectors start here */
#define NR_ISR_VECTORS              64      /* 0x20 - 0x5F */
#define VECTORS_PER_PRIORITY        16      /* architecturally-defined */

#define VECTOR_TICK         0xF0        /* APIC timer scheduling tick */
#define VECTOR_SPURIOUS     0xFF        /* APIC was just kidding */

/*
 * serializing tokens (max 64)
 */

typedef unsigned long token_t; 

#define TOKEN(x)        ((x) << 1L)     /* token builder: 0 <= x <= 63 */

#define TOKEN_PMAP      TOKEN(0)        /* page allocation/deallocation */
#define TOKEN_SLAB      TOKEN(1)        /* slab allocation/deallocation */
#define TOKEN_PROC      TOKEN(2)        /* global process information */
#define TOKEN_HIGH      TOKEN(3)        /* high-priority synchronization */
#define TOKEN_TTY       TOKEN(4)        /* serial device synchronization */
#define TOKEN_NET       TOKEN(5)        /* network device synchronization */
#define TOKEN_BLOCK     TOKEN(6)        /* block device synchronization */

#define TOKEN_ALL       (-1L)

/* Process priorities: lower number means higher priority. */

#define PRIORITY_HIGH       0           /* high-priority ISRs */
#define PRIORITY_TTY        1           /* serial device ISRs */
#define PRIORITY_NET        2           /* network device ISRs */
#define PRIORITY_BLOCK      3           /* block device ISRs */
#define PRIORITY_USER       4           /* user processes */
#define PRIORITY_IDLE       5           /* idle processes: always lowest */

#define NR_RUNQS           (PRIORITY_IDLE + 1)

/* Number of sleep queues. This is the number of hash buckets for 'channel'. */

#define NR_SLEEPQS          64          /* max is 64 (bits in qword) */

#ifdef _KERNEL

extern token_t acquire();
extern long lock();
extern resume();

#endif /* _KERNEL */

#endif /* _SYS_SCHED_H */

/* vi: set ts=4 expandtab: */
