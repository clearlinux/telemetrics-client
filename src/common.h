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

/* This is supposed to be shared file for the daemon and the library
   Contains constants and functions for tranfer of data over the socket */

#pragma once

#include <stdint.h>

#define SMALL_LINE_BUF 80
#define RECORD_SIZE_LEN  sizeof(uint32_t)

#define TM_RECORD_VERSION 0
#define TM_CLASSIFICATION 1
#define TM_SEVERITY 2
#define TM_MACHINE_ID 3
#define TM_TIMESTAMP 4
#define TM_ARCH 5
#define TM_HOST_TYPE 6
#define TM_SYSTEM_BUILD 7
#define TM_KERNEL_VERSION 8
#define TM_PAYLOAD_VERSION 9
#define TM_SYSTEM_NAME 10
#define TM_BOARD_NAME 11
#define TM_CPU_MODEL 12
#define TM_BIOS_VERSION 13
#define TM_EVENT_ID 14

#define TM_RECORD_VERSION_STR "record_format_version"
#define TM_CLASSIFICATION_STR "classification"
#define TM_SEVERITY_STR "severity"
#define TM_MACHINE_ID_STR "machine_id"
#define TM_TIMESTAMP_STR "creation_timestamp"
#define TM_ARCH_STR "arch"
#define TM_HOST_TYPE_STR "host_type"
#define TM_SYSTEM_BUILD_STR "build"
#define TM_KERNEL_VERSION_STR "kernel_version"
#define TM_PAYLOAD_VERSION_STR "payload_format_version"
#define TM_SYSTEM_NAME_STR "system_name"
#define TM_BOARD_NAME_STR "board_name"
#define TM_CPU_MODEL_STR "cpu_model"
#define TM_BIOS_VERSION_STR "bios_version"
#define TM_EVENT_ID_STR "event_id"

#define NUM_HEADERS 15

#define EVENT_ID_ALPHAB "0123456789abcdef"
#define EVENT_ID_LEN 32

/* Journal */
#define JOURNAL_PATH "/var/log/telemetry/journal"
#define JOURNAL_TMP "/tmp/telemetry.journal.swp"

/* For internal library usage. Bump the version whenever we change the record
 * structure (e.g. adding or removing a header field). Note that the value
 * should be an unsigned int.
 */
static const uint32_t RECORD_FORMAT_VERSION = 4;

#define TM_SITE_VERSION_FILE "/etc/os-release"
#define TM_DIST_VERSION_FILE "/usr/lib/os-release"

#define TM_OPT_OUT_FILE "/etc/telemetrics/opt-out"

/* Currently max supported payload size is 8kb */
#define MAX_PAYLOAD_SIZE 8192

/* Maximum size for a category string is 122 bytes */
#define MAX_CLASS_LENGTH 122

/* Maximum size for a sub category 40 bytes */
#define MAX_SUBCAT_LENGTH 40

/* Max Payload size is 8 kibabytes (8192) */
#define MAX_PAYLOAD_LENGTH 8192

/* SPOOL DEFINES */
/* Spooling should run every TM_SPOOL_RUN_MAX atleast */
#define TM_SPOOL_RUN_MAX (60 /*min*/ * 60 /*sec*/)

/* Spooling should not run more often than TM_SPOOL_RUN_MIN */
#define TM_SPOOL_RUN_MIN (2 /*min*/ * 60 /*sec*/)

/* Maximum records that can be sent in a single spool run loop*/
#define TM_SPOOL_MAX_SEND_RECORDS 50

/* Maximum records that can be processed in a single spool run loop*/
#define TM_SPOOL_MAX_PROCESS_RECORDS 60

/* Time to check to exit from  the daemon */
#define TM_DAEMON_EXIT_TIME  (2 /*h*/ * 60 /*m*/ * 60 /*sec*/)

/* Very simple structure. Array of header strings and a payload. Calling
 * program is reponsible for passing in the payload as a simple string.
 */
struct telem_record {
        char *headers[NUM_HEADERS];
        char *payload;
        size_t header_size;
        size_t payload_size;
};

const char *get_header_name(int ind);

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
