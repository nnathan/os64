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

#include "../include/a.out.h"
#include "../include/sys/queue.h"
#include "../include/sys/types.h"
#include "../include/sys/sched.h"
#include "../include/sys/page.h"
#include "../include/sys/seg.h"
#include "../include/sys/acpi.h"
#include "../include/sys/proc.h"
#include "../include/sys/param.h"
#include "../include/sys/clock.h"

/* the APs enter here in their idle process contexts */

static
ap()
{
    boot_flag = 1;

    fpu_init();

    printf("AP %d started\n", lapic_id());

    lapic_init();
    lapic_ticker();

    idle();
}

/* allocate a TSS and idle process for, and start, each AP in turn */

static
start_aps()
{
    int n;
    struct madt_cpu *cpu;
    struct tss *tss;
    struct proc *proc;
    pgno_t pgno;
    unsigned long desc;

    for (n = 0; cpu = acpi_cpu(n); ++n) {
        if (cpu->id == lapic_id()) continue;
        if (!(cpu->flags & MADT_CPU_ENABLED)) continue;

        /* allocate a page for the AP TSS */

        pgno = page_alloc(PMAP_KERNEL, 0);
        boot_tss = PGNO_TO_ADDR(pgno);

        /* create GDT selector for AP's TR */

        desc = (boot_tss & 0x00FFFFFF)  << 16;
        desc |= (boot_tss & 0xFF000000) << 32;
        desc |= 0x0000890000000FFF;
        boot_tr = gdt_alloc(desc);
        gdt_alloc(boot_tss >> 32);

        /* initialize the TSS; remember we offset the struct */

        boot_tss += 4;
        tss = (struct tss *) boot_tss;
        tss_init(tss);

        /* now, allocate the AP's idle process with an entry point at
           ap(), and set the AP to resume that process on boot-up. */

        proc = proc_alloc();
        proc->cpu.rip = (long) ap;
        boot_entry = resume;
        boot_proc = proc;

        /* we start the APs synchronously because
           they share the trampoline stack. */

        boot_flag = 0;
        lapic_startcpu(cpu->id, &exec);

        while (boot_flag == 0) {
            /* call a harmless function so the compiler doesn't hold the flag
               in a register; change to 'volatile' when compiler supports it */

            lapic_id();
        }
    }
}

/* the BSP re-starts here properly situated on a kernel stack as proc0 */

static
bsp()
{
    fpu_init();
    apic_init();        /* disables all interrupt sources */
    sched_init();       /* initialize scheduler qs/lock, enable interrupts */
    release(TOKEN_ALL); /* the scheduler is safe now */
    lapic_ticker();     /* so we can start scheduling ticks */

    acpi_init();
    start_aps();

    idle();
}

/* the BSP enters the kernel at main(), on the trampoline stack,
   interrupts disabled, and only the first 2MB of RAM mapped. */

main()
{
    bzero(((char *) &exec) + exec.a_text + exec.a_data, exec.a_bss);

    tss_init(&tss0);
    cons_init();

    printf("os/64 (compiled %s %s)\n", __DATE__, __TIME__);
    printf("[%d text, %d data, %d bss] @ 0x%x\n", exec.a_text, exec.a_data,
            exec.a_bss, &exec);

    /* manually craft proc0. it must be minimally configured before calling
       page_init() as it will use acquire/release primitives. (we start with
       process0 holding ALL TOKENS so they don't try to schedule, though.)
       once memory is mapped, we can allocate a proper kernel stack */

    proc_init(0, &proc0);
    proc0.cr3 = proto_pml4;
    proc0.cpu.rip = (long) bsp;
    proc0.tokens = TOKEN_ALL;
    this()->curproc = &proc0;
    page_init();
    proc_kstack(&proc0);
    resume(&proc0);

    /* unreached */
}

/* vi: set ts=4 expandtab: */
