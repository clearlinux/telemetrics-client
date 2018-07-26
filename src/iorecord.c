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

void stage_record(char *filepath, char *headers[], char *body)
{
        int tmpfd;
        FILE *tmpfile = NULL;

        // Use default path if not provided
        if (filepath == NULL) {
                telem_log(LOG_ERR, "filepath value must be provided, aborting\n");
                exit(EXIT_FAILURE);
        }

        tmpfd = mkstemp(filepath);
        if (!tmpfd) {
                telem_perror("Error opening staging file");
                close(tmpfd);
                if (unlink(filepath)) {
                        telem_perror("Error deleting staging file");
                }
                goto clean_exit;
        }

        // open file
        tmpfile = fdopen(tmpfd, "a");
        if (!tmpfile) {
                telem_perror("Error opening temp stage file");
                close(tmpfd);
                if (unlink(filepath)) {
                        telem_perror("Error deleting temp stage file");
                }
                goto clean_exit;
        }

        // write headers
        for (int i = 0; i < NUM_HEADERS; i++) {
                fprintf(tmpfile, "%s\n", headers[i]);
        }

        //write body
        fprintf(tmpfile, "%s\n", body);
        fflush(tmpfile);
        fclose(tmpfile);

clean_exit:

        return;
}

bool read_record(char *fullpath, char *headers[], char **body)
{
        int i, ret = 0;
        bool result = false;
        FILE *fp = NULL;
        long offset;
        char line[LINE_MAX] = { 0 };
        struct stat buf;
        long size;

        ret = stat(fullpath, &buf);
        if (ret == -1) {
                telem_log(LOG_ERR, "Unable to stat record %s in staging\n", fullpath);
                return false;
        }
        size = buf.st_size;

        fp = fopen(fullpath, "r");
        if (fp == NULL) {
                telem_log(LOG_ERR, "Unable to open file %s in staging\n", fullpath);
                return false;
        }

        for (i = 0; i < NUM_HEADERS; i++) {
                const char *header_name = get_header_name(i);
                if (!_fgets(line, LINE_MAX, fp)) {
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
