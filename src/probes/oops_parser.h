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
#pragma once

#define MAX_LINES 100

#include <glib.h>
#include <stdbool.h>
#include <regex.h>

/*
 * Contains all possible patterns that start a kernel oops msg.
 * Also Defines the severity and classification associated with each oops.
 */
struct oops_pattern {
        const char *begin_line;
        const char *classification;
        int severity;
        bool is_regex;
        regex_t regex;
};

/*
 * This struct holds the lines of an oops messsage once the start has been
 * detected.
 */
struct oops_log_msg {
        char *lines[MAX_LINES];
        int length;
        struct oops_pattern *pattern;
};

/* Callback function to  be passed when the oops parser is invoked async */
typedef void (*oops_handler_t)(struct oops_log_msg *msg);

/* Cleanup the parser when called async */
void oops_parser_cleanup(void);

/*
 * Initialise the parser for async handling.
 * Sets up the handler to be invoked when a complete oops has been detected and
 * handled.
 */
void oops_parser_init(oops_handler_t handler);

/*
 * Handle a single line of the oops msg.
 * Call this repeatedly for a oops log line.
 */
void parse_single_line(char *line, size_t size);

/* Parses a payload from an oops msg*/
GString *parse_payload(struct oops_log_msg *msg);

/* Given an entire oops log, verifies that the log is an oops log and
 * extracts the lines into an oops struct.
 */
bool handle_entire_oops(char *buf, long size, struct oops_log_msg *msg);

/* Frees up the oops log lines from the oops struct */
void oops_msg_cleanup(struct oops_log_msg *msg);

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */

