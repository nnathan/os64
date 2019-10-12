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

/* the BSP re-starts here properly situated on a kernel stack as proc0 */

static
bsp()
{
    acpi_init();
    panic("finished");
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

    /* manually craft proc0. it must be minimally configured before
       calling page_init() as it will use some scheduling primitives.
       once memory is mapped, we can allocate a proper kernel stack */

    proc_init(&proc0);
    proc0.cr3 = proto_pml4;
    proc0.rip = (long) bsp;
    this()->curproc = &proc0;
    page_init();
    proc_kstack(&proc0);
    resume(&proc0);

    /* unreached */
}

/* vi: set ts=4 expandtab: */
