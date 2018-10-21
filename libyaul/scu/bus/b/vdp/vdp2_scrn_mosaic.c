/*
 * Copyright (c) 2012-2016 Israel Jacquez
 * See LICENSE for details.
 *
 * Israel Jacquez <mrkotfw@gmail.com>
 */

#include <vdp2/scrn.h>

#include "vdp-internal.h"

void
vdp2_scrn_mosaic_set(uint8_t scrn)
{
#ifdef DEBUG
        assert((scrn == SCRN_NBG0) ||
               (scrn == SCRN_NBG1) ||
               (scrn == SCRN_NBG2) ||
               (scrn == SCRN_NBG3) ||
               (scrn == SCRN_RBG0));
#endif /* DEBUG */

        /* If performing mosaic processing in NBG0 or NBG1, the vertical
         * cell-scroll function cannot be used.
         *
         * Mosaic processing is then ignored for NBG0 or NBG1. */

        switch (scrn) {
        case SCRN_RBG1:
        case SCRN_NBG0:
#ifdef DEBUG
                /* Make sure that the vertical cell scroll function is
                 * not being used */
                assert((_state_vdp2()->regs.scrctl & 0x0001) == 0x0000);
#endif /* DEBUG */

                _state_vdp2()->regs.mzctl &= 0xFFFE;
                _state_vdp2()->regs.mzctl |= 0x0001;
                break;
        case SCRN_NBG1:
#ifdef DEBUG
                /* Make sure that the vertical cell scroll function is
                 * not being used */
                assert((_state_vdp2()->regs.scrctl & 0x0100) == 0x0000);
#endif /* DEBUG */

                _state_vdp2()->regs.mzctl &= 0xFFFD;
                _state_vdp2()->regs.mzctl |= 0x0002;
                break;
        case SCRN_NBG2:
                _state_vdp2()->regs.mzctl &= 0xFFFB;
                _state_vdp2()->regs.mzctl |= 0x0004;
                break;
        case SCRN_NBG3:
                _state_vdp2()->regs.mzctl &= 0xFFF7;
                _state_vdp2()->regs.mzctl |= 0x0008;
                break;
        case SCRN_RBG0:
                _state_vdp2()->regs.mzctl &= 0xFFEF;
                _state_vdp2()->regs.mzctl |= 0x0010;
                break;
        }
}

void
vdp2_scrn_mosaic_unset(uint8_t scrn)
{
#ifdef DEBUG
        assert((scrn == SCRN_NBG0) ||
               (scrn == SCRN_NBG1) ||
               (scrn == SCRN_NBG2) ||
               (scrn == SCRN_NBG3) ||
               (scrn == SCRN_RBG0));
#endif /* DEBUG */

        switch (scrn) {
        case SCRN_RBG1:
        case SCRN_NBG0:
                _state_vdp2()->regs.mzctl &= 0xFFFE;
                break;
        case SCRN_NBG1:
                _state_vdp2()->regs.mzctl &= 0xFFFD;
                break;
        case SCRN_NBG2:
                _state_vdp2()->regs.mzctl &= 0xFFFB;
                break;
        case SCRN_NBG3:
                _state_vdp2()->regs.mzctl &= 0xFFF7;
                break;
        case SCRN_RBG0:
                _state_vdp2()->regs.mzctl &= 0xFFEF;
                break;
        }
}

void
vdp2_scrn_mosaic_clear(void)
{
        _state_vdp2()->regs.mzctl &= 0xFFE0;
}

void
vdp2_scrn_mosaic_vertical_set(uint8_t vertical)
{
        if ((vertical < 1) || (vertical > 16)) {
                return;
        }

        _state_vdp2()->regs.mzctl &= 0x0FFF;
        _state_vdp2()->regs.mzctl |= (vertical - 1) << 12;
}

void
vdp2_scrn_mosaic_horizontal_set(uint8_t horizontal)
{
        if ((horizontal < 1) || (horizontal > 16)) {
                return;
        }

        _state_vdp2()->regs.mzctl &= 0xF0FF;
        _state_vdp2()->regs.mzctl |= (horizontal - 1) << 8;
}
