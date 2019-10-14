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
#include "../include/a.out.h"
#include "../include/sys/param.h"
#include "../include/sys/types.h"
#include "../include/sys/queue.h"
#include "../include/sys/page.h"
#include "../include/sys/sched.h"
#include "../include/sys/clock.h"
#include "../include/sys/proc.h"

/* free pages are tracked by keeping their pmap[] entries on free_pages. */

static pgno_t nr_free_pages;
static LIST_HEAD(, pmap) free_pages;

page_free(pgno)
pgno_t pgno;
{
    token_t tokens;

    tokens = acquire(TOKEN_PMAP);
    pmap[pgno].type = PMAP_FREE;
    LIST_INSERT_HEAD(&free_pages, &pmap[pgno], list);
    ++nr_free_pages;
    wakeup(&nr_free_pages);
    release(tokens);
}

/* allocate a page. associate the pmap entry with the 'type' and 'u' given.
  guaranteed to succeed; will sleep to wait for free pages if needed. */

pgno_t
page_alloc(type, u)
long u;
{
    token_t tokens;
    struct pmap *pg;

    tokens = acquire(TOKEN_PMAP);

    while (nr_free_pages == 0)
        sleep(&time, 0);

    pg = LIST_FIRST(&free_pages);
    LIST_REMOVE(pg, list);
    --nr_free_pages;
    release(tokens);

    pg->type = type;
    pg->u.u = u;
    return (pgno_t) (pg - pmap);
}

/* return a pointer to the PTE for 'vaddr' in the address space of 'proc'.

   we abuse the PTE_* constants for 'flags' here rather than defining
   new ones. the only valid flags are (possibly combined):

   PTE_2MB: return the page directory PTE (covers 2MB range)
   PTE_P: create any/all intermediate tables as necessary

   if PTE_P is not given and there is no existing mapping, NULL is returned. */

pte_t *
page_pte(proc, vaddr, flags)
struct proc *proc;
char *vaddr;
{
    pte_t *table = proc->cr3;
    int level = 3;
    pte_t *pte;
    pgno_t pgno;

    for (;;) {
        pte = &table[PTE_INDEX(vaddr, level)];
        if ((level == 1) && (flags & PTE_2MB)) break;
        if (level == 0) break;

        if (!(*pte & PTE_P)) {
            if (flags & PTE_P) {
                pgno = page_alloc(PMAP_PTE, proc);
                table = (pte_t *) PGNO_TO_ADDR(pgno);
                bzero(table, PAGE_SIZE);
                *pte = PGNO_TO_ADDR(pgno) | PTE_P | PTE_W | PTE_U;
            } else
                return NULL;
        } else
            table = (pte_t *) PTE_ADDR(*pte);

        --level;
    }

    return pte;
}

/* locore.s queries the BIOS and exports e820_map[] and nr_e820 */

union e820
{
    struct {
        unsigned long base;
        unsigned long length;
        int type;
        int unused0;
    } bios;

    struct {
        pgno_t first;
        pgno_t last;
        int usable;
    } pages;
};

extern union e820 e820_map[];
extern int nr_e820;

#define E820_TYPE_USABLE 1

/* this is called very early by main(), with only the first 2MB mapped, to
 * initialize the pmap[] and complete the kernel identity-mapping of RAM. */

page_init()
{
    union e820 *entry;
    pgno_t pmapsz = 0;
    pgno_t pgno;
    pgno_t kernel_first, kernel_last;   /* pages consumed by kernel binary */
    pgno_t pmap_first, pmap_last;       /* pages consumed by pmap[] */
    pgno_t physmax;
    int i;

    physmax = ADDR_TO_PGNO(PHYSMAX - PAGE_SIZE);

    /* first, convert the BIOS map to its internal format. while we're at it,
       figure out the size of pmap[] by noting the highest usable RAM page. */

    for (i = 0, entry = e820_map; i < nr_e820; ++i, ++entry) {
        pgno_t first, last;

        if (entry->bios.type == E820_TYPE_USABLE) {
            /* for RAM pages, be pessimistic, rounding "inwards" */

            first = ADDR_TO_PGNO(entry->bios.base + PAGE_SIZE - 1);
            last = ADDR_TO_PGNO(entry->bios.base + entry->bios.length
                                - PAGE_SIZE);
            entry->pages.usable = 1;
            if (last > pmapsz) pmapsz = last;
        } else {
            /* for unusable pages, be greedy, rounding "outwards" */

            first = ADDR_TO_PGNO(entry->bios.base);
            last = ADDR_TO_PGNO(entry->bios.base + entry->bios.length - 1);
            entry->pages.usable = 0;
        }

        entry->pages.first = first;
        entry->pages.last = last;
    }

    if (pmapsz > physmax) pmapsz = physmax;

    /* next, determine the boundaries of usable RAM that are already in
       use: namely, the kernel's text/data/bss and the pmap[] itself. */

    kernel_first = ADDR_TO_PGNO(&exec);
    kernel_last = ADDR_TO_PGNO((unsigned long) &exec + exec.a_text
                    + exec.a_data + exec.a_bss + PAGE_SIZE - 1);

    pmap_first = ADDR_TO_PGNO(pmap);
    pmap_last = ADDR_TO_PGNO((unsigned long) (pmap + pmapsz) - 1);

    /* finally, iterate over all page frames in the system and categorize them
       accordingly. (pmap[0] is unavailable because pgno_t 0 means 'no page'.)
       at every 2MB boundary we make sure to extend the kernel page tables. */

    pmap[0].type = PMAP_UNAVAIL;

    for (pgno = 1; pgno < pmapsz; ++pgno) {
        if ((PGNO_TO_ADDR(pgno) % (2 * 1024 * 1024)) == 0) {
            pte_t *pte;

            pte = page_pte(&proc0, PGNO_TO_ADDR(pgno), PTE_2MB | PTE_P);
            *pte = PGNO_TO_ADDR(pgno) | PTE_2MB | PTE_G | PTE_W | PTE_P;
        }

        if ((pgno >= kernel_first) && (pgno <= kernel_last)) {
            pmap[pgno].type = PMAP_KERNEL;
            continue;
        }

        if ((pgno >= pmap_first) && (pgno <= pmap_last)) {
            pmap[pgno].type = PMAP_PMAP;
            continue;
        }

        /* the E820 map might have overlapping regions, so it's not sufficient
           to see that a page is in an available region to decide it's free. */

        pmap[pgno].type = PMAP_UNKNOWN;
        for (i = 0, entry = e820_map; i < nr_e820; ++i, ++entry) {
            if ((pgno >= entry->pages.first) && (pgno <= entry->pages.last)) {
                if (entry->pages.usable) {
                    if (pmap[pgno].type == PMAP_UNKNOWN)
                        pmap[pgno].type = PMAP_FREE;
                } else
                    pmap[pgno].type = PMAP_UNAVAIL;
            }
        }

        if (pmap[pgno].type == PMAP_FREE) page_free(pgno);
    }

    printf("%d pages, %d kernel, %d pmap, %d free\n",
            pmapsz,
            kernel_last - kernel_first + 1,
            pmap_last - pmap_first + 1,
            nr_free_pages);
}

/* vi: set ts=4 expandtab: */
