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
#include "../include/sys/queue.h"
#include "../include/sys/types.h"
#include "../include/sys/slab.h"
#include "../include/sys/param.h"
#include "../include/sys/page.h"
#include "../include/sys/sched.h"

/* allocate an object from 'slab'. */

char *
slab_alloc(slab)
struct slab *slab;
{
    struct slab_page *page;
    struct slab_free *free;
    pgno_t pgno;
    token_t tokens;
    int i;

    tokens = acquire(TOKEN_SLAB);

    if (LIST_EMPTY(&slab->page_list)) {
        /* no free objects in the slab, allocate new page,
           construct the free object list, and add to slab */

        pgno = page_alloc(PMAP_SLAB, slab);
        page = (struct slab_page *) PGNO_TO_ADDR(pgno);
        page->parent = slab;
        page->nr_free = slab->per_page;
        LIST_INIT(&page->free_list);
        LIST_INSERT_HEAD(&slab->page_list, page, page_links);

        free = (struct slab_free *) (((char *) page) + SLAB_MIN);
        for (i = 0; i < slab->per_page; ++i) {
            LIST_INSERT_HEAD(&page->free_list, free, free_links);
            free = (struct slab_free *) (((char *) free) + slab->obj_size);
        }
    }

    /* grab next free object on the slab page. unlink
       the page from the slab if that was the last one. */

    page = LIST_FIRST(&slab->page_list);
    free = LIST_FIRST(&page->free_list);
    LIST_REMOVE(free, free_links);
    if (--page->nr_free == 0) LIST_REMOVE(page, page_links);

    release(tokens);
    return (char *) free;
}

/* free a slab-allocated object. */

slab_free(free)
struct slab_free *free;
{
    struct slab *slab;
    struct slab_page *page;
    pgno_t pgno;
    token_t tokens;

    tokens = acquire(TOKEN_SLAB);

    pgno = ADDR_TO_PGNO(free);
    page = (struct slab_page *) PGNO_TO_ADDR(pgno);
    slab = page->parent;

    LIST_INSERT_HEAD(&page->free_list, free, free_links);
    ++page->nr_free;

    if (page->nr_free == 1) {
        /* we have free objects again, add page back to slab */
        LIST_INSERT_HEAD(&slab->page_list, page, page_links);
    }

    if (page->nr_free == page->parent->per_page) {
        /* all slab objects free on this page; free the page */
        LIST_REMOVE(page, page_links);
        page_free(pgno);
    }

    release(tokens);
}

/* vi: set ts=4 expandtab: */
