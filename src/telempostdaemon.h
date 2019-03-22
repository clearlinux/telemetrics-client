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

#define EVENT_SIZE sizeof(struct inotify_event)
#define BUFFER_LEN 1024 * (EVENT_SIZE + 16)
#define NFDS 2
#define TM_RATE_LIMIT_SLOTS (1 /*h*/ * 60 /*m*/)
#define TM_RECORD_COUNTER (1)
#define MAX_RETRY_ATTEMPTS 8
#define NETWORK_BYPASS_DURATION TM_DAEMON_EXIT_TIME

#include <poll.h>
#include <stdbool.h>
#include <sys/inotify.h>

#include "common.h"
#include "journal/journal.h"
#include "configuration.h"

enum fdindex {signlfd, watchfd};

typedef struct TelemPostDaemon {
        int fd;
        int wd;
        int sfd;
        char event_buffer[BUFFER_LEN];
        struct pollfd pollfds[NFDS];
        /* Telemetry Journal*/
        TelemJournal *record_journal;
        /* Time of last failed post */
        time_t bypass_http_post_ts;
        /* Rate limit record and byte arrays */
        size_t record_burst_array[TM_RATE_LIMIT_SLOTS];
        size_t byte_burst_array[TM_RATE_LIMIT_SLOTS];
        /* Rate Limit Configurations */
        bool rate_limit_enabled;
        int64_t record_burst_limit;
        int record_window_length;
        int64_t byte_burst_limit;
        int byte_window_length;
        const char *rate_limit_strategy;
        /* Spool configuration */
        bool is_spool_valid;
        long current_spool_size;
        /* Record local copy and delivery  */
        bool record_retention_enabled;
        bool record_server_delivery_enabled;
} TelemPostDaemon;

/**
 * Initializes daemon
 *
 * @param daemon a pointer to telemetry post daemon
 */
void initialize_daemon(TelemPostDaemon *daemon);

/**
 * Starts daemon
 *
 * @param daemon a pointer to telemetry post daemon
 */
void run_daemon(TelemPostDaemon *daemon);

/**
 * Cleans up inotify descriptors
 *
 * @param daemon a pointer to telemetry post daemon
 */
void close_daemon(TelemPostDaemon *daemon);

/**
 * Processed record written on disk
 *
 * @param filename a pointor to record on disk
 * @param is_retry a boolean value that indicates if
 *        the record has been previously processed.
 * @param daemon post to telemetry post daemon
 */
bool process_staged_record(char *filename, bool is_retry, TelemPostDaemon *daemon);

/**
 * Scans staging directory to process files that were
 * missed by file watcher
 *
 * @param daemon a pointer to telemetry post daemon
 * @return the number of records that were removed from spool
 */
int staging_records_loop(TelemPostDaemon *daemon);

/**
 * Posts a record to backend
 *
 * @param headers a pointer to an array with keys and values
 * @param body a pointer to the payload
 * @param cfg_file a pointer to a non-default configuration
 *        file to be used.
 */
bool post_record_http(char *headers[], char *body, char *cfg_file);

/**
 * Pointer to function to isolate backend call during
 * unit testing.
 *
 * @param headers pointer to array of keys
 * @param body a pinter to payload
 * */
extern bool (*post_record_ptr)(char *headers[], char *body, char *cfg_file);

/** Helper functions **/
/* rate limit check */
bool rate_limit_check(int current_minute, int64_t burst_limit,
                      int window_length, size_t *array, size_t incValue);

/* burst limit check  */
bool burst_limit_enabled(int64_t burst_limit);

/* rate limit check  */
void rate_limit_update(int current_minute, int window_length, size_t *array,
                       size_t incValue);

/* spool strategy check */
bool spool_strategy_selected(TelemPostDaemon *daemon);

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
