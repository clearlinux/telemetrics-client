/*
 * This program is part of the Clear Linux Project
 *
 * Copyright 2018 Intel Corporation
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

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "log.h"
#include "common.h"

int delete_record_by_id(char *record_id)
{
        int ret = 0;
        char *record_path = NULL;

        ret = asprintf(&record_path, "%s/%s", RECORD_RETENTION_DIR, record_id);
        if (ret == -1) {
                return 1;
        }
        ret = unlink(record_path);
        if (ret == -1) {
                telem_perror("Error deleting saved record");
        }
        free(record_path);

        return 0;
}

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
