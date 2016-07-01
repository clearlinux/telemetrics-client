/*
 * This program is part of the Clear Linux Project
 *
 * Copyright 2015 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms and conditions of the GNU Lesser General Public License, as
 * published by the Free Software Foundation; either version 2.1 of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "common.h"

/*Record headers to be sent in the following order */
static const char *header_names[] = {
        TM_RECORD_VERSION_STR,
        TM_CLASSIFICATION_STR,
        TM_SEVERITY_STR,
        TM_MACHINE_ID_STR,
        TM_TIMESTAMP_STR,
        TM_ARCH_STR,
        TM_HOST_TYPE_STR,
        TM_SYSTEM_BUILD_STR,
        TM_KERNEL_VERSION_STR,
        TM_PAYLOAD_VERSION_STR,
        TM_SYSTEM_NAME_STR
};

const char *get_header_name(int ind)
{
        assert(ind >= 0 && ind < NUM_HEADERS);
        return header_names[ind];
}

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
