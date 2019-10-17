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

#ifndef _SYS_PAGE_H
#define _SYS_PAGE_H

/* pmap[] has an entry for every physical page on the system.
   (the index is the frame number, i.e., the page's pgno_t). */

#define PMAP_UNKNOWN    0       /* unknown; only valid during page_init() */
#define PMAP_UNAVAIL    1       /* address space not usable RAM */
#define PMAP_FREE       2       /* on free list */
#define PMAP_KERNEL     3       /* kernel load image */
#define PMAP_PMAP       4       /* occupied by pmap[] array */
#define PMAP_PTE        5       /* used by process page tables */
#define PMAP_ANON       6       /* anonymous RAM assigned to process */
#define PMAP_SLAB       7       /* belongs to a slab */

struct pmap
{
    unsigned char type;         /* PMAP_* */

    union {
        struct proc *proc;      /* PMAP_PTE: owner process */
        unsigned long vaddr;    /* PMAP_ANON: virtual address in proc */
        struct slab *slab;      /* PMAP_SLAB: associated slab */

        long u;
    } u;

    LIST_ENTRY(pmap) list;
};

/* get the index of a virtual address 'v' in the page
   table at 'level', where 'level' is:

   3: page map level 4 (returns index of PML4E)
   2: page directory-pointer (returns index of PDPE)
   1: page directory (returns index of PDE)
   0: page table (returns index of PTE proper) */

#define PTES_PER_TABLE  512

#define PTE_INDEX(v,level) \
  ((((unsigned long) v) >> (((level) * 9) + PAGE_SHIFT)) & (PTES_PER_TABLE-1))

/* Intel/AMD give the entries at different page-table levels distinct names
   (see above), but we call them all PTEs- they're mostly the same, but be
   aware that not all of the bits defined are valid at all levels. */

typedef unsigned long pte_t;

#define PTE_G       0x0000000000000100L     /* global */
#define PTE_2MB     0x0000000000000080L     /* 2MB mapping */
#define PTE_D       0x0000000000000040L     /* dirty */
#define PTE_A       0x0000000000000020L     /* accessed */
#define PTE_U       0x0000000000000004L     /* user-accessible */
#define PTE_W       0x0000000000000002L     /* writeable */
#define PTE_P       0x0000000000000001L     /* present */

#define PTE_ADDR(x)     ((x) & ~(0xFFF))    /* address portion */
#define PTE_FLAGS(x)    ((x) & 0xFFF)       /* flags/other bits */

#ifdef _KERNEL

extern struct pmap pmap[];
extern pte_t proto_pml4[];

extern pgno_t page_alloc();
extern pte_t *pte_alloc();
extern pte_t *page_pte();
extern char *page_phys();

#endif /* _KERNEL */

#endif /* _SYS_PAGE_H */

/* vi: set ts=4 expandtab: */
