/*
 * Copyright (c) 2012-2019 Israel Jacquez
 * See LICENSE for details.
 *
 * Israel Jacquez <mrkotfw@gmail.com>
 */

#ifndef _SCU_DMA_H_
#define _SCU_DMA_H_

#include <sys/cdefs.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <scu/map.h>
#include <scu/ic.h>

__BEGIN_DECLS

/*-
 * SCU DMA should be activated by the Master CPU
 *
 * - Keep in mind that:
 *     1. DMA transfers does not begin until it is explictily started
 *     2. DMA transfer level 2 will block the CPU during operation of level 1
 *
 * - SCU DMA is for transfers between different buses:
 *     1. Work RAM-H <-> A-bus
 *     2. Work RAM-H <-> B-bus
 *     3. A-bus <-> B-bus
 *
 * - A-bus write prohibited
 * - VDP2 area read prohibited
 * - WORKRAM-L usage from SCU disabled
 *
 * - Restriction on write address addition value in DMA based on access
 *   address
 *
 *   1. WORKRAM-H                        -> 0x02 can be set
 *   2. External areas 1 to 3            -> 0x02 can be set
 *   3. External area 4 (A-bus I/O area) -> 0x00 and 0x02 can be set
 *   4. VDP1, VDP2, SCSP                 -> all values can be set
 */

#define SCU_DMA_LEVEL_COUNT     3

#define SCU_DMA_MODE_DIRECT     0x00
#define SCU_DMA_MODE_INDIRECT   0x01

#define SCU_DMA_START_FACTOR_VBLANK_IN          0x00
#define SCU_DMA_START_FACTOR_VBLANK_OUT         0x01
#define SCU_DMA_START_FACTOR_HBLANK_IN          0x02
#define SCU_DMA_START_FACTOR_TIMER_0            0x03
#define SCU_DMA_START_FACTOR_TIMER_1            0x04
#define SCU_DMA_START_FACTOR_SOUND_REQ          0x05
#define SCU_DMA_START_FACTOR_SPRITE_DRAW_END    0x06
#define SCU_DMA_START_FACTOR_ENABLE             0x07

#define SCU_DMA_STRIDE_0_BYTES          0x00
#define SCU_DMA_STRIDE_2_BYTES          0x01
#define SCU_DMA_STRIDE_4_BYTES          0x02
#define SCU_DMA_STRIDE_8_BYTES          0x03
#define SCU_DMA_STRIDE_16_BYTES         0x04
#define SCU_DMA_STRIDE_32_BYTES         0x05
#define SCU_DMA_STRIDE_64_BYTES         0x06
#define SCU_DMA_STRIDE_128_BYTES        0x07

#define SCU_DMA_UPDATE_NONE     (0x00000000UL)
#define SCU_DMA_UPDATE_RUP      (0x00010000UL)
#define SCU_DMA_UPDATE_WUP      (0x00000100UL)

#define SCU_DMA_INDIRECT_TABLE_END      (0x80000000UL)

#define SCU_DMA_BUS_A   0x00
#define SCU_DMA_BUS_B   0x01
#define SCU_DMA_BUS_DSP 0x02

#define SCU_DMA_MODE_XFER_INITIALIZER(_len, _dst, _src) {                      \
        .len = (_len),                                                         \
        .dst = (uint32_t)(_dst),                                               \
        .src = (uint32_t)(_src)                                                \
}

#define SCU_DMA_MODE_XFER_END_INITIALIZER(_len, _dst, _src) {                  \
        .len = _len,                                                           \
        .dst = (uint32_t)(_dst),                                               \
        .src = SCU_DMA_INDIRECT_TABLE_END | (uint32_t)(_src)                   \
}

typedef struct scu_dma_handle {
        uint32_t dnr;
        uint32_t dnw;
        uint32_t dnc;
        uint32_t dnad;
        uint32_t dnmd;
} __packed __aligned(4) scu_dma_handle_t;

typedef struct scu_dma_xfer {
        uint32_t len;
        uint32_t dst;
        uint32_t src;
} __packed __aligned(4) scu_dma_xfer_t;

typedef struct scu_dma_level_cfg {
        uint8_t mode;

        union {
                /* Indirect mode */
                scu_dma_xfer_t *indirect;

                /* Direct mode */
                scu_dma_xfer_t direct;
        } xfer;

        uint8_t stride;
        uint32_t update;
} scu_dma_level_cfg_t;

typedef void (*scu_dma_ihr)(void);
typedef void (*scu_dma_callback)(void *);

void scu_dma_level0_fast_start(void);
void scu_dma_level1_fast_start(void);
void scu_dma_level2_fast_start(void);

void scu_dma_level0_start(void);
void scu_dma_level1_start(void);
void scu_dma_level2_start(void);

void scu_dma_level0_stop(void);
void scu_dma_level1_stop(void);
void scu_dma_level2_stop(void);

extern void scu_dma_level0_wait(void);
extern void scu_dma_level1_wait(void);
extern void scu_dma_level2_wait(void);

extern void scu_dma_level0_end_set(scu_dma_callback, void *);
extern void scu_dma_level1_end_set(scu_dma_callback, void *);
extern void scu_dma_level2_end_set(scu_dma_callback, void *);

static inline uint32_t __always_inline
scu_dma_status_get(void)
{
        return MEMORY_READ(32, SCU(DSTA));
}

static inline uint32_t __always_inline
scu_dma_dsp_busy(void)
{
        /* In operation, on standby, or interrupted to background */
        return (MEMORY_READ(32, SCU(DSTA)) & 0x00010003);
}

static inline void __always_inline
scu_dma_dsp_wait(void)
{
        while ((scu_dma_dsp_busy()) != 0x00000000);
}

static inline uint8_t __always_inline
scu_dma_bus_access_busy(void)
{
        return (MEMORY_READ(32, SCU(DSTA)) >> 20) & 0x03;
}

static inline void __always_inline
scu_dma_bus_access_wait(const uint8_t bus_mask)
{
        while (((scu_dma_bus_access_busy()) & bus_mask) != 0x00);
}

static inline uint32_t __always_inline
scu_dma_level0_busy(void)
{
        /* In operation, on standby, or interrupted */
        return (MEMORY_READ(32, SCU(DSTA)) & 0x00010030);
}

static inline uint32_t __always_inline
scu_dma_level1_busy(void)
{
        /* In operation, on standby, or interrupted */
        return (MEMORY_READ(32, SCU(DSTA)) & 0x00020300);
}

static inline uint32_t __always_inline
scu_dma_level2_busy(void)
{
        /* In operation, on standby, or interrupted */
        return (MEMORY_READ(32, SCU(DSTA)) & 0x00003000);
}

static inline uint32_t __always_inline
scu_dma_level_busy(const uint8_t level)
{
        switch (level & 0x03) {
        case 1:
                return scu_dma_level1_busy();
        case 2:
                return scu_dma_level2_busy();
        case 0:
        default:
                return scu_dma_level0_busy();
        }
}

static inline void __always_inline
scu_dma_level_wait(const uint8_t level)
{
        switch (level & 0x03) {
        case 0:
                scu_dma_level0_wait();
                break;
        case 1:
                scu_dma_level1_wait();
                break;
        case 2:
                scu_dma_level2_wait();
                break;
        }
}

static inline void __always_inline
scu_dma_level_fast_start(const uint8_t level)
{
        switch (level & 0x03) {
        case 0:
                scu_dma_level0_fast_start();
                break;
        case 1:
                scu_dma_level1_fast_start();
                break;
        case 2:
                scu_dma_level2_fast_start();
                break;
        }
}

static inline void __always_inline
scu_dma_level_start(const uint8_t level)
{
        switch (level & 0x03) {
        case 0:
                scu_dma_level0_start();
                break;
        case 1:
                scu_dma_level1_start();
                break;
        case 2:
                scu_dma_level2_start();
                break;
        }
}

static inline void __always_inline
scu_dma_level_stop(const uint8_t level)
{
        switch (level & 0x03) {
        case 0:
                scu_dma_level0_stop();
                break;
        case 1:
                scu_dma_level1_stop();
                break;
        case 2:
                scu_dma_level2_stop();
                break;
        }
}

static inline void __always_inline
scu_dma_stop(void)
{
        scu_dma_level0_stop();
        scu_dma_level1_stop();
        scu_dma_level2_stop();
}

static inline void __always_inline
scu_dma_level_end_set(const uint8_t level, scu_dma_callback callback, void *work)
{
        switch (level & 0x03) {
        case 2:
                scu_dma_level2_end_set(callback, work);
                break;
        case 1:
                scu_dma_level1_end_set(callback, work);
                break;
        case 0:
        default:
                scu_dma_level0_end_set(callback, work);
                break;
        }
}

static inline void __always_inline
scu_dma_illegal_set(scu_dma_ihr ihr)
{
        scu_ic_ihr_set(SCU_IC_INTERRUPT_DMA_ILLEGAL, ihr);
}

extern void scu_dma_config_buffer(scu_dma_handle_t *,
    const scu_dma_level_cfg_t *);
extern void scu_dma_config_set(uint8_t, uint8_t,
    const scu_dma_handle_t *, scu_dma_callback);
extern int8_t scu_dma_level_unused_get(void);

__END_DECLS

#endif /* !_SCU_DMA_H_ */
