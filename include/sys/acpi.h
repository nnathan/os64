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

#ifndef _SYS_ACPI_H
#define _SYS_ACPI_H

/* The RSDP is the floating structure used to locate the RSDT. */

#define RSDP_SIG1 0x20445352     /* 'RSD ' */
#define RSDP_SIG2 0x20525450     /* 'PTR ' */

struct rsdp
{
    unsigned sig1, sig2;                /* RSDP_SIG1, RSDP_SIG2 */
    unsigned char sum;
    unsigned char dontcare[7];
    unsigned rsdt;                      /* physical address of RSDT */
};

/* all SDTs have a common header. they're differentiated by 'sig'. */

struct sdt
{
    unsigned sig;
    unsigned len;
    unsigned char dontcare0;
    unsigned char sum;
    unsigned char dontcare1[26];
};

/* the root SDT exists solely to point out where all the other SDTs are */

#define RSDT_SIG 0x54445352 /* 'RSDT' */

struct rsdt
{
    struct sdt sdt;

    unsigned sdts[1];   /* unbounded: physical addresses of other SDTs */
};

/* The MADT is the "multiple APIC descriptor table", which we
   use to enumerate the local APICs (CPUs) on the system. */

#define MADT_SIG 0x43495041     /* 'APIC' */

struct madt_entry
{
    unsigned char type;         /* MADT_ENTRY_* */
    unsigned char len;
};

#define MADT_ENTRY_CPU 0        /* madt_entry.type: local APIC (CPU) */

struct madt_cpu
{
    struct madt_entry entry;

    unsigned char dontcare;
    unsigned char id;           /* local APIC ID */
    unsigned char flags;        /* MADT_CPU_FLAGS_* */
};

#define MADT_CPU_ENABLED 0x01   /* madt_cpu.flags: CPU is enabled */

struct madt
{
    struct sdt sdt;

    unsigned lapic;                 /* local APIC MMIO address */
    unsigned flags;                 /* MADT_* below */
    struct madt_entry entries[1];   /* actually unbounded .. */
};

#define MADT_PICS 0x00000001    /* madt.flags: legacy 8259s installed */

#ifdef _KERNEL

extern struct madt *madt;
extern struct madt_cpu *acpi_cpu();

#endif /* _KERNEL */

#endif /* _SYS_ACPI_H */

/* vi: set ts=4 expandtab: */
