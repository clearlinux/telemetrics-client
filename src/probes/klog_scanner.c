/*
 * This program is part of the Clear Linux Project
 *
 * Copyright © 2015-2019 Intel Corporation
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
#include <sys/types.h>
#include <sys/klog.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

#include "common.h"
#include "log.h"
#include "oops_parser.h"
#include "klog_scanner.h"
#include "telemetry.h"
#include "nica/nc-string.h"

static bool oops_processed = false;

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

        /* Truncate payload if necessary, otherwise nothing will be sent */
        if (strlen(backtrace) > MAX_PAYLOAD_LENGTH) {
                backtrace[MAX_PAYLOAD_LENGTH-1] = 0;
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

int klog_process_buffer(char *bufp, int bytes)
{
        // Splits the buffer into separate lines with /0 terminating
        split_buf_by_line(bufp, (size_t)bytes);
        return 0;
}

void split_buf_by_line(char *bufp, int bytes)
{
        char *start;
        char *eol;
        size_t linelength = 0;
        oops_processed = false;

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
                        if (oops_processed == true) {
                                telem_debug("oops_processed detected !\n");
                                break;
                        }
                        linelength = 0;
                }
        }
}

void klog_process_oops_msgs(struct oops_log_msg *msg)
{
        size_t linelength, size;
        char *line;
        char *contents = NULL;
        nc_string *payload;

        // pass 1: calculate buffer size
        size = 0;
        for (int i = 0; i < msg->length; i++) {
                size += strlen(msg->lines[i]) + 1;
        }

        contents = malloc(size);
        if (!contents) {
                telem_log(LOG_ERR, "Call to malloc failed");
                oops_processed = true;
                return;
        }

        // pass 2: copy strings into the buffer
        size_t done = 0;
        char *bp = contents;
        for (int i = 0; i < msg->length; i++) {
                //Add the newline character to the end of each line
                line = msg->lines[i];
                linelength = strlen(line);
                // Copy the line (without the terminating NULL)
                memcpy(bp, line, linelength);
                bp[linelength] = '\n';
                done += linelength + 1;
                bp = contents + done;
        }

        struct oops_log_msg oops_msg;
        if (handle_entire_oops(contents, (long)size, &oops_msg)) {
#ifdef DEBUG
                telem_debug("DEBUG: Raw oops message:\n");
                for (int i = 0; i < oops_msg.length; i++) {
                        telem_log(LOG_DEBUG, "%s\n", oops_msg.lines[i]);
                }
#endif
                payload = parse_payload(&oops_msg);
                telem_debug("DEBUG: Payload Parsed :%s\n", payload->str);
                oops_msg_cleanup(&oops_msg);
                send_data(payload->str, (char *)oops_msg.pattern->classification, (uint32_t)oops_msg.pattern->severity);
                nc_string_free(payload);
        } else {
                /* File does no contain an oops! */
                telem_log(LOG_ERR, "Did not find an oops in the oops directory\n");
        }

        free(contents);
        oops_processed = true;
}

void signal_handler_success(int signum)
{
        exit(EXIT_SUCCESS);
}

void signal_handler_fail(int signum)
{
        exit(EXIT_FAILURE);
}

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
