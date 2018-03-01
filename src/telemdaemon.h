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
#define _GNU_SOURCE     /* for strchrnul() */
#define __STDC_FORMAT_MACROS    /* for PRIu64 */

#include <poll.h>
#include <sys/queue.h>
#include <stdbool.h>
#include <inttypes.h>
#include "journal/journal.h"

#define TM_MACHINE_ID_EXPIRY (3 /*d*/ * 24 /*h*/ * 60 /*m*/ * 60 /*s*/)

#define TM_MACHINE_ID_FILE LOCALSTATEDIR "/lib/telemetry/machine_id"

#define TM_MACHINE_ID_OVERRIDE "/etc/telemetrics/opt-in-static-machine-id"

#define TM_REFRESH_RATE (1 /*h*/ * 60 /*m*/ * 60 /*s*/)

#define TM_RATE_LIMIT_SLOTS (1 /*h*/ * 60 /*m*/)

#define TM_RECORD_COUNTER (1)

typedef struct client {
        int fd;
        uint8_t *buf;
        size_t offset;
        size_t size;
        LIST_ENTRY(client) client_ptrs;
} client;

typedef LIST_HEAD (client_list_head, client) client_list_head;

typedef struct TelemDaemon {
        /* array of fd's being polled */
        struct pollfd  *pollfds;
        /* number of fds being polled */
        nfds_t nfds;
        /* current number of allocated fds */
        size_t current_alloc;
        /* client list head */
        client_list_head client_head;
        /* Time of last failed post */
        time_t bypass_http_post_ts;
        /* Telemetry Journal*/
        TelemJournal *record_journal;
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

        bool is_spool_valid;
        long current_spool_size;

        char *machine_id_override;
} TelemDaemon;

/**
 * Initialize the daemon struct
 *
 * @param daemon A pointer to the daemon structure.
 */
void initialize_daemon(TelemDaemon *daemon);

/**
 * Initialize rate-limiting in the daemon
 *
 * @param daemon A pointer to the daemon structure
 */
void initialize_rate_limit(TelemDaemon *daemon);

/**
 * Add poll fd struct to the array of pollfds.
 *
 * @param daemon The pointer to the daemon struct
 * @param fd The file descriptor to add
 * @param events The events to poll for
 *
 */
void add_pollfd(TelemDaemon *daemon, int fd, short events);

/**
 *  Delete poll fd struct at index i of the array
 *
 * @param daemon The pointer to the daemon
 * @param i The index of the file decriptor to be deleted
 *
 */
void del_pollfd(TelemDaemon *daemon, nfds_t i);

/**
 * Handle data received on a client connection
 *
 * @param daemon The pointer to the daemon
 * @param ind The index of the client's file desciptor in the
 *    pollfd array
 * @param cl Pointer to the client structure in the client list
 *
 * @return true on success, false on failure
 */
bool handle_client(TelemDaemon *daemon, nfds_t ind, client *cl);

/**
 *  Add a client to the client list
 *
 * @param client_head Pointer to the head of the client list
 *    maintained by the daamon
 * @param fd File descriptor to be added to the client list
 *
 * @return Pointer to the client struct if successfully added,
 *    NULL otherwise
 */
client *add_client(client_list_head *client_head, int fd);

/**
 * Remove client from the client list
 *
 * @param client_head Pointer to the client list head
 * @param cl Pointer to the client to be removed
 *
 */
void remove_client(client_list_head *client_head, client *cl);

/**
 * Check if the client list is empty
 *
 * @param client_head Pointer to the head of the client list
 *
 * @return  true if empty
 */
bool is_client_list_empty(client_list_head *client_head);

/**
 * Terminate a client connection
 *
 * @param daemon Pointer to the daemon
 * @param cl Pointer to the client in the client list
 * @param index The index of the client file descriptor in the poll fd
 *    array
 *
 */
void terminate_client(TelemDaemon *daemon, client *cl, nfds_t index);

/**
 * Process record from a client
 *
 * @param daemon Pointer to the daemon
 * @param cl Pointer to the client in the client list
 *
 * @return true on success, false on failure
 */
void process_record(TelemDaemon *daemon, client *cl);

/**
 * Check if burst limiting is enabled
 *
 * @param burst_limit Burst limit amount. If negative rate limiting
 * is disabled
 *
 * @return true if rate limiting for burst_limit variable is enabled
 */
bool burst_limit_enabled(int64_t burst_limit);

/**
 * Checks whether record does not exceed the burst threshold
 *
 * @param current_minute The current minute of time the record processed
 * @param burst_limit Amount of records that can be passed within a window
 * @param window_length Window of time burst limit refers to
 * @param array Pointer to array that stores record or byte count
 * @param incValue Amount to increase array slot by
 *
 * @return true if record does not exceed threshold, and false otherwise
 */
bool rate_limit_check(int current_minute, int64_t burst_limit, int
                      window_length, size_t *array, size_t incValue);

/**
 * Updates rate limit arrays with passing record information
 *
 * @param current_minute The current minute of time the record processed
 * @param window_length Window of time burst limit refers to
 * @param array Pointer to array that stores record or byte count
 * @param incValue Amount to increase array slot by
 *
 */
void rate_limit_update(int current_minute, int window_length,
                       size_t *array, size_t incValue);

/**
 * Checks if the spool strategy is chosen for rate-limiting
 *
 * @param daemon Pointer to the daemon
 *
 * @return true if spool is choosen false if drop is choosen
 */
bool spool_strategy_selected(TelemDaemon *daemon);

/**
 * Check if daemon should direct-spool after encountering network issues.
 *
 * @param daemon Pointer to the daemon
 * @param time The time as time_t (usually from time())
 *
 * @return true if inside direct spool window, false otherwise.
 */
bool inside_direct_spool_window(TelemDaemon *daemon, time_t time);

/**
 * Set the start time of the direct spool window. Will use the
 * current time.
 *
 * @param daemon Pointer to the daemon
 *
 * @return no return value.
 */
void start_network_bypass(TelemDaemon *daemon);

/**
 * Function pointer for posting the records to the server. Replaced by a
 * mock function in the unit tests.
 *
 * @param header_values The header data read from the socket
 * @param body The payload of the record read
 * @param spool Indicate whether the record should be spooled if not
 *    successfully delivered
 *
 * @return true if the record was successfully posted
 */
extern bool (*post_record_ptr)(char *headers[], char *body, bool spool);

/**
 * Post records on a http connection to server
 *
 * @param header_values The header data read from the socket
 * @param body The payload of the record read
 * @param spool Indicate whether the record should be spooled if not
 *    successfully delivered
 *
 * @return true if the record was successfully posted
 */
bool post_record_http(char *header_values[], char *body, bool spool);

/**
 * Spool the record in case it cannot be delivered to the server
 *
 * @param headers The headers for the record
 * @param body The payload of the record
 *
 */
void spool_record(TelemDaemon *daemon, char *headers[], char *body);

/**
 * Get random machine id stored in file
 *
 * @return machine id stored in file if successfully read, 0 otherwise
 */
bool get_machine_id(char *machine_id);

/**
 *  Update machine id periodically
 *
 * @return 0 on success, -1 on failure
 */
int update_machine_id(void);

/**
 * Reads the machine id from the machine id override file if it exists.
 *
 * @return the first 32 bytes in the file, NULL if file does not exist
 * or if the file is empty
 */
char *read_machine_id_override(void);
/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
