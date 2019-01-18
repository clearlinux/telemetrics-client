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

#pragma once
#include "oops_parser.h"

/**
 * Process the buffer and send it to the backend
 *
 * @param *bufp A pointer to a buffer containing the bytes read
 *		from the kernel ring buffer
 * @param bytes Number of bytes read from ring buffer into bufp
 * 
 * @return 0 on succes, -1 on failure
 *
 */
int klog_process_buffer(char *bufp, int bytes);
/**
 * Splits the buffer into individual lines
 *
 * @param *bufp A pointer to a buffer containing the bytes read
 *		from the kernel ring buffer
 * @param bytes Number of bytes read from ring buffer into bufp
 *
 */
void split_buf_by_line(char *bufp, int bytes);

/**
 * Process the oops message and send it to the backend
 *
 * @param msg Struct that contains oops message lines and linecount
 *
 */
void klog_process_oops_msgs(struct oops_log_msg *msg);
/**
 * Signal handler for cleanup purposes
 *
 * @param signum An integer for the signal type
 *
 */
void signal_handler_fail(int signum);
void signal_handler_success(int signum);

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
