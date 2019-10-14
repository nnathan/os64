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

#ifndef _SYS_PARAM_H
#define _SYS_PARAM_H

#define PAGE_SIZE   4096            /* bytes per page */
#define PAGE_SHIFT  12              /* log2(PAGE_SIZE) */

/* kernel stacks are fixed in size. one per process, and never swapped out.
   luckily we don't nest interrupts in the traditional sense, so these don't
   need to be too large; on the other hand, 64-bit stacks are large by their
   very nature. the right size is yet to be determined. guess for now. */

#define KSTACK_PAGES    2           /* 8K kernel stacks */

/* boundaries of user virtual address space. */

#define USER_BASE   0xFFFFFF8000000000L     /* beginning of text */
#define KSTACK_TOP  0x0000000000000000L     /* kernel stack at very top */

/* for now, we assume the system has exactly one I/O APIC, and that the
   APICs are memory-mapped in their standard locations. beware: the APIC
   initialization code assumes these addresses are 2MB-page aligned. */

#define LAPIC_BASE  0x00000000FEE00000L
#define IOAPIC_BASE 0x00000000FEC00000L

/* limit of physical address space: all RAM and memory-mapped I/O devices
   must live below this physical address. this limit determines the max
   size of pmap[], the required size of pgno_t, and the number of system
   PML4Es, among other things. keep those in mind before increasing this.
   note: this value will be effectively rounded down to page boundary. */

#define PHYSMAX     (128L * 1024 * 1024 * 1024)     /* 128GB */

#endif /* _SYS_PARAM_H */

/* vi: set ts=4 expandtab: */
