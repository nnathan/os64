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

#ifndef _SYS_SLAB_H
#define _SYS_SLAB_H

/* minimum size of a slab-allocated object; also determines the alignment
   of slab objects. this should be a power of two and must at least as big
   as sizeof(struct slab_page) and sizeof(struct slab_free). see below. */

#define SLAB_MIN    64

/* struct slab is an opaque object to its users; clients wishing to
   slab-allocate must declare a slab and call slab_init(). internally,
   this has obvious housekeeping information and a list of slab_pages
   that have at least one object free. (full pages are unlinked from
   the list so we don't needlessly traverse them during allocation.) */

struct slab
{
    int obj_size;       /* size of objects in this slab */
    int per_page;       /* number of objects that fit in a page */

    LIST_HEAD(,slab_page) page_list;
};

/* every page allocated in the slab has a struct 'slab_page' header. */

struct slab_page
{
    struct slab *parent;                /* associated slab */
    LIST_ENTRY(slab_page) page_links;      /* our siblings */

    /* a list and count of all the free objects in this slab_page.
       when nr_free reaches parent->per_page, we free the page. */

    int nr_free;
    LIST_HEAD(,slab_free) free_list;
};

/* unallocated entries are occupied by a 'slab_free'
   to link them together in a list for quick access. */

struct slab_free
{
    LIST_ENTRY(slab_free) free_links;
};

#ifdef _KERNEL

char *slab_alloc();

#endif /* _KERNEL */

#endif /* _SYS_SLAB_H */

/* vi: set ts=4 expandtab: */
