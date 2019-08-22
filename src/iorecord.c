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
#include <limits.h>
#include <stdbool.h>
#include <sys/stat.h>

#include "log.h"
#include "util.h"
#include "common.h"
#include "iorecord.h"

bool _fgets(char *s, int n, FILE *stream)
{
        char *i;

        if (!fgets(s, n, stream)) {
                return false;
        }
        if ((i = strchr(s, '\n')) != NULL) {
                *i = '\0';
        }

        return true;
}

bool read_record(char *fullpath, char *headers[], char **body, char **cfg_file)
{
        int i = 0;
        bool result = false;
        FILE *fp = NULL;
        long offset;
#if (LINE_MAX > PATH_MAX)
        char line[LINE_MAX+1] = { 0 };
#else
        char line[PATH_MAX+1] = { 0 };
#endif
        long size;
        uint32_t cfg_prefix = 0;

        fp = fopen(fullpath, "r");
        if (fp == NULL) {
                telem_log(LOG_ERR, "Unable to open file %s in staging\n", fullpath);
                return false;
        }

        // Get the file size
        fseek(fp, 0 , SEEK_END);
        size = ftell(fp);
        fseek(fp, 0 , SEEK_SET);

        // First line may contain configuration file path
        if (fread(&cfg_prefix, CFG_PREFIX_LENGTH, 1, fp) != 1) {
                telem_log(LOG_ERR, "Error while parsing staged record configuration info.\n");
                goto read_error;
        }

        if (cfg_prefix == CFG_PREFIX_32BIT) {
                if (!_fgets(line, sizeof(line), fp)) {
                        telem_log(LOG_ERR, "Error while parsing staged record [%x]\n", cfg_prefix);
                        goto read_error;
                }

                size_t pathlen = strlen(line);
                *cfg_file = malloc(pathlen + 1);
                if (*cfg_file == NULL) {
                        telem_log(LOG_ERR, "Could not allocate memory for config file path\n");
                        goto read_error;
                }
                strcpy (*cfg_file, line);
                telem_debug("DEBUG: cfg_file specified: %s\n", *cfg_file);
        } else {
                telem_debug("DEBUG: no user cfg file specified, cfg_prefix: %08x\n", cfg_prefix);
                *cfg_file = NULL;
                rewind(fp);
        }

        for (i = 0; i < NUM_HEADERS; i++) {
                const char *header_name = get_header_name(i);
                if (!_fgets(line, sizeof(line), fp)) {
                        telem_log(LOG_ERR, "Error while parsing staged record\n");
                        fclose(fp);
                        return false;
                }
                //Get rid of trailing newline
                result = get_header(line, header_name, &headers[i]);
                if (!result) {
                        telem_log(LOG_ERR, "read_record: Incorrect"
                                  " headers in record\n");
                        goto read_error;
                }
        }

        offset = ftell(fp);
        if (offset == -1) {
                telem_perror("Failed to get staged file offset of record payload");
                result = false;
                goto read_error;
        }

        size = size - offset + 1;
        *body = malloc((size_t)size);
        if (*body == NULL) {
                telem_log(LOG_ERR, "Could not allocate memory for payload from staged file\n");
                result = false;
                goto read_error;
        }
        memset(*body, 0, (size_t)size);

        //read rest of file
        size_t newlen = fread(*body, sizeof(char), (size_t)size, fp);
        if (newlen == 0) {
                telem_perror("Error reading staged file");
                result = false;
                goto read_error;
        } else {
                result = true;
        }

read_error:
        if (fp) {
                fclose(fp);
        }

        return result;
}
