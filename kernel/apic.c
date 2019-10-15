/* Copyright (c) 2018 Charles E. Youse (charles@gnuless.org).
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

#include "../include/sys/queue.h"
#include "../include/sys/param.h"
#include "../include/sys/types.h"
#include "../include/sys/page.h"
#include "../include/sys/proc.h"
#include "../include/sys/sched.h"

/* local APIC definitions. for now, we use it in xAPIC (memory-mapped) mode;
   until the compiler supports inline asm this is faster than MSR access. */

#define LAPIC_READ(r)    (*((unsigned *) (LAPIC_BASE + ((r) << 4))))
#define LAPIC_WRITE(r,v) (*((unsigned *) (LAPIC_BASE + ((r) << 4))) = (v))

#define LAPIC_ID            0x02        /* identification */
#define LAPIC_EOI           0x0B        /* end of interrupt */
#define LAPIC_SPUR          0x0F        /* spurious interrupt vector */
#define LAPIC_TMRx          0x18        /* trigger modes (TMR0..7 0x18-0x1F) */
#define LAPIC_CMCI_LVT      0x2F        /* corrected machine check */
#define LAPIC_ICRLO         0x30        /* interrupt command[31:0] */
#define LAPIC_ICRHI         0x31        /* interrupt command[63:32] */
#define LAPIC_TIMER_LVT     0x32        /* timer */
#define LAPIC_THERMAL_LVT   0x33        /* thermal monitor */
#define LAPIC_PERF_LVT      0x34        /* performance counter */
#define LAPIC_LINT0_LVT     0x35        /* local interrupt pin 0 */
#define LAPIC_LINT1_LVT     0x36        /* local interrupt pin 1 */
#define LAPIC_ERROR_LVT     0x37        /* internal error */

#define LAPIC_LVT_MASK      0x00010000  /* common to *_LVT: LVT is masked */
#define LAPIC_SPUR_ENABLE   0x00000100  /* APIC enable bit */
#define LAPIC_ICRLO_BUSY    0x00001000  /* IPI delivery in progress */

#define LAPIC_ICR_IPI_OTHERS    0x000C4000  /* regular IPI to other CPUs */
#define LAPIC_ICR_IPI_INIT      0x00004500  /* init IPI to target CPU */
#define LAPIC_ICR_IPI_STARTUP   0x00004600  /* startup IPI to target CPU */

/* called by each CPU during startup. this is probably overly pedantic, as
   the firmware should leave the local APICs in a well-defined state. also,
   some of these LVTs may not exist on all APICs, and there may be others.
   (at some point we might have to get more fancy and so some probing.) */

lapic_init()
{
    LAPIC_WRITE(LAPIC_SPUR, LAPIC_SPUR_ENABLE | VECTOR_SPURIOUS);

    LAPIC_WRITE(LAPIC_CMCI_LVT, LAPIC_LVT_MASK);
    LAPIC_WRITE(LAPIC_TIMER_LVT, LAPIC_LVT_MASK);
    LAPIC_WRITE(LAPIC_THERMAL_LVT, LAPIC_LVT_MASK);
    LAPIC_WRITE(LAPIC_PERF_LVT, LAPIC_LVT_MASK);
    LAPIC_WRITE(LAPIC_LINT0_LVT, LAPIC_LVT_MASK);
    LAPIC_WRITE(LAPIC_LINT1_LVT, LAPIC_LVT_MASK);
    LAPIC_WRITE(LAPIC_ERROR_LVT, LAPIC_LVT_MASK);

    LAPIC_WRITE(LAPIC_EOI, 0);      /* clear any pending interrupt */
}

/* internal use only, for lapic_startcpu() and lapic_schedipi() */

static
lapic_ipi(target, ipi, vector)
{
    long flags;

    ipi |= vector;

    flags = lock();

    while (LAPIC_READ(LAPIC_ICRLO) & LAPIC_ICRLO_BUSY)
        /* wait until not busy */ ;

    LAPIC_WRITE(LAPIC_ICRHI, target << 24);
    LAPIC_WRITE(LAPIC_ICRLO, ipi);

    unlock(flags);
}

/* fire up another CPU (identified by its APIC ID). this is not robust and
   will just hang if the CPU never comes up. also, the delay loop is bogus;
   once we have some kind of driver for a timer source we should use that. */

lapic_startcpu(target, entry)
int entry();
{
    int i;

    lapic_ipi(target, LAPIC_ICR_IPI_INIT, 0);
    for (i = 0; i < 1000000; ++i) ;
    lapic_ipi(target, LAPIC_ICR_IPI_STARTUP, ADDR_TO_PGNO(entry));
}

/* I/O APIC definitions. like the CMOS or CRTC, the I/O APIC only directly
   exposes a register index and a window, so access is not atomic. */

#define IOAPIC_IOREGSEL     0x00000000  /* register index */
#define IOAPIC_IOWIN        0x00000010  /* window to register data */

#define IOAPIC_ID           0x00
#define IOAPIC_VER          0x01
#define IOAPIC_RTE(x)       (0x10 + ((x) * 2))

#define IOAPIC_RTELO_MASK       0x00010000  /* interrupt masked */
#define IOAPIC_RTELO_LEVEL      0x00008000  /* level-sensitive interrupt */
#define IOAPIC_RTELO_ACTLOW     0x00002000  /* active-low interrupt */

#define IOAPIC_WRITE(r, v) \
    do { \
        long flags = lock(); \
        (*(unsigned *)(IOAPIC_BASE + IOAPIC_IOREGSEL)) = (r); \
        (*(unsigned *)(IOAPIC_BASE + IOAPIC_IOWIN)) = (v); \
        unlock(flags); \
    } while(0)

#define IOAPIC_READ(r, v) \
    do { \
        long flags = lock(); \
        (*(unsigned *)(IOAPIC_BASE + IOAPIC_IOREGSEL)) = (r); \
        (v) = (*(unsigned *)(IOAPIC_BASE + IOAPIC_IOWIN)); \
        unlock(flags); \
    } while(0)

/* disable the RTE at the I/O APIC. this is too slow, but will do for now. */

ioapic_disable(rte)
{
    int v;

    IOAPIC_READ(IOAPIC_RTE(rte), v);
    IOAPIC_WRITE(IOAPIC_RTE(rte), v | IOAPIC_RTELO_MASK);
}

/* called by the BSP during startup. whereas the APs need only initialize
   their local APICs, the BSP must also set up the I/O APIC, and, moreover,
   it must first map them both into kernel space for everybody's benefit. */

apic_init()
{
    pte_t *pte;
    int max_rte;
    int i;

    pte = page_pte(&proc0, LAPIC_BASE, PTE_P | PTE_2MB);
    *pte = LAPIC_BASE | PTE_2MB | PTE_W | PTE_P;
    pte = page_pte(&proc0, IOAPIC_BASE, PTE_P | PTE_2MB);
    *pte = IOAPIC_BASE | PTE_2MB | PTE_W | PTE_P;

    /* initialize the I/O APIC: configure all the RTEs
       to broadcast, and mask them off until later. */

    IOAPIC_READ(IOAPIC_VER, max_rte);
    max_rte = (max_rte >> 16) & 0xFF;

    for (i = 0; i <= max_rte; ++i) {
        IOAPIC_WRITE(IOAPIC_RTE(i) + 1, 0xFF000000);
        ioapic_disable(i);
    }

    lapic_init();
}

/* vi: set ts=4 expandtab: */
