/*
 * Copyright (c) 2012-2016 Israel Jacquez
 * See LICENSE for details.
 *
 * Israel Jacquez <mrkotfw@gmail.com>
 */

#include <vdp2/scrn.h>

#include <assert.h>

#include "vdp-internal.h"

void
vdp2_scrn_priority_set(uint8_t scrn, uint8_t priority)
{
#ifdef DEBUG
        assert((scrn == SCRN_NBG0) ||
               (scrn == SCRN_NBG1) ||
               (scrn == SCRN_NBG2) ||
               (scrn == SCRN_NBG3) ||
               (scrn == SCRN_RBG0) ||
               (scrn == SCRN_RBG1));
#endif /* DEBUG */

        /* When priority is zero, scroll screen is transparent. */
        priority &= 0x07;

        switch (scrn) {
        case SCRN_RBG1:
        case SCRN_NBG0:
                _state_vdp2()->regs.prina &= 0xFFF8;
                _state_vdp2()->regs.prina |= priority;
                break;
        case SCRN_NBG1:
                _state_vdp2()->regs.prina &= 0xF8FF;
                _state_vdp2()->regs.prina |= priority << 8;
                break;
        case SCRN_NBG2:
                _state_vdp2()->regs.prinb &= 0xFFF8;
                _state_vdp2()->regs.prinb |= priority;
                break;
        case SCRN_NBG3:
                _state_vdp2()->regs.prinb &= 0xF8FF;
                _state_vdp2()->regs.prinb |= priority << 8;
                break;
        case SCRN_RBG0:
                _state_vdp2()->regs.prir &= 0xFFF8;
                _state_vdp2()->regs.prir |= priority;
                break;
        default:
                return;
        }
}
