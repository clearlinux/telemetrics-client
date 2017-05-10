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
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/inotify.h>
#include <poll.h>

#include "oops_parser.h"
#include "log.h"
#include "telemetry.h"

#include "nica/nc-string.h"

char *oops_dir_path = KERNELOOPSDIR;

static uint32_t version = 1;

static bool send_data(char *backtrace, char *class, uint32_t severity)
{
        struct telem_ref *handle = NULL;
        int ret;

        if ((ret = tm_create_record(&handle, severity, class, version)) < 0) {
                telem_log(LOG_ERR, "Failed to create record: %s",
                          strerror(-ret));
                return false;
        }

        if ((ret = tm_set_payload(handle, backtrace)) < 0) {
                telem_log(LOG_ERR, "Failed to set payload: %s", strerror(-ret));
                tm_free_record(handle);
                return false;
        }

        if ((ret = tm_send_record(handle)) < 0) {
                telem_log(LOG_ERR, "Failed to send record: %s", strerror(-ret));
                tm_free_record(handle);
                return false;
        }

        tm_free_record(handle);

        return true;
}

void handle_oops_file(const char *fname)
{
        int ret;
        FILE *fp = NULL;
        char *filename = NULL, *contents = NULL;
        long sz;
        size_t size, bytes_read;
        nc_string *payload;

        ret = asprintf(&filename, "%s/%s", oops_dir_path, fname);
        if (ret == -1) {
                exit(EXIT_FAILURE);
        }

        #ifdef DEBUG
        printf("File to be handled: %s\n", filename);
        #endif

        fp = fopen(filename, "r");
        if (fp == NULL) {
                telem_log(LOG_ERR, "Failed to open oops log file %s: %s", fname, strerror(errno));
                goto file_end;
        }

        if (fseek(fp, 0, SEEK_END) == -1) {
                telem_log(LOG_ERR, "Failed to seek file %s: %s\n", fname, strerror(errno));
                goto file_end;
        }

        sz = ftell(fp);
        if (sz == -1L) {
                telem_perror("ftell failed");
                goto file_end;
        }

        /* If file is empty */
        if (sz == 0L) {
                telem_perror("File is empty\n");
                goto file_end;
        }

        if (fseek(fp, 0, SEEK_SET) < 0) {
                telem_log(LOG_ERR, "Fseek failed for file %s:%s\n", fname, strerror(errno));
                goto  file_end;
        }

        size = (size_t)sz;
        contents = malloc(size);
        if (!contents) {
                telem_log(LOG_ERR, "Call to malloc failed");
                goto file_end;
        }
        memset(contents, 0, size);

        bytes_read = fread(contents, sizeof(char), size, fp);

        if (bytes_read < size) {
                telem_log(LOG_ERR, "Error while reading file %s:%s\n", fname, strerror(errno));
                goto file_end;
        }

        struct oops_log_msg oops_msg;
        if (handle_entire_oops(contents, (long)size, &oops_msg)) {
            #ifdef DEBUG
                printf("Raw message:\n");
                for (int i = 0; i < oops_msg.length; i++) {
                        printf("%s\n", oops_msg.lines[i]);
                }
            #endif

                payload = parse_payload(&oops_msg);
            #ifdef DEBUG
                printf("Payload Parsed :%s\n", payload->str);
            #endif

                oops_msg_cleanup(&oops_msg);
                send_data(payload->str, (char *)oops_msg.pattern->classification, (uint32_t)oops_msg.pattern->severity);
                nc_string_free(payload);
        } else {
                /* File does no contain an oops! */
                telem_log(LOG_ERR, "Did not find an oops in the oops directort\n");
        }
file_end:
        if (contents) {
                free(contents);
        }
        if (fp) {
                fclose(fp);
        }
        if (unlink(filename) == -1) {
                telem_log(LOG_ERR, "Failed to unlink %s: %s\n", filename, strerror(errno));
        }
        free(filename);
}

void handle_inotify_event(const struct inotify_event *event)
{

        if (!event) {
                telem_perror("Null event received");
                return;
        }

        if (event->len == 0) {
                telem_perror("inotify event received with no file.");
                return;
        }

        if (event->mask & IN_CLOSE_WRITE) {
                if (event->mask & IN_ISDIR) {
                        return;
                }

                #ifdef DEBUG
                telem_log(LOG_DEBUG, "New file written to the oops location: %s\n", event->name);
                #endif
                handle_oops_file(event->name);
        }
}

int main(int argc, char **argv)
{
        int inotify_fd;
        char buf[4096];
        char *ptr = NULL;
        ssize_t len;
        const struct inotify_event *event;

        inotify_fd = inotify_init();
        if (inotify_fd == -1) {
                telem_log(LOG_ERR, "Failed to create inotify instance: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
        }

        if (inotify_add_watch(inotify_fd, oops_dir_path, IN_CLOSE_WRITE) == -1) {
                telem_perror("Error adding watch to the inotify instance");
        }

        while (1) {
                len = read(inotify_fd, buf, sizeof buf);
                if (len <= 0) {
                        telem_perror("error reading event");
                        continue;
                }

                if (len < sizeof(struct inotify_event)) {
                        telem_perror("Incomplete event received");
                        continue;
                }

                telem_log(LOG_INFO, "Got inotify event/s\n");

                /* Loop over all events in the buffer */
                for (ptr = buf; ptr < buf + len;
                     ptr += sizeof(struct inotify_event) + event->len) {

                        event = (const struct inotify_event *)ptr;
                        /* handle the event */
                        handle_inotify_event(event);
                }
        }

        return 0;
}

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
