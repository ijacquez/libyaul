/*
 * Copyright (c) 2012-2019 Israel Jacquez
 * See LICENSE for details.
 *
 * Israel Jacquez <mrkotfw@gmail.com>
 */

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include <cpu/cache.h>
#include <cpu/dmac.h>
#include <cpu/intc.h>

#include <vdp.h>

#include <sys/dma-queue.h>

#include <dbgio.h>
#include <internal.h>

#include "../dbgio-internal.h"

#include "cons/cons.h"

#include "vdp2_font.inc"

#define STATE_IDLE                      (0x00)
#define STATE_INITIALIZED               (0x01)
#define STATE_BUFFER_DIRTY              (0x02)
#define STATE_BUFFER_CLEARED            (0x04)
#define STATE_BUFFER_FLUSHING           (0x08)
#define STATE_BUFFER_FORCE_FLUSHING     (0x10)

/* CPU-DMAC channel used for _simple_flush() and _buffer_clear() */
#define DEV_DMAC_CHANNEL 0

struct dev_font_state;

struct dev_state {
        uint8_t state;

        struct dev_font_state *font_state;

        /* Base CPD VRAM address */
        uint32_t cp_table;
        /* Base palette CRAM address */
        uint32_t color_palette;

        /* Base page VRAM address */
        uint32_t page_base;
        uint16_t *page_pnd;
        /* Size of split page */
        uint16_t page_size;
        uint16_t page_width;
        uint16_t page_height;

        uint8_t cram_mode;

        /* PND value */
        uint16_t pnd_value;

        /* PND value for clearing a page */
        uint16_t pnd_value_clear;
};

struct dev_font_state {
        uint8_t *cpd_buffer;
        color_rgb1555_t *pal_buffer;
};

struct dev_font_load_state {
        uint8_t intc_priority_a;

        font_load_callback_t callback;
};

/* Restrictions:
 * 1. Screen will always be displayed
 * 2. Rotational backgrounds are not supported
 * 3. Screen priority is always 7
 * 4. 1x1 plane size is enforced
 * 5. Page 0 of plane A will always be used
 * 6. Resets scroll position to (0, 0) */

static const dbgio_vdp2_t _default_params = {
        .font_cpd = &_font_cpd[0],
        .font_pal = &_font_pal[0],
        .font_fg = 7,
        .font_bg = 0,
        .scroll_screen = VDP2_SCRN_NBG3,
        .cpd_bank = VDP2_VRAM_BANK_B1,
        .cpd_offset = 0x00000,
        .pnd_bank = VDP2_VRAM_BANK_B1,
        .map_index = 2,
        .cpd_cycp = {
                .t0 = VDP2_VRAM_CYCP_PNDR_NBG3,
                .t1 = VDP2_VRAM_CYCP_CPU_RW,
                .t2 = VDP2_VRAM_CYCP_CPU_RW,
                .t3 = VDP2_VRAM_CYCP_CPU_RW,
                .t4 = VDP2_VRAM_CYCP_CHPNDR_NBG3,
                .t5 = VDP2_VRAM_CYCP_CPU_RW,
                .t6 = VDP2_VRAM_CYCP_CPU_RW,
                .t7 = VDP2_VRAM_CYCP_CPU_RW
        },
        .pnd_cycp = {
                .t0 = VDP2_VRAM_CYCP_PNDR_NBG3,
                .t1 = VDP2_VRAM_CYCP_CPU_RW,
                .t2 = VDP2_VRAM_CYCP_CPU_RW,
                .t3 = VDP2_VRAM_CYCP_CPU_RW,
                .t4 = VDP2_VRAM_CYCP_CHPNDR_NBG3,
                .t5 = VDP2_VRAM_CYCP_CPU_RW,
                .t6 = VDP2_VRAM_CYCP_CPU_RW,
                .t7 = VDP2_VRAM_CYCP_CPU_RW
        },
        .cram_index = 0
};

/* Parameters set by the user */
static dbgio_vdp2_t _params;

/* Device state for both async and direct VDP2 devices */
static struct dev_state *_dev_state;

static void _buffer_clear(void);
static void _buffer_area_clear(int16_t, int16_t, int16_t, int16_t);
static void _buffer_line_clear(int16_t);
static void _buffer_line_partial_clear(int16_t, int16_t, int16_t);
static void _buffer_write(int16_t, int16_t, uint8_t);

static const cons_ops_t _cons_ops = {
        .clear = _buffer_clear,
        .area_clear = _buffer_area_clear,
        .line_clear = _buffer_line_clear,
        .line_partial_clear = _buffer_line_partial_clear,
        .write = _buffer_write
};

static inline void __always_inline
_pnd_write(int16_t col, int16_t row, uint16_t value)
{
        int16_t offset;
        offset = col + (row * _dev_state->page_width);

        _dev_state->page_pnd[offset] = value;
}

static inline void __always_inline
_pnd_clear(int16_t col, int16_t row)
{
        _pnd_write(col, row, _dev_state->pnd_value_clear);
}

static void
_pnd_values_update(bool force)
{
        const uint8_t cram_mode = vdp2_cram_mode_get();

        if (!force && (_dev_state->cram_mode != cram_mode)) {
                return;
        }

        _dev_state->cram_mode = cram_mode;

        uint16_t pnd_value;

        switch (cram_mode) {
        case 0:
        case 1:
        default:
                pnd_value = VDP2_SCRN_PND_CONFIG_0(0,
                    _dev_state->cp_table,
                    _dev_state->color_palette,
                    /* vf = */ 0,
                    /* hf = */ 0);
                break;
        case 2:
                pnd_value = VDP2_SCRN_PND_CONFIG_0(2,
                    _dev_state->cp_table,
                    _dev_state->color_palette,
                    /* vf = */ 0,
                    /* hf = */ 0);
                break;
        }

        _dev_state->pnd_value = pnd_value;
        /* PND value used to clear pages */
        _dev_state->pnd_value_clear = pnd_value;

        for (uint32_t i = 0; i < _dev_state->page_size; i++) {
                _dev_state->page_pnd[i] &= ~0xF000;
                _dev_state->page_pnd[i] |= pnd_value;
        }
}

static void
_buffer_clear(void)
{
        static cpu_dmac_cfg_t dmac_cfg = {
                .channel = DEV_DMAC_CHANNEL,
                .src_mode = CPU_DMAC_SOURCE_FIXED,
                .src = 0x00000000,
                .dst = 0x00000000,
                .dst_mode = CPU_DMAC_DESTINATION_INCREMENT,
                .len = 0x00000000,
                .stride = CPU_DMAC_STRIDE_2_BYTES,
                .bus_mode = CPU_DMAC_BUS_MODE_CYCLE_STEAL,
                .ihr = NULL
        };

        /* Don't try to clear the buffer again if it's already been cleared */
        if ((_dev_state->state & STATE_BUFFER_CLEARED) == STATE_BUFFER_CLEARED) {
                return;
        }

        _dev_state->state |= STATE_BUFFER_DIRTY;

        dmac_cfg.dst = (uint32_t)&_dev_state->page_pnd[0];
        dmac_cfg.src = _dev_state->pnd_value_clear;
        dmac_cfg.len = _dev_state->page_size;

        cpu_dmac_channel_wait(DEV_DMAC_CHANNEL);
        cpu_dmac_channel_config_set(&dmac_cfg);
        cpu_dmac_channel_start(DEV_DMAC_CHANNEL);
        cpu_dmac_channel_wait(DEV_DMAC_CHANNEL);

        _dev_state->state |= STATE_BUFFER_CLEARED;
}

static void
_buffer_area_clear(int16_t col_start, int16_t col_end, int16_t row_start,
    int16_t row_end)
{
        _dev_state->state |= STATE_BUFFER_DIRTY;

        int16_t row;
        for (row = row_start; row < row_end; row++) {
                int16_t col;
                for (col = col_start; col < col_end; col++) {
                        _pnd_clear(col, row);
                }
        }
}

static void
_buffer_line_clear(int16_t row)
{
        _dev_state->state |= STATE_BUFFER_DIRTY;

        int16_t col;
        for (col = 0; col < _dev_state->page_width; col++) {
                _pnd_clear(col, row);
        }
}

static void
_buffer_line_partial_clear(int16_t col_start, int16_t col_end, int16_t row)
{
        _dev_state->state |= STATE_BUFFER_DIRTY;

        int16_t col;
        for (col = col_start; col < col_end; col++) {
                _pnd_clear(col, row);
        }
}

static void
_buffer_write(int16_t col, int16_t row, uint8_t ch)
{
        _dev_state->state |= STATE_BUFFER_DIRTY;
        _dev_state->state &= ~STATE_BUFFER_CLEARED;

        /* We can get away with this because of the imposed limitations of the
         * device.
         *
         * Pattern name data must be 1-word, character size must be 1x1, and no
         * character flipping */
        _dev_state->pnd_value &= 0xF000;
        _dev_state->pnd_value |= ch;

        _pnd_write(col, row, _dev_state->pnd_value);
}

static inline uint8_t __always_inline
_1bpp_4bpp_convert(const uint8_t byte, const uint8_t *fgbg)
{
        const uint8_t ubyte = (byte >> 1) & 0x01;
        const uint8_t lbyte = byte & 0x01;

        return (fgbg[lbyte] << 4) | fgbg[ubyte];
}

static void
_font_1bpp_4bpp_decompress(uint8_t *dec_cpd, const uint8_t *cmp_cpd,
    const uint8_t fg, const uint8_t bg)
{
        assert(dec_cpd != NULL);
        assert(cmp_cpd != NULL);
        assert(((uintptr_t)cmp_cpd & 0x00000003) == 0x00000000);

        const uint8_t fgbg[] = {
                bg & 0x0F,
                fg & 0x0F
        };

        uint8_t cpd;

        uint32_t i;
        uint32_t j;
        for (i = 0, j = 0; i < FONT_1BPP_CPD_SIZE; i++) {
                cpd = cmp_cpd[i];

                dec_cpd[j + 0] = _1bpp_4bpp_convert(cpd, fgbg);
                cpd >>= 2;
                dec_cpd[j + 1] = _1bpp_4bpp_convert(cpd, fgbg);
                cpd >>= 2;
                dec_cpd[j + 2] = _1bpp_4bpp_convert(cpd, fgbg);
                cpd >>= 2;
                dec_cpd[j + 3] = _1bpp_4bpp_convert(cpd, fgbg);

                j += 4;
        }
}

static void
_dev_state_init(const dbgio_vdp2_t *params)
{
        assert(params != NULL);

        if (_dev_state == NULL) {
                _dev_state = _internal_malloc(sizeof(struct dev_state));
                assert(_dev_state != NULL);

                (void)memset(_dev_state, 0x00, sizeof(struct dev_state));

                _dev_state->font_state =
                    _internal_malloc(sizeof(struct dev_font_state));
                assert(_dev_state->font_state != NULL);

                (void)memset(_dev_state->font_state, 0x00,
                    sizeof(struct dev_font_state));

                _dev_state->state = STATE_IDLE;
        }

        /* Return if we're already initialized */
        if ((_dev_state->state & STATE_INITIALIZED) != 0x00) {
                return;
        }

        _dev_state->page_size = VDP2_SCRN_CALCULATE_PAGE_SIZE_M(1 * 1, 1);
        _dev_state->page_width = VDP2_SCRN_CALCULATE_PAGE_WIDTH_M(1 * 1);
        _dev_state->page_height = VDP2_SCRN_CALCULATE_PAGE_HEIGHT_M(1 * 1);

        /* One page per plane */
        _dev_state->page_base = VDP2_VRAM_ADDR(params->pnd_bank,
            params->map_index * _dev_state->page_size);

        _dev_state->cp_table = VDP2_VRAM_ADDR(params->cpd_bank, params->cpd_offset);
        _dev_state->color_palette = VDP2_CRAM_ADDR(params->cram_index << 3);

        /* Restricting the page to 64x32 avoids wasting space */
        _dev_state->page_size >>= 1;

        _pnd_values_update(/* force = */ true);

        if (_dev_state->page_pnd == NULL) {
                _dev_state->page_pnd = _internal_malloc(_dev_state->page_size);
                assert(_dev_state->page_pnd != NULL);
        }

        struct dev_font_state *font_state;
        font_state = _dev_state->font_state;

        if (font_state->cpd_buffer == NULL) {
                font_state->cpd_buffer = _internal_malloc(FONT_4BPP_CPD_SIZE);
                assert(font_state->cpd_buffer != NULL);
        }

        if (font_state->pal_buffer == NULL) {
                font_state->pal_buffer =
                    _internal_malloc(FONT_4BPP_COLOR_COUNT * sizeof(color_rgb1555_t));
                assert(font_state->pal_buffer != NULL);
        }
}

static inline void __always_inline
_scroll_screen_init(const dbgio_vdp2_t *params)
{
        assert(params != NULL);
        assert(_dev_state != NULL);

        const vdp2_scrn_cell_format_t cell_format = {
                .scroll_screen = params->scroll_screen,
                .cc_count = VDP2_SCRN_CCC_PALETTE_16,
                .character_size = 1 * 1,
                .pnd_size = 1, /* 1-word */
                .auxiliary_mode = 0,
                .cp_table = _dev_state->cp_table,
                .color_palette = _dev_state->color_palette,
                .plane_size = 1 * 1,
                .map_bases.plane_a = _dev_state->page_base,
                .map_bases.plane_b = _dev_state->page_base,
                .map_bases.plane_c = _dev_state->page_base,
                .map_bases.plane_d = _dev_state->page_base
        };

        vdp2_scrn_cell_format_set(&cell_format);
}

static void
_scroll_screen_reset(void)
{
        /* Force reset */
        _pnd_values_update(/* force = */ false);

        vdp2_scrn_priority_set(_params.scroll_screen, 7);
        vdp2_scrn_scroll_x_set(_params.scroll_screen, FIX16(0.0f));
        vdp2_scrn_scroll_y_set(_params.scroll_screen, FIX16(0.0f));
        vdp2_scrn_display_set(_params.scroll_screen, /* transparent = */ true);

        vdp2_vram_cycp_bank_set(_params.cpd_bank, &_params.cpd_cycp);
        vdp2_vram_cycp_bank_set(_params.pnd_bank, &_params.pnd_cycp);
}

static inline void __always_inline
_assert_shared_init(const dbgio_vdp2_t *params __unused)
{
        assert(params != NULL);

        assert(params->font_cpd != NULL);
        assert(params->font_pal != NULL);

        assert(params->font_bg <= 15);
        assert(params->font_bg <= 15);

        assert((params->scroll_screen == VDP2_SCRN_NBG0) ||
               (params->scroll_screen == VDP2_SCRN_NBG1) ||
               (params->scroll_screen == VDP2_SCRN_NBG2) ||
               (params->scroll_screen == VDP2_SCRN_NBG3));

        assert((params->scroll_screen != VDP2_SCRN_RBG0) &&
               (params->scroll_screen != VDP2_SCRN_RBG1));

        assert(params->cpd_bank <= 3);
        /* XXX: Fetch the VRAM bank split configuration and determine the VRAM
         *      bank size */
        assert(params->cpd_offset < VDP2_VRAM_BSIZE_4);

        assert(params->pnd_bank <= 3);
        /* XXX: Determine the page size and check against the number of
         *      available offsets */

        /* There are 128 16-color banks, depending on CRAM mode */
        /* XXX: Fetch CRAM mode and check number of available 16-color banks */
        assert(params->cram_index < 128);
}

static void
_shared_init(const dbgio_vdp2_t *params)
{
        _assert_shared_init(params);

        _dev_state_init(params);

        /* Return if we're already initialized */
        if ((_dev_state->state & STATE_INITIALIZED) != 0x00) {
                return;
        }

        cons_init(&_cons_ops, CONS_COLS_MIN, CONS_ROWS_MIN);

        /* Copy user's set device parameters */
        (void)memcpy(&_params, params, sizeof(dbgio_vdp2_t));

        _scroll_screen_init(params);

        struct dev_font_state *font_state;
        font_state = _dev_state->font_state;

        _font_1bpp_4bpp_decompress(font_state->cpd_buffer,
            params->font_cpd,
            params->font_fg,
            params->font_bg);

        (void)memcpy(font_state->pal_buffer, params->font_pal,
            FONT_4BPP_COLOR_COUNT * sizeof(color_rgb1555_t));
}

static void
_shared_deinit(void)
{
        if (_dev_state == NULL) {
                return;
        }

        if ((_dev_state->state & STATE_INITIALIZED) == 0x00) {
                return;
        }

        _internal_free(_dev_state->page_pnd);

        _internal_free(_dev_state->font_state->cpd_buffer);
        _internal_free(_dev_state->font_state->pal_buffer);
        _internal_free(_dev_state->font_state);

        _internal_free(_dev_state);

        _dev_state = NULL;
}

static void
_shared_puts(const char *buffer)
{
        if (_dev_state == NULL) {
                return;
        }

        if ((_dev_state->state & STATE_INITIALIZED) == 0x00) {
                return;
        }

        /* It's the best we can do for now. If the current buffer is marked for
         * flushing, we have to silently drop any calls to write to the
         * buffer */
        uint8_t state_mask;
        state_mask = STATE_BUFFER_FLUSHING | STATE_BUFFER_FORCE_FLUSHING;

        if ((_dev_state->state & state_mask) == STATE_BUFFER_FLUSHING) {
                return;
        }

        cons_buffer(buffer);
}

static void
_shared_font_load_cpd_callback(void *work)
{
        const struct dev_font_load_state *font_load_state = work;

        if (font_load_state->callback != NULL) {
                font_load_state->callback();
        }

        cpu_intc_priority_a_set(font_load_state->intc_priority_a);
}

static void
_shared_font_load(font_load_callback_t callback)
{
        static cpu_dmac_cfg_t dmac_cfg = {
                .channel = DEV_DMAC_CHANNEL,
                .src_mode = CPU_DMAC_SOURCE_INCREMENT,
                .src = 0x00000000,
                .dst = 0x00000000,
                .dst_mode = CPU_DMAC_DESTINATION_INCREMENT,
                .len = 0x00000000,
                .stride = CPU_DMAC_STRIDE_2_BYTES,
                .bus_mode = CPU_DMAC_BUS_MODE_CYCLE_STEAL
        };

        static struct dev_font_load_state font_load_state;

        if (_dev_state == NULL) {
                return;
        }

        if ((_dev_state->state & STATE_INITIALIZED) == 0x00) {
                return;
        }

        const uint8_t intc_mask __unused = cpu_intc_mask_get();

        /* Be sure to have interrupts enabled */
        assert(intc_mask < 15);

        struct dev_font_state *font_state;
        font_state = _dev_state->font_state;

        /* Font palette (32 bytes) is small enough that we'd spend more time
         * setting up a CPU-DMAC transfer, than by just transferring the palette
         * directly */
        (void)memcpy((void *)_dev_state->color_palette,
            font_state->pal_buffer,
            FONT_4BPP_COLOR_COUNT * sizeof(color_rgb1555_t));

        dmac_cfg.len = FONT_4BPP_CPD_SIZE;
        dmac_cfg.dst = (uint32_t)_dev_state->cp_table;
        dmac_cfg.src = CPU_CACHE_THROUGH | (uint32_t)font_state->cpd_buffer;
        dmac_cfg.ihr = _shared_font_load_cpd_callback;
        dmac_cfg.ihr_work = &font_load_state;

        cpu_dmac_channel_wait(DEV_DMAC_CHANNEL);
        cpu_dmac_channel_config_set(&dmac_cfg);

        cpu_dmac_enable();

        font_load_state.callback = callback;
        font_load_state.intc_priority_a = cpu_intc_priority_a_get();

        cpu_dmac_interrupt_priority_set(15);

        /* Due to the 1BPP font being decompressed in cached H-WRAM, we need to
         * flush the cache as the DMA transfer accesses the uncached mirror
         * address to the decompressed 4BPP font, which could result in fetching
         * stale values not yet written back to H-WRAM */
        cpu_cache_purge();

        cpu_dmac_channel_start(DEV_DMAC_CHANNEL);
}

#include "vdp2-async.inc"
#include "vdp2-simple.inc"
