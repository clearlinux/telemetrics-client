/*
 * This file is part of libnica.
 *
 * Copyright © 2017 Intel Corporation
 *
 * libnica is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This is a chained base64 encode implementation.
 */

#pragma once

#define _GNU_SOURCE

#include "macros.h"

/*
 * Encodes contents of file handler in base64
 *
 * @param fh File handler to encode as base64
 * @param buff Output buffer where the base64 output should be written
 * @param buff_size Size of the output buffer
 *
 * @returns 1 in success and 0 in failure
 */
_nica_public_ int nc_b64enc_file(FILE *fh, char *buff, size_t buff_size);


/*
 * Encodes contents of file (filename) in base64
 *
 * @param filename File to encode as base64
 * @param buff Output buffer where the base64 output should be written
 * @param buff_size Size of the output buffer
 *
 * @returns 1 in success and 0 in failure
 */
_nica_public_ int nc_b64enc_filename(const char *filename, char *buff, size_t buff_size);

