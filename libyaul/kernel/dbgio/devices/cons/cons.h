/*
 * Copyright (c) 2012-2016 Israel Jacquez
 * See LICENSE for details.
 *
 * Israel Jacquez <mrkotfw@gmail.com>
 */

#ifndef _CONS_H_
#define _CONS_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define CONS_COLS_MIN   40
#define CONS_ROWS_MIN   32

#define CONS_COLS_MAX   64
#define CONS_ROWS_MAX   64

#define CONS_TAB_WIDTH  2

typedef struct {
        void (*clear)(void);
        void (*area_clear)(int32_t, int32_t, int32_t, int32_t);
        void (*line_clear)(int32_t, int32_t, int32_t);
        void (*write)(int32_t, int32_t, uint8_t);
} cons_ops_t;

extern void cons_init(const cons_ops_t *, uint16_t, uint16_t);
extern void cons_buffer(const char *);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !_CONS_H_ */
