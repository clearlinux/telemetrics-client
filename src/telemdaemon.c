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
#include <curl/curl.h>
#include <unistd.h>
#include <limits.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <malloc.h>
#include <sys/uio.h>

#include "telemdaemon.h"
#include "common.h"
#include "util.h"
#include "log.h"
#include "configuration.h"

void initialize_daemon(TelemDaemon *daemon)
{
        client_list_head head;
        LIST_INIT(&head);
        daemon->nfds = 0;
        daemon->pollfds = NULL;
        daemon->client_head = head;
        daemon->bypass_http_post_ts = 0;
        daemon->is_spool_valid = false;
        initialize_rate_limit(daemon);
        daemon->current_spool_size = 0;
}

void initialize_rate_limit(TelemDaemon *daemon)
{
        for (int i = 0; i < TM_RATE_LIMIT_SLOTS; i++) {
                daemon->record_burst_array[i] = 0;
                daemon->byte_burst_array[i] = 0;
        }
        daemon->rate_limit_enabled = rate_limit_enabled_config();
        daemon->record_burst_limit = record_burst_limit_config();
        daemon->record_window_length = record_window_length_config();
        daemon->byte_burst_limit = byte_burst_limit_config();
        daemon->byte_window_length = byte_window_length_config();
        daemon->rate_limit_strategy = rate_limit_strategy_config();
}

client *add_client(client_list_head *client_head, int fd)
{
        client *cl;

        cl = (client *)malloc(sizeof(client));
        if (cl) {
                cl->fd = fd;
                cl->offset = 0;
                cl->buf = NULL;

                LIST_INSERT_HEAD(client_head, cl, client_ptrs);
        }
        return cl;
}

void remove_client(client_list_head *client_head, client *cl)
{
        assert(cl);
        LIST_REMOVE(cl, client_ptrs);
        if (cl->buf) {
                free(cl->buf);
        }
        if (cl->fd >= 0) {
                close(cl->fd);
        }
        free(cl);
        cl = NULL;
}

bool is_client_list_empty(client_list_head *client_head)
{
        return (client_head->lh_first == NULL);
}

void terminate_client(TelemDaemon *daemon, client *cl, nfds_t index)
{
        /* Remove fd from the pollfds array */
        del_pollfd(daemon, index);

        telem_log(LOG_INFO, "Removing client: %d\n", cl->fd);

        /* Remove client from the client list */
        remove_client(&(daemon->client_head), cl);
}

bool handle_client(TelemDaemon *daemon, nfds_t ind, client *cl)
{
        /* For now  read data from fd */
        ssize_t len;
        size_t record_size = 0;
        bool processed = false;

        if (!cl->buf) {
                cl->buf = malloc(RECORD_SIZE_LEN);
                cl->size = RECORD_SIZE_LEN;
        }
        if (!cl->buf) {
                telem_log(LOG_ERR, "Unable to allocate memory, exiting\n");
                exit(EXIT_FAILURE);
        }

        malloc_trim(0);
        len = recv(cl->fd, cl->buf, RECORD_SIZE_LEN, MSG_PEEK | MSG_DONTWAIT);
        if (len < 0) {
                telem_log(LOG_ERR, "Failed to talk to client %d: %s\n", cl->fd,
                          strerror(errno));
                goto end_client;
        } else if (len == 0) {
                /* Connection closed by client, most likely */
                telem_log(LOG_INFO, "No data to receive from client %d\n",
                          cl->fd);
                goto end_client;
        }

        do {
                malloc_trim(0);
                len = recv(cl->fd, cl->buf + cl->offset, cl->size - cl->offset, 0);
                if (len < 0) {
                        telem_log(LOG_ERR, "Failed to receive data from client"
                                  " %d: %s\n", cl->fd, strerror(errno));
                        goto end_client;
                } else if (len == 0) {
                        telem_log(LOG_DEBUG, "End of transmission for client"
                                  " %d\n", cl->fd);
                        goto end_client;
                }

                cl->offset += (size_t)len;
                if (cl->offset < RECORD_SIZE_LEN) {
                        continue;
                }
                if (cl->size == RECORD_SIZE_LEN) {
                        record_size = *(uint32_t *)(cl->buf);
                        telem_log(LOG_DEBUG, "Total size of record: %zu\n", record_size);
                        if (record_size == 0) {     //record_size < RECORD_MIN_SIZE || record_size > RECORD_MAX_SIZE
                                goto end_client;
                        }
                        // We just processed the record size field, so the remaining format
                        // is (header size field + record body + terminating '\0' byte)
                        cl->size = sizeof(uint32_t) + record_size + 1;
                        cl->buf = realloc(cl->buf, cl->size);
                        memset(cl->buf, 0, cl->size);
                        cl->offset = 0;
                        if (!cl->buf) {
                                telem_log(LOG_ERR, "Unable to allocate memory, exiting\n");
                                exit(EXIT_FAILURE);
                        }
                }
                if (cl->offset != cl->size) {
                        /* full record not received yet */
                        continue;
                }
                if (cl->size != RECORD_SIZE_LEN) {
                        /* entire record has been received */
                        process_record(daemon, cl);
                        /* TODO: cleanup or terminate? */
                        cl->offset = 0;
                        cl->size = RECORD_SIZE_LEN;
                        processed = true;
                        telem_log(LOG_DEBUG, "Record processed for client %d\n", cl->fd);
                        break;
                }
        } while (len > 0);

end_client:
        telem_log(LOG_DEBUG, "Processed client %d: %s\n", cl->fd, processed ? "true" : "false");
        terminate_client(daemon, cl, ind);
        return processed;
}

char *read_machine_id_override()
{
        char *machine_override = NULL;
        FILE *fp = NULL;
        size_t bytes_read;
        char *endl = NULL;

        fp = fopen(TM_MACHINE_ID_OVERRIDE, "r");
        if (!fp) {
                if (errno == ENOMEM) {
                        exit(EXIT_FAILURE);
                } else if (errno != ENOENT) {
                        /* Log any other error */
                        telem_log(LOG_ERR, "Unable to open static machine id file %s: %s\n",
                                  TM_MACHINE_ID_OVERRIDE, strerror(errno));
                }
                return NULL;
        }

        machine_override = malloc(sizeof(char) * 33);
        if (!machine_override) {
                exit(EXIT_FAILURE);
        }

        memset(machine_override, 0, 33);

        bytes_read = fread(machine_override, sizeof(char), 32, fp);
        if (bytes_read == 0) {
                if (ferror(fp) != 0) {
                        telem_log(LOG_ERR, "Error while reading %s file: %s\n",
                                  TM_MACHINE_ID_OVERRIDE, strerror(errno));
                }
                fclose(fp);
                free(machine_override);
                return NULL;
        }

        endl = memchr(machine_override, '\n', 32);
        if (endl) {
                *endl = '\0';
        }

        fclose(fp);
        return machine_override;
}

void machine_id_replace(char **machine_header, char *machine_id_override)
{
        char machine_id[33] = { 0 };
        char *old_header;
        int ret;

        if (machine_id_override) {
                strcpy(machine_id, machine_id_override);
        } else {
                if (!get_machine_id(machine_id)) {
                        // TODO: decide if error handling is needed here
                        machine_id[0] = '0';
                }
        }

        old_header = *machine_header;
        ret = asprintf(machine_header, "%s: %s", TM_MACHINE_ID_STR, machine_id);

        if (ret == -1) {
                telem_log(LOG_ERR, "Failed to write machine id header\n");
                exit(EXIT_FAILURE);
        }
        free(old_header);
}

void process_record(TelemDaemon *daemon, client *cl)
{
        int i = 0;
        char *headers[NUM_HEADERS];
        bool do_spool = false;
        bool record_sent = false;
        char *tok = NULL;
        size_t header_size = 0;
        size_t message_size = 0;
        char *temp_headers = NULL;
        char *msg;
        char *body;
        int current_minute = 0;
        bool record_check_passed = true;
        bool byte_check_passed = true;
        bool record_burst_enabled = true;
        bool byte_burst_enabled = true;

        /* Gets the current minute of time */
        time_t temp = time(NULL);
        struct tm *tm_s = localtime(&temp);
        current_minute = tm_s->tm_min;

        header_size = *(uint32_t *)cl->buf;
        message_size = cl->size - header_size;
        assert(message_size > 0);      //TODO:Check for min and max limits
        msg = (char *)cl->buf + sizeof(uint32_t);

        /* Copying the headers as strtok modifies the orginal buffer */
        temp_headers = strndup(msg, header_size);
        tok = strtok(temp_headers, "\n");

        for (i = 0; i < NUM_HEADERS; i++) {
                const char *header_name = get_header_name(i);

                if (get_header(tok, header_name, &headers[i], strlen(header_name))) {
                        if (strcmp(header_name, TM_MACHINE_ID_STR) == 0) {
                                machine_id_replace(&headers[i], daemon->machine_id_override);
                        }

                        tok = strtok(NULL, "\n");
                } else {
                        telem_log(LOG_ERR, "process_record: Incorrect headers in record\n");
                        goto end;
                }
        }
        /* TODO : check if the body is within the limits. */
        body = msg + header_size;

        if (inside_direct_spool_window(daemon, time(NULL))) {
                telem_log(LOG_INFO, "process_record: delivering directly to spool\n");
                spool_record(daemon, headers, body);
                goto end;
        }
        if (daemon->record_window_length == -1 ||
            daemon->byte_window_length == -1) {
                telem_log(LOG_ERR, "Invalid value for window length\n");
                exit(EXIT_FAILURE);
        }
        /* Checks if entirety of rate limiting is enabled */
        if (daemon->rate_limit_enabled) {
                /* Checks whether record and byte bursts are enabled individually */
                record_burst_enabled = burst_limit_enabled(daemon->record_burst_limit);
                byte_burst_enabled = burst_limit_enabled(daemon->byte_burst_limit);

                if (record_burst_enabled) {
                        record_check_passed = rate_limit_check(current_minute,
                                                               daemon->record_burst_limit, daemon->record_window_length,
                                                               daemon->record_burst_array, TM_RECORD_COUNTER);
                }
                if (byte_burst_enabled) {
                        byte_check_passed = rate_limit_check(current_minute,
                                                             daemon->byte_burst_limit, daemon->byte_window_length,
                                                             daemon->byte_burst_array, cl->size);
                }
                /* If both record and byte burst disabled, rate limiting disabled */
                if (!record_burst_enabled && !byte_burst_enabled) {
                        daemon->rate_limit_enabled = false;
                }
        }
        /* Sends record if rate limiting is disabled, or all checks passed */
        if (!daemon->rate_limit_enabled || (record_check_passed && byte_check_passed)) {
                /* Send the record as https post */
                record_sent = post_record_ptr(headers, body, true);
        }

        // Get rate-limit strategy
        do_spool = spool_strategy_selected(daemon);

        // Drop record
        if (!record_sent && !do_spool) {
                goto end;
        }
        // Spool Record
        else if (!record_sent && do_spool) {
                start_network_bypass(daemon);
                telem_log(LOG_INFO, "process_record: initializing direct-spool window\n");
                spool_record(daemon, headers, body);
        } else {
                /* Updates rate limiting arrays if record sent */
                if (record_burst_enabled) {
                        rate_limit_update (current_minute, daemon->record_window_length,
                                           daemon->record_burst_array, TM_RECORD_COUNTER);
                }
                if (byte_burst_enabled) {
                        rate_limit_update (current_minute, daemon->byte_window_length,
                                           daemon->byte_burst_array, cl->size);
                }
        }

end:
        free(temp_headers);
        for (int k = 0; k < i; k++)
                free(headers[k]);
        return;
}

bool burst_limit_enabled(int64_t burst_limit)
{
        return (burst_limit > -1) ? true : false;
}

bool rate_limit_check(int current_minute, int64_t burst_limit, int
                      window_length, size_t *array, size_t incValue)
{
        size_t count = 0;

        /* Variable to determine last minute allowed in burst window */
        int window_start = (TM_RATE_LIMIT_SLOTS + (current_minute - window_length + 1))
                           % TM_RATE_LIMIT_SLOTS;

        /* The modulo is not placed in the for loop inself, because if
         * the current minute is less than the window start (from wrapping)
         * the for loop will never be entered.
         */

        /* Counts all elements within burst_window in array */
        for (int i = window_start; i < (window_start + window_length); i++) {
                count += array[i % TM_RATE_LIMIT_SLOTS];

                if ((array[i % TM_RATE_LIMIT_SLOTS] + incValue) >= SIZE_MAX) {
                        /* Exceeds maximum size array can hold */
                        return false;
                }
        }
        /* Include current record being processed to count */
        count += incValue;

        /* Determines whether the count has exceeded the limit or not */
        return (count > burst_limit) ? false : true;
}

void rate_limit_update(int current_minute, int window_length, size_t *array,
                       size_t incValue)
{
        /* Variable to determine the amount of zero'd out spots needed */
        int blank_slots = (TM_RATE_LIMIT_SLOTS - window_length);

        /* Update specified array depending on increment value */
        array[current_minute] += incValue;

        /* Zero out expired records for record array */
        for (int i = current_minute + 1; i < (blank_slots + current_minute + 1); i++) {
                array[i % TM_RATE_LIMIT_SLOTS] = 0;
        }
}

bool spool_strategy_selected(TelemDaemon *daemon)
{
        /* Performs action depending on strategy choosen */
        return (strcmp(daemon->rate_limit_strategy, "spool") == 0) ? true : false;
}

bool inside_direct_spool_window(TelemDaemon *daemon, time_t current_time)
{
        return (current_time < daemon->bypass_http_post_ts + 1800) ? true : false;
}

void start_network_bypass(TelemDaemon *daemon)
{
        daemon->bypass_http_post_ts = time(NULL);
}

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
        telem_log(LOG_DEBUG, "Received data:\n%s\n", ptr);
        return size * nmemb;
}

bool post_record_http(char *headers[], char *body, bool spool)
{
        CURL *curl;
        int res = 0;
        char *content = "Content-Type: application/text";
        struct curl_slist *custom_headers = NULL;
        char errorbuf[CURL_ERROR_SIZE];
        long http_response = 0;
        const char *cert_file = get_cainfo_config();
        const char *tid_header = get_tidheader_config();

        // Initialize the libcurl global environment once per POST. This lets us
        // clean up the environment after each POST so that when the daemon is
        // sitting idle, it will be consuming as little memory as possible.
        curl_global_init(CURL_GLOBAL_ALL);

        curl = curl_easy_init();
        if (!curl) {
                telem_log(LOG_ERR, "curl_easy_init(): Unable to start libcurl"
                          " easy session, exiting\n");
                exit(EXIT_FAILURE);
                /* TODO: check if memory needs to be released */
        }

        // Errors for any curl_easy_* functions will store nice error messages
        // in errorbuf, so send log messages with errorbuf contents
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorbuf);

        curl_easy_setopt(curl, CURLOPT_URL, server_addr_config());
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_POST, 1);
#ifdef DEBUG
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
#endif

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

        for (int i = 0; i < NUM_HEADERS; i++) {
                custom_headers = curl_slist_append(custom_headers, headers[i]);
        }
        custom_headers = curl_slist_append(custom_headers, tid_header);
        // This should be set by probes/libtelemetry in the future
        custom_headers = curl_slist_append(custom_headers, content);

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, custom_headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(body));
        curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_TRY);

        if (strlen(cert_file) > 0) {
                if (access(cert_file, F_OK) != -1) {
                        curl_easy_setopt(curl, CURLOPT_CAINFO, cert_file);
                        telem_log(LOG_INFO, "cafile was set to %s\n", cert_file);
                }
        }

        telem_log(LOG_DEBUG, "Executing curl operation...\n");
        errorbuf[0] = 0;
        res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_response);

        if (res) {
                size_t len = strlen(errorbuf);
                if (len) {
                        telem_log(LOG_ERR, "Failed sending record: %s%s", errorbuf,
                                  ((errorbuf[len - 1] != '\n') ? "\n" : ""));
                } else {
                        telem_log(LOG_ERR, "Failed sending record: %s\n",
                                  curl_easy_strerror(res));
                }
        } else if (http_response != 201 && http_response != 200) {
                /*  201 means the record was  successfully created
                 *  200 is a generic "ok".
                 *  TODO: Make list of white-listed response codes configurable?
                 */
                telem_log(LOG_ERR, "Encountered error %ld on the server\n",
                          http_response);
                // We treat HTTP error codes the same as libcurl errors
                res = 1;
        } else {
                telem_log(LOG_INFO, "Record sent successfully\n");
        }

        curl_slist_free_all(custom_headers);
        curl_easy_cleanup(curl);

        curl_global_cleanup();

        return res ? false : true;
}

void spool_record(TelemDaemon *daemon, char *headers[], char *body)
{
        int ret = 0;
        struct stat stat_buf;
        int64_t max_spool_size = 0;
        char *tmpbuf = NULL;
        FILE *tmpfile = NULL;
        int tmpfd = 0;

        if (!daemon->is_spool_valid) {
                /* If spool is not valid, simply drop the record */
                return;
        }

        //check if the size is greater than 1 MB/spool max size
        max_spool_size = spool_max_size_config();
        if (max_spool_size != -1) {
                telem_log(LOG_DEBUG, "Total size of spool dir: %ld\n", daemon->current_spool_size);
                if (daemon->current_spool_size < 0) {
                        telem_log(LOG_ERR, "Error getting spool directory size: %s\n",
                                  strerror(-(int)(daemon->current_spool_size)));
                        return;
                } else if (daemon->current_spool_size >= (max_spool_size * 1024)) {
                        telem_log(LOG_INFO, "Spool dir full, dropping record\n");
                        return;
                }
        }

        // create file with record
        ret = asprintf(&tmpbuf, "%s/XXXXXX", spool_dir_config());
        if (ret == -1) {
                telem_log(LOG_ERR, "Failed to allocate memory for record name, aborting\n");
                exit(EXIT_FAILURE);
        }

        tmpfd = mkstemp(tmpbuf);
        if (tmpfd == -1) {
                telem_perror("Error while creating temp file");
                goto spool_err;
        }

        //fp = fopen(spool_dir_path, const char *mode);
        tmpfile = fdopen(tmpfd, "a");
        if (!tmpfile) {
                telem_perror("Error opening temp file");
                close(tmpfd);
                if (unlink(tmpbuf)) {
                        telem_perror("Error deleting temp file");
                }
                goto spool_err;
        }

        // write headers
        for (int i = 0; i < NUM_HEADERS; i++) {
                fprintf(tmpfile, "%s\n", headers[i]);
        }

        //write body
        fprintf(tmpfile, "%s\n", body);
        fflush(tmpfile);
        fclose(tmpfile);

        /* The stat should not fail here unless it is ENOMEM */
        if (stat(tmpbuf, &stat_buf) == 0) {
                daemon->current_spool_size += (stat_buf.st_blocks * 512);
        }

spool_err:
        free(tmpbuf);
        return;
}

void add_pollfd(TelemDaemon *daemon, int fd, short events)
{
        assert(daemon);
        assert(fd >= 0);

        if (!daemon->pollfds) {
                daemon->pollfds = (struct pollfd *)malloc(sizeof(struct pollfd));
                if (!daemon->pollfds) {
                        telem_log(LOG_ERR, "Unable to allocate memory for"
                                  " pollfds array, exiting\n");
                        exit(EXIT_FAILURE);
                }
                daemon->current_alloc = sizeof(struct pollfd);
        } else {
                /* Reallocate here */
                if (!reallocate((void **)&(daemon->pollfds),
                                &(daemon->current_alloc),
                                ((daemon->nfds + 1) * sizeof(struct pollfd)))) {
                        telem_log(LOG_ERR, "Unable to realloc, exiting\n");
                        exit(EXIT_FAILURE);
                }
        }

        daemon->pollfds[daemon->nfds].fd = fd;
        daemon->pollfds[daemon->nfds].events = events;
        daemon->pollfds[daemon->nfds].revents = 0;
        daemon->nfds++;
}

void del_pollfd(TelemDaemon *daemon, nfds_t i)
{
        assert(daemon);
        assert(i < daemon->nfds);

        if (i < daemon->nfds - 1) {
                memmove(&(daemon->pollfds[i]), &(daemon->pollfds[i + 1]),
                        (sizeof(struct pollfd) * (daemon->nfds - i - 1)));
        }
        daemon->nfds--;
}

bool get_machine_id(char *machine_id)
{
        FILE *id_file = NULL;
        int ret;

        char *machine_id_file_name = TM_MACHINE_ID_FILE;

        id_file = fopen(machine_id_file_name, "r");
        if (id_file == NULL) {
                telem_log(LOG_ERR, "Could not open machine id file\n");
                return false;
        }

        ret = fscanf(id_file, "%s", machine_id);
        if (ret != 1) {
                telem_perror("Could not read machine id from file");
                fclose(id_file);
                return false;
        }
        fclose(id_file);
        return true;
}

int machine_id_write(uint64_t upper, uint64_t lower)
{
        FILE *fp;
        int ret = 0;
        int result = -1;

        fp = fopen(TM_MACHINE_ID_FILE, "w");
        if (fp == NULL) {
                telem_perror("Could not open machine id file");
                goto file_error;
        }

        ret = fprintf(fp, "%.16" PRIx64 "%.16" PRIx64, upper, lower);
        if (ret < 0) {
                telem_perror("Unable to write to machine id file");
                goto file_error;
        }
        result = 0;
        fflush(fp);
file_error:
        if (fp != NULL) {
                fclose(fp);
        }

        return result;
}

int generate_machine_id(void)
{
        int frandom = -1;
        struct stat stat_buf;
        int ret = 0;
        struct iovec random_id[2];
        uint64_t random_id_upper;
        uint64_t random_id_lower;
        int result = -1;

        ret = stat("/dev/urandom", &stat_buf);
        if (ret == -1) {
                telem_log(LOG_ERR, "Unable to stat urandom device\n");
                goto rand_err;
        }

        if (!S_ISCHR(stat_buf.st_mode)) {
                telem_log(LOG_ERR, "/dev/urandom is not a character device file\n");
                goto rand_err;
        }

        /* TODO : check for major and minor numbers to be extra sure ??
           ((stat_buffer.st_rdev == makedev(1, 8)) || (stat_buffer.st_rdev == makedev(1, 9))*/

        frandom = open("/dev/urandom", O_RDONLY);
        if (frandom < 0) {
                telem_perror("Error opening /dev/urandom");
                goto rand_err;
        }

        random_id[0].iov_base = &random_id_upper;
        random_id[0].iov_len = 8;
        random_id[1].iov_base = &random_id_lower;
        random_id[1].iov_len = 8;

        if (readv(frandom, random_id, 2) < 0) {
                telem_perror("Error while reading /dev/urandom");
                goto rand_err;
        }

        result = machine_id_write(random_id_upper, random_id_lower);
rand_err:
        if (frandom >= 0) {
                close(frandom);
        }
        return result;
}

int update_machine_id()
{
        int result = 0;
        struct stat buf;
        int ret = 0;

        char *machine_id_filename = TM_MACHINE_ID_FILE;
        ret = stat(machine_id_filename, &buf);

        if (ret == -1) {
                if (errno == ENOENT) {
                        telem_log(LOG_INFO, "Machine id file does not exist\n");
                        result = generate_machine_id();
                } else {
                        telem_log(LOG_ERR, "Unable to stat machine id file\n");
                        result = -1;
                }
        } else {
                time_t current_time = time(NULL);

                if ((current_time - buf.st_mtime) > TM_MACHINE_ID_EXPIRY) {
                        telem_log(LOG_INFO, "Machine id file has expired\n");
                        result = generate_machine_id();
                }
        }
        return result;
}

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
