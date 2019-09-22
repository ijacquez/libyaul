/*
 * Copyright (c) 2012-2019
 * See LICENSE for details.
 *
 * Israel Jacquez <mrkotfw@gmail.com
 */

#include <sys/cdefs.h>

#include <stdio.h>

#include <cpu/intc.h>
#include <cpu/map.h>
#include <cpu/frt.h>

static void _frt_oci_handler(void);
static void _frt_ovi_handler(void);

static void _default_ihr(void);

#define FRT_IHR_INDEX_NONE      0
#define FRT_IHR_INDEX_OCBI      1
#define FRT_IHR_INDEX_OCAI      2

static void (*_frt_ovi_ihr)(void) = _default_ihr;

static void (*_frt_oc_ihr_table[])(void) = {
        _default_ihr,
        _default_ihr,
        _default_ihr
};

void
cpu_frt_init(bool slave, uint8_t clock_div)
{
        MEMORY_WRITE_AND(8, CPU(TIER), ~0x8E);
        MEMORY_WRITE_AND(8, CPU(FTCSR), ~0x8F);

        MEMORY_WRITE(16, CPU(VCRC), (CPU_INTC_INTERRUPT_FRT_ICI << 8) | CPU_INTC_INTERRUPT_FRT_OCI);
        MEMORY_WRITE(16, CPU(VCRD), CPU_INTC_INTERRUPT_FRT_OVI << 8);

        cpu_frt_interrupt_priority_set(15);

        /* Set internal clock (divisor) */
        MEMORY_WRITE_AND(8, CPU(TCR), ~0x83);
        MEMORY_WRITE_OR(8, CPU(TCR), clock_div & 0x03);

        if (!slave) {
                cpu_frt_oca_clear();
                cpu_frt_ocb_clear();
                cpu_frt_ovi_clear();

                cpu_intc_ihr_set(CPU_INTC_INTERRUPT_FRT_OCI, _frt_oci_handler);
                cpu_intc_ihr_set(CPU_INTC_INTERRUPT_FRT_OVI, _frt_ovi_handler);
        } else {
                cpu_intc_ihr_set(CPU_INTC_INTERRUPT_FRT_OCI + 0x100, _frt_oci_handler);
                cpu_intc_ihr_set(CPU_INTC_INTERRUPT_FRT_OVI + 0x100, _frt_ovi_handler);

        }

        cpu_frt_count_set(0);
}

void
cpu_frt_oca_set(uint16_t count, void (*ihr)(void))
{
        /* Disable interrupt */
        MEMORY_WRITE_AND(8, CPU(TIER), ~0x08);
        MEMORY_WRITE_AND(8, CPU(FTCSR), ~0x08);

        /* Select OCRA register and select output compare A match */
        MEMORY_WRITE_AND(8, CPU(TOCR), ~0x12);
        MEMORY_WRITE(8, CPU(OCRAH), 0);
        MEMORY_WRITE(8, CPU(OCRAL), 0);

        _frt_oc_ihr_table[FRT_IHR_INDEX_OCAI] = _default_ihr;

        if ((count > 0) && (ihr != NULL)) {
                MEMORY_WRITE_AND(8, CPU(TOCR), ~0x10);
                MEMORY_WRITE(8, CPU(OCRAH), (uint8_t)(count >> 8));
                MEMORY_WRITE(8, CPU(OCRAL), (uint8_t)(count & 0xFF));

                /* Compare on match A */
                MEMORY_WRITE_OR(8, CPU(TOCR), 0x02);
                MEMORY_WRITE_OR(8, CPU(TIER), 0x08);

                _frt_oc_ihr_table[FRT_IHR_INDEX_OCAI] = ihr;
        }
}

void
cpu_frt_ocb_set(uint16_t count, void (*ihr)(void))
{
        /* Disable interrupt */
        MEMORY_WRITE_AND(8, CPU(TIER), ~0x04);
        MEMORY_WRITE_AND(8, CPU(FTCSR), ~0x04);

        /* Select output compare B match */
        MEMORY_WRITE_AND(8, CPU(TOCR), ~0x01);

        /* Select OCRB register */
        MEMORY_WRITE_OR(8, CPU(TOCR), 0x10);
        MEMORY_WRITE(8, CPU(OCRBH), 0);
        MEMORY_WRITE(8, CPU(OCRBL), 0);

        _frt_oc_ihr_table[FRT_IHR_INDEX_OCBI] = _default_ihr;

        if ((count > 0) && (ihr != NULL)) {
                MEMORY_WRITE(8, CPU(OCRBH), (uint8_t)(count >> 8));
                MEMORY_WRITE(8, CPU(OCRBL), (uint8_t)(count & 0xFF));

                MEMORY_WRITE_OR(8, CPU(TOCR), 0x01);
                MEMORY_WRITE_OR(8, CPU(TIER), 0x04);

                _frt_oc_ihr_table[FRT_IHR_INDEX_OCBI] = ihr;
        }
}

void
cpu_frt_ovi_set(void (*ihr)(void))
{
        volatile uint8_t *reg_tier;
        reg_tier = (volatile uint8_t *)CPU(TIER);

        *reg_tier &= ~0x02;

        MEMORY_WRITE_AND(8, CPU(FTCSR), ~0x02);

        _frt_ovi_ihr = _default_ihr;

        if (ihr != NULL) {
                _frt_ovi_ihr = ihr;

                *reg_tier |= 0x02;
        }
}

static void __interrupt_handler
_frt_oci_handler(void)
{
        volatile uint8_t *reg_tier;
        reg_tier = (volatile uint8_t *)CPU(TIER);

        volatile uint8_t *reg_ftcsr;
        reg_ftcsr = (volatile uint8_t *)CPU(FTCSR);

        uint8_t ftcsr_bits;
        ftcsr_bits = *reg_ftcsr;

        /* Disable OCA or OCB interrupt (or neither), invoke the
         * callback and enable interrupt again */

        uint32_t ocf_bits;
        ocf_bits = ftcsr_bits & 0x0C;

        *reg_tier &= ~ocf_bits;
        *reg_ftcsr = ftcsr_bits & ~0x0C;

        _frt_oc_ihr_table[(ocf_bits & 0x08) >> 2]();
        _frt_oc_ihr_table[(ocf_bits & 0x04) >> 2]();

        *reg_tier |= ocf_bits;
}

static void __interrupt_handler
_frt_ovi_handler(void)
{
        volatile uint8_t *reg_tier;
        reg_tier = (volatile uint8_t *)CPU(TIER);

        *reg_tier &= ~0x02;

        MEMORY_WRITE_AND(8, CPU(FTCSR), ~0x02);

        _frt_ovi_ihr();

        *reg_tier |= 0x02;
}

static void
_default_ihr(void)
{
}
