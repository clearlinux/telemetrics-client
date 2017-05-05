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

#include <stdlib.h>
#include <stdio.h>
#include <sys/klog.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

#include "log.h"
#include "oops_parser.h"
#include "klog_scanner.h"

#define MAX_OOPS_MSG_LENGTH 1024
#define FILE_PATH KERNELOOPSDIR "/kerneloopsXXXXXX"

static int fd;
char filename[] = FILE_PATH;
int last_line_length;

bool oops_written = false;
void split_buf_by_line(char *bufp, int bytes)
{
        char *start;
        char *eol;
        size_t linelength = 0;

        while (bytes > 0) {
                eol = memchr(bufp, '\n', (size_t)bytes);
                if (!eol) {
                        eol = bufp + bytes;
                }

                start = bufp;
                linelength = (size_t)(eol - bufp);

                bytes -= (int)linelength + 1;
                bufp = eol + 1;

                if (bytes >= 0) {
                        start[linelength] = '\0';
                        parse_single_line(start, linelength);
                        linelength = 0;
                }
        }
}

int allocate_filespace()
{
        int allocate_disk = 0;
        strcpy(filename, FILE_PATH);

        fd = mkstemp(filename);
        if (fchmod(fd, 0644) == -1) {
                telem_log(LOG_ERR, "Error while changing permissions for file %s: %s\n",
                          filename, strerror(errno));
        }

        if (fd == -1) {
                telem_log(LOG_ERR, "fd error: %s\n", strerror(errno));
                return -1;
        }

        // Allocates disk space to write kernel oops to
        allocate_disk = fallocate(fd, 0, 0, MAX_OOPS_MSG_LENGTH);
        if (allocate_disk != 0) {
                telem_log(LOG_ERR, "Error allocating disk space: %s\n", strerror(errno));
                return -1;
        }
        return 0;
}

void write_oops_to_file(struct oops_log_msg *msg)
{
        ssize_t writer;
        size_t linelength;
        char *line;

        for (int i = 0; i < msg->length; i++) {
                //Add the newline character to the end of each line
                line = msg->lines[i];
                linelength = strlen(line);
                line[linelength] = '\n';
                linelength++;

                // Write each line of oops to file
                writer = write(fd, line, linelength);
                if (writer == -1) {
                        telem_log(LOG_ERR, "Error writing buffer to file: %s\n", strerror(errno));
                        exit(EXIT_FAILURE);
                }
        }

        telem_log(LOG_INFO, "Oops written to file\n");
        oops_written = true;
        close(fd);

        // Allocate more space to prepare for another oops
        if (allocate_filespace() < 0) {
                telem_log(LOG_ERR, "%s\n", strerror(errno));
                exit(EXIT_FAILURE);
        }
}

bool get_oops_written(void)
{
        return oops_written;
}

char *get_file_name(void)
{
        return filename;
}

__attribute__((destructor))
void clean_up(void)
{
        // Unlink empty file and close any open file
        unlink(filename);
        close(fd);
}

void signal_handler(int signum)
{
        exit(EXIT_FAILURE);
}

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
