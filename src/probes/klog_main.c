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
#include <sys/klog.h>
#include <errno.h>
#include <unistd.h>
#include <malloc.h>
#include <signal.h>

#include "log.h"
#include "oops_parser.h"
#include "klog_scanner.h"

#define SYSLOG_ACTION_READ 2
#define SYSLOG_ACTION_SIZE_BUFFER 10
#define MAX_BUF 8192

int main(void)
{
        int log_size = 0;
        char *bufp = NULL;
        size_t buflen = 0;

        oops_parser_init(klog_process_oops_msgs);

        // Signal Handling to terminate the probe.
        if (signal (SIGINT, signal_handler_fail) == SIG_ERR) {
                telem_log(LOG_ERR, "Error handling interrupt signal\n");
        }
        if (signal (SIGTERM, signal_handler_success) == SIG_ERR) {
                telem_log(LOG_ERR, "Error handling terminating signal\n");
        }

        // Gets the size of the kernel ring buffer
        log_size = klogctl(SYSLOG_ACTION_SIZE_BUFFER, NULL, 0);
        if (log_size < 0) {
                telem_log(LOG_ERR, "Cannot read size of kernel ring buffer: %s\n", strerror(errno));
                return 1;
        }

        buflen = (size_t)log_size;
        if (buflen > MAX_BUF) {
                buflen = MAX_BUF;
        }

        // Gets the contents of the kernel ring buffer
        bufp = (char *)malloc(buflen);

        while (1) {
                int bytes_read;
 
                malloc_trim(0);
                memset(bufp, 0, buflen);
                bytes_read = klogctl(SYSLOG_ACTION_READ, bufp, (int)buflen);
                if (bytes_read < 0) {
                        telem_log(LOG_ERR, "Cannot read contents of kernel ring buffer: %s\n", strerror(errno));
                        return 1;
                }

                klog_process_buffer(bufp, (size_t)bytes_read);
        }

        // Not reached
        free(bufp);
        return 0;
}

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
