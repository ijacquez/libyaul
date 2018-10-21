/*
 * Copyright (c) 2012-2016 Israel Jacquez
 * See LICENSE for details.
 *
 * Israel Jacquez <mrkotfw@gmail.com>
 */

#ifndef _DRAM_CART_MAP_H_
#define _DRAM_CART_MAP_H_

#include <scu/map.h>

/* DRAM cart registers */
#define ID              0x04FFFFFF
#define UNKNOWN         0x007EFFFE

/* Specific macros */
#define DRAM(t, b, x)   (CS0((x) + ((((b) & 0x03) +                            \
        (((t) & 0x01) << 2)) << 19)))

#endif /* _DRAM_CART_MAP_H_ */
