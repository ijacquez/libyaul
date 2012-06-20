/*
 * Copyright (c) 2012 Israel Jacques
 * See LICENSE for details.
 *
 * Israel Jacques <mrko@eecs.berkeley.edu>
 */

#include <cpu/intc.h>
#include <vdp2/tvmd.h>

#include <stdbool.h>

void __attribute__ ((noreturn))
abort(void)
{
        /* Disable interrupts */
        cpu_intc_disable();

        /* This does not execute cleanup functions registered with
         * 'atexit' or 'on_exit' */
        while (true) {
                vdp2_tvmd_vblank_in_wait();
                vdp2_tvmd_vblank_out_wait();
        }
}