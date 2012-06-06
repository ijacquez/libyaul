/*
 * Copyright (c) 2012 Israel Jacques
 * See LICENSE for details.
 *
 * Israel Jacques <mrko@eecs.berkeley.edu>
 */

#ifndef _FONT_H_
#define _FONT_H_

#include <inttypes.h>

#define FONT_H          8
#define FONT_W          8

#define FONT_NCOLORS    16

extern const uint32_t font_unpacked[];
extern const uint32_t font[];

extern const uint16_t palette[];

#endif /* !_FONT_H_ */