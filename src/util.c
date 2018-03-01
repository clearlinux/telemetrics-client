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

#define _GNU_SOURCE

#define RANDOM_ID_LEN 32

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

#include "common.h"
#include "util.h"

bool get_header(const char *haystack, const char *needle, char **line, size_t len)
{
        if (haystack && !strncmp(haystack, needle, len)) {
                *line = strdup(haystack);
                return true;
        }
        return false;
}

bool get_header_value(const char *header, char **value)
{
        char *sep = NULL;

        *value = NULL;

        if((sep = strchr(header, ':')) != NULL) {
                sep++;
                // skip space after colon if there's one
                if(*sep == ' ') {
                        sep++;
                }
                *value = strdup(sep);
        }

        return (bool)(value != NULL);
}

void *reallocate(void **addr, size_t *allocated, size_t requested)
{
        void *newaddr;
        size_t new_allocated;

        assert(addr);

        if (*allocated >= requested) {
                return *addr;
        }

        new_allocated =  requested * 2;
        newaddr = realloc(*addr, new_allocated);
        if (newaddr == NULL) {
                return NULL;
        }
        *addr = newaddr;
        *allocated = new_allocated;
        return newaddr;
}

long get_directory_size(const char *dir_path)
{
        long total_size = 0;
        DIR *dir;
        struct dirent *de;
        int ret;
        struct stat buf;
        char *file_path = NULL;

        dir = opendir(dir_path);
        if (!dir) {
                ret = -errno;
#ifdef DEBUG
                fprintf(stderr, "ERR: Error opening spool dir: %s\n",
                        strerror(errno));
#endif
                return ret;
        }

        while ((de = readdir(dir)) != NULL) {
                if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) {
                        continue;
                }
                ret = asprintf(&file_path, "%s/%s", dir_path, de->d_name);
                if (ret < 0) {
#ifdef DEBUG
                        fprintf(stderr, "CRIT: Cannot allocate memory\n");
#endif
                        closedir(dir);
                        return -ENOMEM;
                }

                ret = lstat(file_path, &buf);
                if (ret < 0) {
#ifdef DEBUG
                        fprintf(stderr, "ERR: Could not stat file %s: %s\n",
                                file_path, strerror(errno));
#endif
                } else {
                        total_size += (buf.st_blocks * 512);
                }

                free(file_path);
        }
        closedir(dir);

        return total_size;
}

/**
 * Generates a 32 characters random id
 *
 * @param buffer pointer to allocate and copy data
 *
 */
int get_random_id(char **buff)
{
        int result = -1;
        int frandom = -1;
        uint64_t random_id[2] = {0};

        frandom = open("/dev/urandom", O_RDONLY);
        if (frandom < 0) {
                return -1;
        }

        if (read(frandom, &random_id, sizeof(random_id)) == sizeof(random_id)) {
                  if (asprintf(buff, "%.16" PRIx64 "%.16" PRIx64, random_id[0], random_id[1]) == RANDOM_ID_LEN) {
                            result = 0;
                  }
        }

        close(frandom);

        return result;
}

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
