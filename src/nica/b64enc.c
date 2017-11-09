/*
 * This file is part of libnica.
 *
 * Copyright © 2017 Intel Corporation
 *
 * libnica is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdint.h>
#include "b64enc.h"

#define B64_LINE_LEN 76

static int padding[] = {0, 2, 1, 0};
static char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


/*
 * base 64 encoding of 3 bytes to 4 printable characters
 * (https://tools.ietf.org/html/rfc4648).
 *
 * i.e. (Reference: https://en.wikipedia.org/wiki/Base64)
 *
 *      [   M  ] [   a  ] [  n  ]
 * bin  01001101 01100001 01101110
 *
 * b64  01001101 01100001 01101110  >> 18 & 0x3F -> 010011 -> ( 19 dec) T
 *      ------
 *
 * b64  01001101 01100001 01101110  >> 12 & 0x3F -> 010110 -> ( 22 dec) W
 *            -------
 *
 * b64  01001101 01100001 01101110  >>  6 & 0x3F -> 000101 -> (  5 dec) F
 *                   -------
 *
 * b64  01001101 01100001 01101110  >>  0 & 0x3F -> 101110 -> (117 dec) u
 *                          ------
 *
 */
static void b64_3b(char bin[3], char *out) {

    uint32_t x = (((uint32_t) bin[0] << 16) & 0xFF0000) |
                 (((uint32_t) bin[1] <<  8) & 0xFF00)   |
                 (((uint32_t) bin[2])       & 0xFF);

    out[3] = table[x       & 0x3F];
    out[2] = table[x >> 6  & 0x3F];
    out[1] = table[x >> 12 & 0x3F];
    out[0] = table[x >> 18 & 0x3F];

}


/*
 * Provide padding to already initialized b64 output
 */
static void b64_padding(char *out, int pad_size) {

    if (pad_size > 0) {
        *out++ = '=';
    }

    if (pad_size > 1) {
        *out++ = '=';
    }

    *out = '\0';
}


/*
 * Encode file contents as b64 string
 */
int nc_b64enc_file(FILE *fh, char *buff, size_t buff_size) {

    int ret_val = 1;
    int out_count = 0;
    int pad_size = 0;
    char *buff_ptr = buff;
    char bin[3] = {0, 0, 0};
    size_t len = 0;

    while ((len = fread(bin, 1, 3, fh)) > 0) {
        b64_3b(bin, buff_ptr);
        /*
         Reset 2nd and 3rd bytes in case only one byte was read. If these
         bytes are not clean b64 output will be corrupted with old bits.
         */
        bin[1] = bin[2] = 0;
        /*
         Advance pointer to the next free position of the b64 encoded
         buffer. This happens to be len + 1 (as longs as len >= 1 and len <= 3)

            len = 1 byte  -> b64 -> 2 bytes (or len + 1)
            len = 2 bytes -> b64 -> 3 bytes (or len + 1)
            len = 3 bytes -> b64 -> 4 bytes (or len + 1)
        */
        buff_ptr += (len + 1);
        /*
         Line break every 76 characters
        */
        out_count += (int) (len + 1);
        if (out_count % B64_LINE_LEN == 0) {
            *buff_ptr++ = '\n';
        }
        pad_size = padding[len];
        /*
         Check not to overflow buffer

            buffer size <= b64 characters + padding length + null termination
        */
        if (buff_size <= (size_t) (buff_ptr - buff) + (size_t) pad_size + 1) {
           ret_val = 0;
           goto end_b64_enc;
        }
    }

    b64_padding(buff_ptr, pad_size);

end_b64_enc:

    return ret_val;
}


int nc_b64enc_filename(const char *filename, char *buff, size_t buff_size) {

    int ret = 0;
    FILE *fh = NULL;

    fh = fopen(filename, "rb");

    if (fh == NULL) {
        goto nc_b64_clean;
    }

    ret = nc_b64enc_file(fh, buff, buff_size);

nc_b64_clean:

    if (fh) {
        fclose(fh);
    }

    return ret;
}

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
