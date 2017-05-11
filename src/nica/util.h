/*
 * This file is part of libnica.
 *
 * Copyright © 2016-2017 Intel Corporation
 *
 * libnica is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 */

#pragma once

#define _GNU_SOURCE

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <nica/macros.h>

#define DEF_AUTOFREE(N, C)                                                                         \
        static inline void _autofree_func_##N(void *p)                                             \
        {                                                                                          \
                if (p && *(N **)p) {                                                               \
                        C(*(N **)p);                                                               \
                        (*(void **)p) = NULL;                                                      \
                }                                                                                  \
        }

#define autofree(N) __attribute__((cleanup(_autofree_func_##N))) N

/**
 * Dump any leaked file descriptors
 */
_nica_public_ void nc_dump_file_descriptor_leaks(void);

_nica_public_ void *greedy_realloc(void **p, size_t *allocated, size_t need);

/**
 * Test string equality of two null terminated strings
 */
__attribute__((always_inline)) static inline bool streq(const char *a, const char *b)
{
        if (a == b) {
                return true;
        }
        if (!a || !b) {
                return false;
        }
        return strcmp(a, b) == 0;
}

DEF_AUTOFREE(char, free)
DEF_AUTOFREE(FILE, fclose)

/*
 * Editor modelines  -  https://www.wireshark.org/tools/modelines.html
 *
 * Local variables:
 * c-basic-offset: 8
 * tab-width: 8
 * indent-tabs-mode: nil
 * End:
 *
 * vi: set shiftwidth=8 tabstop=8 expandtab:
 * :indentSize=8:tabSize=8:noTabs=true:
 */
