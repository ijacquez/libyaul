/*
 * Copyright (c) 2012-2019 Israel Jacquez
 * See LICENSE for details.
 *
 * Israel Jacquez <mrkotfw@gmail.com>
 */

#include <assert.h>
#include <stdio.h>

#include <dbgio.h>

#include "../dbgio-internal.h"

static void _nop(void);

const struct dbgio_dev_ops _internal_dev_ops_vdp1 = {
        .dev            = DBGIO_DEV_VDP1,
        .default_params = NULL,
        .init           = (void (*)(const void *))_nop,
        .deinit         = _nop,
        .puts           = (void (*)(const char *))_nop,
        .flush          = _nop
};

static void
_nop(void)
{
}
