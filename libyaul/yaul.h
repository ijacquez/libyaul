/*
 * Copyright (c) 2012-2016 Israel Jacquez
 * See LICENSE for details.
 *
 * Israel Jacquez <mrkotfw@gmail.com>
 */

#ifndef _LIBYAUL_H_
#define _LIBYAUL_H_

/* Check if HAVE_DEV_CARTRIDGE has a valid value */
/* USB flash cartridge */
/* Datel Action Replay cartridge */
#if (HAVE_DEV_CARTRIDGE < 0) || (HAVE_DEV_CARTRIDGE > 2)
#error "Invalid `HAVE_DEV_CARTRIDGE' value"
#endif /* HAVE_DEV_CARTRIDGE */

#include <bios.h>

/* CPU-bus CPU */
#include <scu/bus/cpu/cpu.h>

/* CPU-bus SMPC */
#include <scu/bus/cpu/smpc/smpc.h>

/* SCU */
#include <scu/scu.h>

/* CS0 */
#if HAVE_DEV_CARTRIDGE == 1 /* USB flash cartridge */
#include <usb-cart.h>
#elif HAVE_DEV_CARTRIDGE == 2 /* Datel Action Replay cartridge */
#include <arp.h>
#endif /* HAVE_DEV_CARTRIDGE */

#include <scu/bus/a/cs0/dram-cartridge/dram-cartridge.h>

/* CS2 */
#include <scu/bus/a/cs2/cd-block/cd-block.h>

/* B-bus VDP1 and VDP2 */
#include <scu/bus/b/vdp/vdp.h>

/* B-bus SCSP */
#include <scu/bus/b/scsp/scsp.h>

/* Kernel */
#include <common/ip.h>

#include <common/gdb/gdb.h>

#include <dbgio/dbgio.h>

#include <math.h>

#include <mm/memb.h>
#include <mm/slob.h>

#include <sys/init.h>
#include <sys/dma-queue.h>
#include <sys/irq-mux.h>

#include <fs/iso9660/iso9660.h>
#include <fs/romdisk/romdisk.h>

#endif /* !_LIBYAUL_H_ */
