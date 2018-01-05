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

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include "util.h"

bool get_header(const char *haystack, const char *needle, char **line, size_t len)
{
        if (haystack && !strncmp(haystack, needle, len)) {
                *line = strdup(haystack);
                return true;
        }
        return false;
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



unsigned long long int rdmsr_on_cpu(int reg, int cpu)
{
	unsigned long long int data = 0;
	int fd;
	char msr_file_name[64];

	sprintf(msr_file_name, "/dev/cpu/%d/msr", cpu);
	fd = open(msr_file_name, O_RDONLY);
	if (fd < 0) {
		if (errno == ENXIO) {
#ifdef DEBUG
			fprintf(stderr, "rdmsr: No CPU %d\n", cpu);
#endif
		} else if (errno == EIO) {
#ifdef DEBUG
			fprintf(stderr, "rdmsr: CPU %d doesn't support MSRs\n",
				cpu);
#endif
		} else {
#ifdef DEBUG
			perror("rdmsr: open");
#endif
		}
	}

	if (pread(fd, &data, sizeof data, reg) != sizeof data) {
		if (errno == EIO) {
#ifdef DEBUG
			fprintf(stderr, "rdmsr: CPU %d cannot read "
				"MSR 0x%08x\n",
				cpu, reg);
#endif
		} else {
#ifdef DEBUG
			perror("rdmsr: pread");
#endif
		}
		data = 0;
	}
	close(fd);
	return data;
}

int ppin_capable(void)
{
	unsigned long long int data;
	data = rdmsr_on_cpu(206, 0);
	data >>= 23;
	data &= (1ULL << 1) - 1;
	return (int)data;
}

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
