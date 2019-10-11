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
#include "../include/sys/acpi.h"

struct madt *madt;

/* the 8-bit checksum of a valid ACPI structure is 0 */

static
sum(buf, len)
char *buf;
{
    unsigned char sum = 0;

    while (len--)
        sum += *buf++;

    return sum == 0;
}

/* the RSDP will be located in one of several well-defined areas. for
   now, only check the BIOS ROM, but account for future possibilities. */

static struct {
    unsigned base;
    unsigned top;
} area[] = {
    { 0x000E0000, 0x00100000 }          /* BIOS ROM */
};

#define NR_AREAS sizeof(area)/sizeof(*area)

/* find relevant ACPI data. for now, all we're interested in is
   the MADT, which acpi_cpu() uses to identify available CPUs. */

acpi_init()
{
    struct rsdt *rsdt;
    int nr_sdts;
    int i;

    /* find valid RSDT via RSDP */

    for (i = 0, rsdt = NULL; (i < NR_AREAS) && !rsdt; ++i) {
        unsigned addr = area[i].base;

        while (addr < area[i].top) {
            struct rsdp *rsdp = (struct rsdp *) addr;

            if ((rsdp->sig1 == RSDP_SIG1) && (rsdp->sig2 == RSDP_SIG2)
                && sum(rsdp, sizeof(struct rsdp)))
            {
                rsdt = (struct rsdt *) rsdp->rsdt;

                if ((rsdt->sdt.sig) == RSDT_SIG && sum(rsdt, rsdt->sdt.len))
                    break;
                else
                    rsdt = NULL;
            }

            addr += 0x10;
        }
    }

    if (rsdt == NULL) panic("can't find ACPI RSDP/RSDT");

    /*
     * now locate the MADT amongst the SDTs listed in the RSDT.
     */

    nr_sdts = (rsdt->sdt.len - (sizeof(rsdt) - sizeof(rsdt->sdts))) / 4;

    for (i = 0; i < nr_sdts; ++i) {
        struct sdt *sdt = (struct sdt *) rsdt->sdts[i];
        if ((sdt->sig == MADT_SIG) && sum(sdt, sdt->len)) {
            madt = (struct madt *) sdt;
            return;
        }
    }

    panic("can't find ACPI MADT");
}

/* return the entry for the nth CPU in the system, or NULL if not present */

struct madt_cpu *
acpi_cpu(n)
{
    unsigned base = (unsigned) madt;
    unsigned offset;

    offset = ((unsigned) madt->entries) - base;
    while (offset < madt->sdt.len) {
        struct madt_entry *entry = (struct madt_entry *) (offset + base);

        if ((entry->type == MADT_ENTRY_CPU) && !n--)
            return (struct madt_cpu *) entry;

        offset += entry->len;
    }

    return NULL;
}

/* vi: set ts=4 expandtab: */
