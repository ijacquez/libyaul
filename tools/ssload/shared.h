/*
 * Copyright (c) 2012-2016 Israel Jacquez
 * See LICENSE for details.
 *
 * Israel Jacquez <mrkotfw@gmail.com>
 */

#ifndef SHARED_H_
#define SHARED_H_

#include <inttypes.h>

#ifdef HAVE_LIBFTD2XX
#include <ftd2xx.h>
#else
#include <ftdi.h>
#endif /* HAVE_LIBFTD2XX */

/* Compatibility with Linux and FreeBSD */
#ifndef __unused
#define __unused __attribute__ ((unused))
#endif /* !__unused */

/* XXX: Remove manual check until upgrading from libftdi1-1.4 to libftdi1-1.5 */
#ifndef ftdi_tcioflush
#define ftdi_tcioflush(ctx) ftdi_usb_purge_buffers((ctx))
#endif /* !ftdi_tcioflush */

#define ADDRESS_MSB(x)  ((uint8_t)((x) >> 24) & 0xFF)
#define ADDRESS_02(x)   ((uint8_t)((x) >> 16) & 0xFF)
#define ADDRESS_01(x)   ((uint8_t)((x) >> 8) & 0xFF)
#define ADDRESS_LSB(x)  ((uint8_t)(x) & 0xFF)

#define LEN_MSB(x)      (ADDRESS_MSB(x))
#define LEN_02(x)       (ADDRESS_02(x))
#define LEN_01(x)       (ADDRESS_01(x))
#define LEN_LSB(x)      (ADDRESS_LSB(x))

#define TO_LE(x)        ((ADDRESS_LSB(x) << 24) |                              \
                         (ADDRESS_02(x) << 16) |                               \
                         (ADDRESS_01(x) << 8) |                                \
                         ADDRESS_MSB(x))               

#define TO_BE(x)        ((ADDRESS_LSB(x) << 24) |                              \
                         (ADDRESS_02(x) << 16) |                               \
                         (ADDRESS_01(x) << 8) |                                \
                         ADDRESS_MSB(x))               

#ifdef HAVE_LIBFTD2XX
#define MAX_ENUMERATE_DEVICES 16

extern FT_HANDLE ft_handle;
extern FT_STATUS ft_error;
extern const char *ft_error_strings[];

const char **enumerate_devices(void);
#else
extern struct ftdi_context ftdi_ctx;
extern int ftdi_error;
#endif /* HAVE_LIBFTD2XX */

int verbose_printf(const char *, ...);

#ifdef DEBUG
void debug_hexdump(const uint8_t *, uint32_t);
#endif /* DEBUG */

#endif /* !SHARED_H_ */
