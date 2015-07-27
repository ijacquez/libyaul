/*
 * Copyright (c) 2013, 2014
 * See LICENSE for details.
 *
 * Israel Jacquez <mrkotfw@gmail.com>
 * David Oberhollenzer
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "tga.h"

#define BGR15_DATA_TO_RGB555         BGR16_DATA_TO_RGB555
#define BGR16_DATA_TO_RGB555(c)                                                \
        (0x8000 | (((*(uint16_t *)(c)) & 0x001F) << 10) |                      \
        ((*(uint16_t *)(c)) & 0x03E0) |                                        \
        (((*(uint16_t *)(c)) & 0x7C00) >> 10))
#define BGR24_DATA_TO_RGB555(c)                                                \
        (0x8000 | (((c)[0] >> 3) << 10) | (((c)[1] >> 3) << 5) | ((c)[2] >> 3))
#define BGRA32_DATA_TO_RGB555        BGR24_DATA_TO_RGB555

#define RGB24_TO_RGB555(r, g, b) (0x8000 | (((b) >> 3) << 10) |                \
    (((g) >> 3) << 5) | ((r) >> 3))

#define READ_LITTLE_ENDIAN_16(a, i)                                            \
        (((uint32_t)(a)[(i)]) | ((uint32_t)(a)[(i) + 1]) << 8)

#define TGA_HEADER_LEN  18
#define TGA_FOOTER_LEN  26

static uint32_t cmap_image_decode_tiled(uint8_t *, const tga_t *);
static uint32_t cmap_image_decode(uint8_t *, const tga_t *);

static uint32_t true_color_image_decode(uint16_t *, const tga_t *);
static uint32_t true_color_rle_image_decode(uint16_t *, const tga_t *);

static void inline _cmap_image_tile_draw(uint8_t *, uint16_t, uint16_t,
    const tga_t *);
static int32_t _true_color_image_fill(uint16_t *, uint16_t, size_t);
static inline uint32_t _image_calculate_offset(const tga_t *);
static inline uint32_t _cmap_calculate_offset(const tga_t *);

/*
 *
 */
int
tga_read(tga_t *tga, const uint8_t *file)
{
#define TGA_IMAGE_ORIGIN_BOTTOM 0x00
#define TGA_IMAGE_ORIGIN_TOP    0x20
#define TGA_IMAGE_ORIGIN_LEFT   0x00
#define TGA_IMAGE_ORIGIN_RIGHT  0x10
#define TGA_IMAGE_ORIGIN_MASK   0x30

        const uint8_t *header;
        const uint8_t *image_buf;

        uint8_t id_len;
        uint8_t image_type;
        uint8_t bpp;
        uint8_t bytes_pp;
        uint16_t width;
        uint16_t height;
        uint8_t origin;

        uint8_t have_cmap;
        uint8_t cmap_bpp;
        uint16_t cmap_offset;
        uint16_t cmap_len;
        uint32_t cmap_bytes;

        /* Read the TGA header */
        header = &file[0];
        id_len = header[0];

        image_type = header[2];
        bpp = header[16];
        bytes_pp = header[16] >> 3;
        width = READ_LITTLE_ENDIAN_16(header, 12);
        height = READ_LITTLE_ENDIAN_16(header, 14);
        origin = header[17] & TGA_IMAGE_ORIGIN_MASK;

        have_cmap = header[1];
        cmap_offset = READ_LITTLE_ENDIAN_16(header, 3);
        cmap_len = READ_LITTLE_ENDIAN_16(header, 5);
        cmap_bpp = header[7];
        cmap_bytes = cmap_len * (cmap_bpp >> 3);

        memset(tga, 0x00, sizeof(tga_t));

        /* Sanity checks */
        /* The maximum size of a TGA image is 512 pixels wide by 482
         * pixels high. */
        if ((width == 0) || (height == 0) || (width > 512) || (height > 482)) {
                return TGA_FILE_CORRUPTED;
        }

        /* Only support the origin being at the top-left */
        if ((origin & TGA_IMAGE_ORIGIN_MASK) !=
            (TGA_IMAGE_ORIGIN_TOP | TGA_IMAGE_ORIGIN_LEFT)) {
                return TGA_FILE_NOT_SUPPORTED;
        }

        switch (image_type) {
        case TGA_IMAGE_TYPE_GRAYSCALE:
        case TGA_IMAGE_TYPE_RLE_GRAYSCALE:
                if ((bytes_pp != 1) || have_cmap) {
                        return TGA_FILE_CORRUPTED;
                }
                break;
        case TGA_IMAGE_TYPE_TRUE_COLOR:
        case TGA_IMAGE_TYPE_RLE_TRUE_COLOR:
                if (((bytes_pp != 2) && (bytes_pp != 3) && (bytes_pp != 4)) ||
                    have_cmap) {
                        return TGA_FILE_CORRUPTED;
                }
                break;
        case TGA_IMAGE_TYPE_CMAP:
        case TGA_IMAGE_TYPE_RLE_CMAP:
                if (!have_cmap || (cmap_offset >= cmap_len)) {
                        return TGA_FILE_CORRUPTED;
                }

                switch (cmap_bpp) {
                case 15:
                case 16:
                case 24:
                case 32:
                        break;
                default:
                        return TGA_FILE_CORRUPTED;
                }
                break;
        case TGA_IMAGE_TYPE_NONE:
        default:
                return TGA_FILE_CORRUPTED;
        }

        switch (image_type) {
        case TGA_IMAGE_TYPE_GRAYSCALE:
        case TGA_IMAGE_TYPE_RLE_GRAYSCALE:
                return TGA_FILE_NOT_SUPPORTED;
        case TGA_IMAGE_TYPE_CMAP:
                if (!have_cmap) {
                        return TGA_FILE_CORRUPTED;
                }
                break;
        case TGA_IMAGE_TYPE_TRUE_COLOR:
        case TGA_IMAGE_TYPE_RLE_TRUE_COLOR:
                break;
        case TGA_IMAGE_TYPE_RLE_CMAP:
                return TGA_FILE_NOT_SUPPORTED;
        }

        tga->tga_file = header;

        tga->tga_type = image_type;

        tga->tga_bpp = bpp;
        tga->tga_width = width;
        tga->tga_height = height;

        tga->tga_cmap_bpp = cmap_bpp;
        tga->tga_cmap_len = cmap_len;
        tga->tga_cmap_bytes = cmap_bytes;

        return TGA_FILE_OK;
}

uint32_t
tga_image_decode_tiled(const tga_t *tga, void *dst)
{
        /* XXX: Check if tga is valid */
        const uint8_t *image_buf;
        image_buf = (const uint8_t *)((uint32_t)tga->tga_file +
            _image_calculate_offset(tga));

        switch (tga->tga_type) {
        case TGA_IMAGE_TYPE_GRAYSCALE:
        case TGA_IMAGE_TYPE_RLE_GRAYSCALE:
                return TGA_FILE_NOT_SUPPORTED;
        case TGA_IMAGE_TYPE_CMAP:
                return cmap_image_decode_tiled(dst, tga);
        case TGA_IMAGE_TYPE_TRUE_COLOR:
                return TGA_FILE_NOT_SUPPORTED;
        case TGA_IMAGE_TYPE_RLE_TRUE_COLOR:
                return TGA_FILE_NOT_SUPPORTED;
        case TGA_IMAGE_TYPE_RLE_CMAP:
                return TGA_FILE_NOT_SUPPORTED;
        default:
                return TGA_FILE_CORRUPTED;
        }
}

uint32_t
tga_image_decode(const tga_t *tga, void *dst)
{
        /* XXX: Check if tga is valid */
        const uint8_t *image_buf;
        image_buf = (const uint8_t *)((uint32_t)tga->tga_file +
            _image_calculate_offset(tga));

        switch (tga->tga_type) {
        case TGA_IMAGE_TYPE_GRAYSCALE:
        case TGA_IMAGE_TYPE_RLE_GRAYSCALE:
                return TGA_FILE_NOT_SUPPORTED;
        case TGA_IMAGE_TYPE_CMAP:
                return cmap_image_decode(dst, tga);
        case TGA_IMAGE_TYPE_TRUE_COLOR:
                return true_color_image_decode(dst, tga);
        case TGA_IMAGE_TYPE_RLE_TRUE_COLOR:
                return true_color_rle_image_decode(dst, tga);
        case TGA_IMAGE_TYPE_RLE_CMAP:
                return TGA_FILE_NOT_SUPPORTED;
        default:
                return TGA_FILE_CORRUPTED;
        }
}

uint32_t
tga_cmap_decode(const tga_t *tga, uint16_t *dst)
{
        if ((tga->tga_type != TGA_IMAGE_TYPE_CMAP) &&
            (tga->tga_type != TGA_IMAGE_TYPE_RLE_CMAP)) {
                return 0;
        }

        uint16_t cmap_idx;

        const uint8_t *cmap_buf;
        cmap_buf = (const uint8_t *)((uint32_t)tga->tga_file +
            _cmap_calculate_offset(tga));

        for (cmap_idx = 0; cmap_idx < tga->tga_cmap_len; cmap_idx++) {
                switch (tga->tga_cmap_bpp >> 3) {
                case 2:
                        dst[cmap_idx] =
                            BGR16_DATA_TO_RGB555(&cmap_buf[cmap_idx * 2]);
                        break;
                case 3:
                        dst[cmap_idx] =
                            BGR24_DATA_TO_RGB555(&cmap_buf[cmap_idx * 3]);
                        break;
                case 4:
                        dst[cmap_idx] =
                            BGRA32_DATA_TO_RGB555(&cmap_buf[cmap_idx * 4]);
                        break;
                }
        }

        return cmap_idx;
}

const char *
tga_error_stringify(int error)
{
        switch (error) {
        case TGA_FILE_OK:
                return "File OK";
        case TGA_FILE_UNKNOWN_FORMAT:
                return "File unknown format";
        case TGA_FILE_CORRUPTED:
                return "File corrupted";
        case TGA_FILE_NOT_SUPPORTED:
                return "File not supported";
        case TGA_MEMORY_ERROR:
                return "Memory error";
        default:
                return "Unknown error";
        }
}

static uint32_t
cmap_image_decode_tiled(uint8_t *dst, const tga_t *tga)
{
        uint16_t tx;
        uint16_t ty;

        if ((tga->tga_cmap_len - 1) <= 16) {
                return 0;
        } else {
                for (tx = 0; tx < (tga->tga_width / 8); tx++) {
                        for (ty = 0; ty < (tga->tga_height / 8); ty++) {
                                _cmap_image_tile_draw(dst, tx, ty, tga);
                        }
                }
        }

        return 1;
}

static uint32_t
cmap_image_decode(uint8_t *dst, const tga_t *tga)
{
        uint32_t pixel_idx;

        const uint8_t *buf;
        buf = (const uint8_t *)((uint32_t)tga->tga_file +
            _image_calculate_offset(tga));

        uint32_t pixels;
        pixels = tga->tga_width * tga->tga_height;

        if ((tga->tga_cmap_len - 1) <= 16) {
                for (pixel_idx = 0; pixel_idx < pixels; pixel_idx += 2) {
                        *dst++ = ((buf[pixel_idx] & 0x0F) << 4) |
                            (buf[pixel_idx + 1] & 0x0F);
                }
        } else {
                for (pixel_idx = 0; pixel_idx < pixels; pixel_idx++) {
                        *dst++ = buf[pixel_idx];
                }
        }

        return pixel_idx;
}

/*
 *
 */
static uint32_t
true_color_image_decode(uint16_t *dst, const tga_t *tga)
{
        uint32_t pixel_idx;

        const uint8_t *buf;
        buf = (const uint8_t *)((uint32_t)tga->tga_file +
            _image_calculate_offset(tga));

        uint16_t bytes_pp;
        bytes_pp = tga->tga_bpp >> 3;

        uint32_t pixels;
        pixels = tga->tga_width * tga->tga_height;

        for (pixel_idx = 0; pixel_idx < pixels; pixel_idx++) {
                const uint8_t *pixel_data;
                pixel_data = &buf[bytes_pp * pixel_idx];

                uint16_t pixel;
                switch (bytes_pp) {
                case 2:
                        pixel = BGR16_DATA_TO_RGB555(pixel_data);
                        break;
                case 3:
                        pixel = BGR24_DATA_TO_RGB555(pixel_data);
                        break;
                case 4:
                        pixel = BGRA32_DATA_TO_RGB555(pixel_data);
                        break;
                }

                if (pixel == RGB24_TO_RGB555(255, 0, 255)) {
                        pixel &= ~0x8000;
                }

                *dst++ = pixel;
        }

        return pixel_idx;
}

/*
 *
 */
static uint32_t
true_color_rle_image_decode(uint16_t *dst, const tga_t *tga)
{
#define TGA_PACKET_TYPE_RLE 1
#define TGA_PACKET_TYPE_RAW 0

        const uint8_t *packet;

        int pixel_idx;

        const uint8_t *buf;
        buf = (const uint8_t *)((uint32_t)tga->tga_file +
            _image_calculate_offset(tga));

        uint16_t bytes_pp;
        bytes_pp = tga->tga_bpp >> 3;

        for (pixel_idx = 0, packet = buf; pixel_idx < (tga->tga_width * tga->tga_height); ) {
                uint8_t packet_type;
                uint8_t rcf;
                uint32_t rcf_idx;

                packet_type = (packet[0] & 0x80) >> 7;
                /* Determine how many pixels are in this packet */
                rcf = (packet[0] & ~0x80) + 1;

                const uint8_t *pixel_data;
                uint16_t pixel;
                int32_t amt;

                switch (packet_type) {
                case TGA_PACKET_TYPE_RAW:
                        /* Slow copy */
                        for (rcf_idx = 0; rcf_idx < rcf; rcf_idx++) {
                                pixel_data = &packet[1 + (bytes_pp * rcf_idx)];
                                switch (bytes_pp) {
                                case 2:
                                        pixel = BGR16_DATA_TO_RGB555(
                                                pixel_data);
                                        break;
                                case 3:
                                        pixel = BGR24_DATA_TO_RGB555(
                                                pixel_data);
                                        break;
                                case 4:
                                        pixel = BGRA32_DATA_TO_RGB555(
                                                pixel_data);
                                        break;
                                }

                                if (pixel == RGB24_TO_RGB555(255, 0, 255)) {
                                        pixel &= ~0x8000;
                                }

                                *dst++ = pixel;
                        }

                        packet = (uint8_t *)(packet + (rcf * bytes_pp) + 1);
                        break;
                case TGA_PACKET_TYPE_RLE:
                        pixel_data = &packet[1];

                        switch (bytes_pp) {
                        case 2:
                                pixel = BGR16_DATA_TO_RGB555(pixel_data);
                                break;
                        case 3:
                                pixel = BGR24_DATA_TO_RGB555(pixel_data);
                                break;
                        case 4:
                                pixel = BGRA32_DATA_TO_RGB555(pixel_data);
                                break;
                        }

                        if (pixel == RGB24_TO_RGB555(255, 0, 255)) {
                                pixel &= ~0x8000;
                        }

                        amt = _true_color_image_fill(dst, pixel,
                            rcf * sizeof(uint16_t));
                        /* Memory address is not on a 2-byte or 4-byte
                         * boundary */
                        assert(amt >= 0);
                        dst += rcf;

                        packet = (uint8_t *)(packet + bytes_pp + 1);
                        break;
                }

                pixel_idx += rcf;
        }

        return pixel_idx;
}

static inline void
_cmap_image_tile_draw(uint8_t *dst, uint16_t tx, uint16_t ty, const tga_t *tga)
{
        const uint8_t *buf;
        buf = (const uint8_t *)((uint32_t)tga->tga_file +
            _image_calculate_offset(tga));

        uint32_t x;
        uint32_t y;

        uint16_t tile_pixels;
        tile_pixels = 8 * 8;

        uint16_t tile_base;
        tile_base = tile_pixels * (tx + ((tga->tga_width / 8) * ty));

        for (y = 0; y < 8; y++) {
                for (x = 0; x < 8; x++) {
                        uint32_t x_offset;
                        x_offset = (8 * tx) + x;
                        uint32_t y_offset;
                        y_offset = ((8 * ty) + y) * tga->tga_width;
                        uint32_t offset;
                        offset = x_offset + y_offset;

                        dst[tile_base + (x + (8 * y))] = buf[offset];
                }
        }
}

static inline int32_t
_true_color_image_fill(uint16_t *dst, uint16_t s, size_t length)
{
        /* Check if address is unaligned */
        if (((uintptr_t)dst & 0x00000001) != 0) {
                return -1;
        }

        /* Check if the length is even */
        if ((length & 0x00000001) != 0) {
                return -1;
        }

        uint32_t amt;

        amt = length;

        /* If we're on a 2-byte boundary, write a single value */
        if (((uintptr_t)dst & 0x00000002) == 0x00000002) {
                *dst++ = s;
                amt -= sizeof(uint16_t);
        }

        uint32_t *dst_32;
        uint32_t value;

        dst_32 = (uint32_t *)dst;
        value = (s << 16) | s;
        while (amt >= (4 * sizeof(uint32_t))) {
                *dst_32++ = value;
                *dst_32++ = value;
                *dst_32++ = value;
                *dst_32++ = value;

                amt -= 4 * sizeof(uint32_t);
        }

        while (amt >= sizeof(uint32_t)) {
                *dst_32++ = value;
                amt -= sizeof(uint32_t);
        }

        /* Transfer the remainder */
        dst = (uint16_t *)dst_32;
        if (amt != 0) {
                *dst++ = s;
                amt -= sizeof(uint16_t);
        }

        assert(amt == 0);

        return length;
}

static inline uint32_t
_image_calculate_offset(const tga_t *tga)
{
        const uint8_t *header;
        header = &tga->tga_file[0];
        uint8_t id_len;
        id_len = header[0];

        return TGA_HEADER_LEN + id_len + tga->tga_cmap_bytes;
}

static inline uint32_t
_cmap_calculate_offset(const tga_t *tga)
{
        const uint8_t *header;
        header = &tga->tga_file[0];
        uint8_t id_len;
        id_len = header[0];

        return TGA_HEADER_LEN + id_len;
}
