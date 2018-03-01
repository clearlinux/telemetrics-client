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

#pragma once

#include <stdbool.h>

/* Increase memory allocated */
void *reallocate(void **addr, size_t *allocated, size_t requested);

/* Check if haystacks begins with needle and return a copy of haystack */
bool get_header(const char *haystack, const char *needle, char **line, size_t len);

/* Get the value of a header */
bool get_header_value(const char *header, char **value);

/* Get the size of the directory */
long get_directory_size(const char *sdir);

/* Initialize buff and copy generated id */
int get_random_id(char **buff);

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
