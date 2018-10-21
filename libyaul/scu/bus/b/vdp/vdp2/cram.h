/*
 * Copyright (c) 2012-2016 Israel Jacquez
 * See LICENSE for details.
 *
 * Israel Jacquez <mrkotfw@gmail.com>
 */

#ifndef _VDP2_CRAM_H_
#define _VDP2_CRAM_H_

#include <sys/cdefs.h>

/* Color RAM (CRAM) stores color data of sprites and scroll screens.
 *
 * R/W access from the CPU or DMA controller(s) is possible, but the
 * image may be disturbed by the access timing. Access through CPU or
 * DMA controller(s) is possible only in word units and long word
 * units. Access in bytes is not allowed. */

#define CRAM_ADDR(x)                    (0x25F00000 + ((x) << 1))

#define CRAM_MODE_0_OFFSET(x, y, z)     (0x25F00000 + ((x) << 9) +             \
    ((y) << 5) + ((z) << 1))
#define CRAM_MODE_1_OFFSET(x, y, z)     CRAM_MODE_0_OFFSET(x, y, z)
#define CRAM_MODE_2_OFFSET(x, y, z)     (0x25F00000 + ((x) << 10) +            \
    ((y) << 6) + ((z) << 2))

/*
 * CRAM Modes
 *
 * Mode 0: RGB 555 1,024 colors mirrored
 * Mode 1: RGB 555 2,048 colors
 * Mode 2: RGB 888 1,024 colors
 */

#define CRAM_MODE_0_SIZE        0x0800
#define CRAM_MODE_1_SIZE        0x1000
#define CRAM_MODE_2_SIZE        0x1000

#define CRAM_SIZE               0x1000

#define COLOR_PALETTE_DATA      0x0000
#define COLOR_RGB_DATA          0x8000

#define COLOR_RGB888(r, g, b)   (((b) << 16) | ((g) << 8) | (r))
#define COLOR_RGB888_TO_RGB555(r, g, b) ((((b) >> 3) << 10) |                  \
    (((g) >> 3) << 5) | ((r) >> 3))

#endif /* !_VDP2_CRAM_H_ */
