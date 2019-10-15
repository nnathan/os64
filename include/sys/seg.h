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

#ifndef _SYS_SEG_H
#define _SYS_SEG_H

/* a 64-bit task state segment. note that this structure is "offset", that is,
   its first member is actually 4 bytes into the actual TSS, because the TSS
   elements are otherwise misaligned. we avoid awkwardness by ensuring that
   we access the TSS via an offset pointer (via the GS segment).

   we make no use of the architecturally-defined fields, except RSP0. we do,
   however, reserve an entire page for the TSS for our per-CPU variables. */

struct tss
{
    unsigned long rsp0;
    unsigned long rsp1;
    unsigned long rsp2;

    unsigned long reserved0;

    unsigned long ist1;
    unsigned long ist2;
    unsigned long ist3;
    unsigned long ist4;
    unsigned long ist5;
    unsigned long ist6;
    unsigned long ist7;

    unsigned char reserved1[10];
    unsigned short iomap;

    /*
     * end of architecturally-defined section.
     *
     * the rest of these are os/64 per-cpu variables; do not move
     * them around without keeping the offsets in defs.s in sync!
     */

    struct tss *this;           /* pointer to self */
    struct proc *curproc;       /* currently executing process */
};

#ifdef _KERNEL

extern struct tss tss0;     /* BSP's TSS is predefined in locore */

/* returns a pointer to this CPU's TSS. when we add inline asm
   in the compiler, this should be reimplemented as a macro */

extern struct tss *this();

/* the GDT is defined in locore.s. empty space is reserved
   between gdt_free[] and gdt_end[] for dynamic allocation. */

extern unsigned long gdt[], gdt_free[], gdt_end[];

/* bootstrapping variables. the startup code common to every CPU consults
   these variables to set up its per-CPU state and determine where to enter
   the kernel. the BSP's values are predetermined (see locore.s). */

extern unsigned short boot_tr;      /* per-CPU task state segment selector */
extern unsigned long boot_tss;      /* per-CPU GS (overlays TSS) */
extern int (*boot_entry)();

#endif /* _KERNEL */

#endif /* _SYS_SEG_H */

/* vi: set ts=4 expandtab: */
