/*
 * This program is part of the Clear Linux Project
 *
 * Copyright 2018-2023 Intel Corporation
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
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <dirent.h>
#include <malloc.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <sys/signalfd.h>

#include "log.h"
#include "util.h"
#include "spool.h"
#include "iorecord.h"
#include "retention.h"
#include "telempostdaemon.h"

/* spool window check */
static bool inside_direct_spool_window(TelemPostDaemon *daemon, time_t current_time)
{
        return (current_time < daemon->bypass_http_post_ts + NETWORK_BYPASS_DURATION) ? true : false;
}

/* set http bypass */
static void start_network_bypass(TelemPostDaemon *daemon)
{
        daemon->bypass_http_post_ts = time(NULL);
}

/* burst limit check  */
bool burst_limit_enabled(int64_t burst_limit)
{
        return (burst_limit > -1) ? true : false;
}

/* rate limit check */
bool rate_limit_check(int current_minute, int64_t burst_limit, int
                      window_length, size_t *array, size_t incValue)
{
        size_t count = 0;

        /* Variable to determine last minute allowed in burst window */
        int window_start = (TM_RATE_LIMIT_SLOTS + (current_minute - window_length + 1))
                           % TM_RATE_LIMIT_SLOTS;

        /* The modulo is not placed in the for loop itself, because if
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

/* spool strategy check */
bool spool_strategy_selected(TelemPostDaemon *daemon)
{
        /* Performs action depending on strategy choosen */
        return (strcmp(daemon->rate_limit_strategy, "spool") == 0) ? true : false;
}

/* rate limit check  */
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

static void set_pollfd(TelemPostDaemon *daemon, int fd, enum fdindex i, short events)
{
        assert(daemon);
        assert(fd);

        daemon->pollfds[i].fd = fd;
        daemon->pollfds[i].events = events;
        daemon->pollfds[i].revents = 0;
}

static void initialize_signals(TelemPostDaemon *daemon)
{
        int sigfd;
        sigset_t mask;

        sigemptyset(&mask);

        if (sigaddset(&mask, SIGHUP) != 0) {
                telem_perror("Error adding signal SIGHUP to mask");
                exit(EXIT_FAILURE);
        }

        if (sigaddset(&mask, SIGINT) != 0) {
                telem_perror("Error adding signal SIGINT to mask");
                exit(EXIT_FAILURE);
        }

        if (sigaddset(&mask, SIGTERM) != 0) {
                telem_perror("Error adding signal SIGTERM to mask");
                exit(EXIT_FAILURE);
        }

        if (sigaddset(&mask, SIGPIPE) != 0) {
                telem_perror("Error adding signal SIGPIPE to mask");
                exit(EXIT_FAILURE);
        }

        if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
                telem_perror("Error changing signal mask with SIG_BLOCK");
                exit(EXIT_FAILURE);
        }

        sigfd = signalfd(-1, &mask, 0);
        if (sigfd == -1) {
                telem_perror("Error creating the signalfd");
                exit(EXIT_FAILURE);
        }

        daemon->sfd = sigfd;

        set_pollfd(daemon, sigfd, signlfd, POLLIN);
}

static void initialize_rate_limit(TelemPostDaemon *daemon)
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

static void initialize_record_delivery(TelemPostDaemon *daemon)
{
        daemon->record_retention_enabled = record_retention_enabled_config();
        daemon->record_server_delivery_enabled = record_server_delivery_enabled_config();
}

void initialize_post_daemon(TelemPostDaemon *daemon)
{
        assert(daemon);

        daemon->bypass_http_post_ts = 0;
        daemon->is_spool_valid = is_spool_valid();
        daemon->record_journal = open_journal(JOURNAL_PATH);
        daemon->fd = inotify_init();
        if (daemon->fd < 0) {
                telem_perror("Error initializing inotify");
                exit(EXIT_FAILURE);
        }
        daemon->wd = inotify_add_watch(daemon->fd, spool_dir_config(), IN_CLOSE_WRITE);

        initialize_signals(daemon);
        set_pollfd(daemon, daemon->fd, watchfd, POLLIN);

        initialize_rate_limit(daemon);
        initialize_record_delivery(daemon);
        /* Register record retention delete action as a callback to prune entry */
        if (daemon->record_journal != NULL && daemon->record_retention_enabled) {
                daemon->record_journal->prune_entry_callback = &delete_record_by_id;
        }
        daemon->current_spool_size = 0;
}

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
        telem_log(LOG_DEBUG, "Received data:\n%s\n", ptr);
        return size * nmemb;
}

char *create_json_message(char *tm_headers[], char *tm_payload)
{
        /*
         * Embed the telemetry record headers and the telemetry payload into a
         * JSON object string.
         */
        char *json_string = NULL;
        json_object *root = json_object_new_object();

        /* Add the telemetry record headers */

        for (int i = 0; i < NUM_HEADERS; i++) {
                /* ex: arch: x86_64 */

                /* Split the header field into name/value */
                strtok(tm_headers[i], ":");
                json_object *value = json_object_new_string(strtok(NULL, " "));
                json_object_object_add(root, tm_headers[i], value);
        }
        json_object *payload = json_object_new_string(tm_payload);
        json_object_object_add(root, "payload", payload);

        /* Keep our own copy of the json string */
        json_string = strdup(json_object_to_json_string_ext(root,
                                                            JSON_C_TO_STRING_PLAIN |
                                                            JSON_C_TO_STRING_NOSLASHESCAPE));

        /* Free the memory associated with the JSON objects */
        json_object_put(root);

        return json_string;
}

bool post_record_http(char *headers[], char *body, char *cfg)
{
        CURL *curl;
        int res = 0;
        char *content = "Content-Type: application/json";
        struct curl_slist *custom_headers = NULL;
        char errorbuf[CURL_ERROR_SIZE];
        char *json_body = NULL;
        long http_response = 0;
        const char *cert_file = get_cainfo_config();
        const char *tid_header = get_tidheader_config();
        const char *saved_config_file = NULL;

        if (cfg != NULL) {
                saved_config_file = get_config_file();
                if (set_config_file(cfg) != 0) {
                       telem_log(LOG_ERR, "set-config_file(): Failed to set %s\n", cfg);
                       // If we fail to load the specified config file, do not send the
                       // record out. We don't want to send the record out with different
                       // settings than explicitly requested.
                       // However, report success so the record gets deleted.
                       res = 0;
                       goto Done;
                }
                reload_config();
                telem_debug("DEBUG: override server_addr:%s\n", server_addr_config());
        }

        // Generate the JSON message body
        json_body = create_json_message(headers, body);

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

        custom_headers = curl_slist_append(custom_headers, tid_header);
        // This should be set by probes/libtelemetry in the future
        custom_headers = curl_slist_append(custom_headers, content);

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, custom_headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(json_body));
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
                        telem_log(LOG_DEBUG, "Failed sending record: %s%s", errorbuf,
                                  ((errorbuf[len - 1] != '\n') ? "\n" : ""));
                } else {
                        telem_log(LOG_DEBUG, "Failed sending record: %s\n",
                                  curl_easy_strerror(res));
                }
        } else if (http_response != 201 && http_response != 200) {
                /*  201 means the record was  successfully created
                 *  200 is a generic "ok".
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

Done:
        if (json_body) {
                free(json_body);
                json_body = NULL;
        }

        if (saved_config_file != NULL) {
                if (set_config_file(saved_config_file) != 0) {
                        telem_log(LOG_ERR, "set-config_file(): Failed to set %s",
                                  saved_config_file);
                        res = 1;
                }
                reload_config();
                telem_debug("DEBUG: restored server_addr:%s\n", server_addr_config());
        }

        return res ? false : true;
}

static void save_local_copy(TelemPostDaemon *daemon, char *body)
{
        int ret = 0;
        char *tmpbuf = NULL;
        FILE *tmpfile = NULL;

        if (daemon == NULL || daemon->record_journal == NULL ||
            daemon->record_journal->latest_record_id == NULL) {
                return;
        }

        ret = asprintf(&tmpbuf, "%s/%s", RECORD_RETENTION_DIR,
                       daemon->record_journal->latest_record_id);
        if (ret == -1) {
                telem_log(LOG_ERR, "Failed to allocate memory for record full path, aborting\n");
                return;
        }

        tmpfile = fopen(tmpbuf, "w");
        if (!tmpfile) {
                telem_perror("Error opening local record copy temp file");
                goto save_err;
        }

        // Save body
        fprintf(tmpfile, "%s\n", body);
        fclose(tmpfile);

save_err:
        free(tmpbuf);
        return;
}

static void save_entry_to_journal(TelemPostDaemon *daemon, time_t t_stamp, char *headers[])
{
        char *classification_value = NULL;
        char *event_id_value = NULL;

        if (get_header_value(headers[TM_CLASSIFICATION], &classification_value) &&
            get_header_value(headers[TM_EVENT_ID], &event_id_value)) {
                if (new_journal_entry(daemon->record_journal, classification_value, t_stamp, event_id_value) != 0) {
                        telem_log(LOG_INFO, "new_journal_entry in process_record: failed saving record entry\n");
                }
        }
        free(classification_value);
        free(event_id_value);
}

/* Check window length conf values */
static bool windows_length_value_check(TelemPostDaemon *daemon)
{
        if (daemon->record_window_length == -1 ||
            daemon->byte_window_length == -1) {
                telem_log(LOG_ERR, "Invalid value for window length\n");
                return false;
        }

        return true;
}

/* Rate limiting checks */
static void rate_limit_checks(TelemPostDaemon *daemon, bool *record_check_passed, bool *byte_check_passed)
{
        time_t temp = time(NULL);
        struct tm *tm_s = localtime(&temp);
        int current_minute;
        current_minute = tm_s->tm_min;
        bool record_burst_enabled = burst_limit_enabled(daemon->record_burst_limit);
        bool byte_burst_enabled =  burst_limit_enabled(daemon->byte_burst_limit);

        /* Checks if entirety of rate limiting is enabled */
        if (daemon->rate_limit_enabled) {
                /* Checks whether record and byte bursts are enabled individually */
                record_burst_enabled = burst_limit_enabled(daemon->record_burst_limit);
                byte_burst_enabled = burst_limit_enabled(daemon->byte_burst_limit);

                if (record_burst_enabled) {
                        *record_check_passed = rate_limit_check(current_minute,
                                                                daemon->record_burst_limit, daemon->record_window_length,
                                                                daemon->record_burst_array, TM_RECORD_COUNTER);
                }
                if (byte_burst_enabled) {
                        *byte_check_passed = rate_limit_check(current_minute,
                                                              daemon->byte_burst_limit, daemon->byte_window_length,
                                                              daemon->byte_burst_array, RECORD_SIZE_LEN);
                }
                /* If both record and byte burst disabled, rate limiting disabled */
                if (!record_burst_enabled && !byte_burst_enabled) {
                        daemon->rate_limit_enabled = false;
                }
        }

}

/* Wrapper for save local copy */
static void apply_retention_policies(TelemPostDaemon *daemon, char *body)
{
        if (daemon->record_retention_enabled) {
                save_local_copy(daemon, body);
        }
}

/* Deliver record to backend if rate limiting policies are met otherwise
 * spool record for future delivery */
static bool deliver_record(TelemPostDaemon *daemon, char *headers[], char *body, char* cfg_file)
{

        bool ret = false;
        int current_minute;
        bool do_spool = false;
        bool record_sent = false;
        time_t temp = time(NULL);
        struct tm *tm_s = localtime(&temp);
        current_minute = tm_s->tm_min;
        /* Checks flags */
        bool record_check_passed = true;
        bool byte_check_passed = true;
        bool record_burst_enabled = burst_limit_enabled(daemon->record_burst_limit);
        bool byte_burst_enabled =  burst_limit_enabled(daemon->byte_burst_limit);

        /* Perform record and byte rate limiting checks */
        rate_limit_checks(daemon, &record_check_passed, &byte_check_passed);

        /* Sends record if rate limiting is disabled, or all checks passed */
        if (!daemon->rate_limit_enabled || (record_check_passed && byte_check_passed)) {
                /* Send the record as https post */
                record_sent = post_record_ptr(headers, body, cfg_file);
                /**
                 * This is the only point where an error condition could be returned
                 * if the record was not sent
                 * */
                ret = record_sent;
        }
        // Get rate-limit strategy
        do_spool = spool_strategy_selected(daemon);

        // Drop record
        if (!record_sent && !do_spool) {
                // Not an error condition
                ret = true;
        }
        // Spool Record
        else if (!record_sent && do_spool) {
                start_network_bypass(daemon);
                telem_log(LOG_INFO, "process_record: initializing direct-spool window\n");
                // False will keep record around
                ret = false;
        } else {
                /* Updates rate limiting arrays if record sent */
                if (record_burst_enabled) {
                        rate_limit_update(current_minute, daemon->record_window_length,
                                          daemon->record_burst_array, TM_RECORD_COUNTER);
                }
                if (byte_burst_enabled) {
                        rate_limit_update(current_minute, daemon->byte_window_length,
                                          daemon->byte_burst_array, RECORD_SIZE_LEN);
                }
        }

        return ret;
}

bool process_staged_record(char *filename, TelemPostDaemon *daemon)
{
        int k;
        bool ret = false;
        char *headers[NUM_HEADERS];
        char *body = NULL;
        struct stat buf = { 0 };
        time_t current_time = time(NULL);
        int64_t max_spool_size = 0;
        char *cfg_file = NULL;

        for (k = 0; k < NUM_HEADERS; k++) {
                headers[k] = NULL;
        }

        /** Load record **/
        if ((ret = read_record(filename, headers, &body, &cfg_file)) == false) {
                telem_log(LOG_WARNING, "unable to read record\n");
                ret = true; // Record corrupted? true will remove record
                goto end_processing_file;
        }

        /** Get file information  **/
        if (stat(filename, &buf) == -1) {
                telem_perror("Processing staged file unable to stat record in spool");
                ret = true; // true to remove it
                goto end_processing_file;
        }

        /** Update spool directory size **/
        daemon->current_spool_size += (buf.st_blocks * 512);

        /** Check that record is not expired **/
        if (!S_ISREG(buf.st_mode) ||
            (current_time - buf.st_mtime > (record_expiry_config() * 60)) ||
            (buf.st_uid  != getuid())) {
                ret = true; // Expired, true to remove it
                goto end_processing_file;
        }

        /** Record delivery **/
        if (!daemon->record_server_delivery_enabled) {
                telem_log(LOG_INFO, "record server delivery disabled\n");
                // Not an error condition
                ret = true;
                goto end_record_delivery;
        }

        /** Spool policies **/
        if (inside_direct_spool_window(daemon, time(NULL))) {
                telem_log(LOG_INFO, "process_record: delivering directly to spool\n");
                /* Check spool max size conf */
                max_spool_size = spool_max_size_config();
                if (max_spool_size != -1 &&
                    daemon->current_spool_size >= (max_spool_size * 1024)) {
                        // Drop record
                        telem_log(LOG_INFO, "Spool dir full, dropping record\n");
                        ret = true;
                } else {
                        // Keep record, non error condition
                        ret = false;
                }
                goto end_processing_file;
        }

        /** Check window_length **/
        if (windows_length_value_check(daemon) == false) {
                exit(EXIT_FAILURE);
        }

        /** Deliver or spool **/
        ret = deliver_record(daemon, headers, body, cfg_file);

end_record_delivery:
        /** Save record once it is properly delivered, if record
         *  is spooled the record is not saved to journal until
         *  delievered on a re-try **/
        if (ret) {
                /** Save to journal **/
                save_entry_to_journal(daemon, current_time, headers);
                /** Record retention **/
                apply_retention_policies(daemon, body);
        }

end_processing_file:
        /** Update spool size if record will be removed **/
        if (ret) {
                daemon->current_spool_size -= (buf.st_blocks * 512);
        }
        telem_log(LOG_DEBUG, "spool_size: %ld\n", daemon->current_spool_size);
        free(body);

        for (k = 0; k < NUM_HEADERS; k++) {
                free(headers[k]);
        }

        if (cfg_file != NULL) {
                free(cfg_file);
        }
        return ret;
}

static int directory_dot_filter(const struct dirent *entry)
{
        if ((strcmp(entry->d_name, ".") == 0) || (strcmp(entry->d_name, "..") == 0)) {
                return 0;
        } else {
                return 1;
        }
}

int staging_records_loop(TelemPostDaemon *daemon)
{
        int ret;
        int processed;
        int numentries;
        struct dirent **namelist;

        numentries = scandir(spool_dir_config(), &namelist, directory_dot_filter, NULL);
        processed = 0;

        if (numentries == 0) {
                telem_log(LOG_DEBUG, "No entries in staging\n");
                return numentries;
        } else if (numentries < 0) {
                telem_perror("Error while scanning staging");
                return numentries;
        }

        for (int i = 0; i < numentries; i++) {
                char *record_path;
                telem_log(LOG_DEBUG, "Processing staged record: %s\n",
                          namelist[i]->d_name);
                ret = asprintf(&record_path, "%s/%s", spool_dir_config(), namelist[i]->d_name);
                if (ret == -1) {
                        telem_log(LOG_ERR, "Failed to allocate memory for staging record full path\n");
                        exit(EXIT_FAILURE);
                }
                if (process_staged_record(record_path, daemon)) {
                        unlink(record_path);
                        processed++;
                }
                free(record_path);
        }

        for (int i = 0; i < numentries; i++) {
                free(namelist[i]);
        }
        free(namelist);

        return numentries - processed;
}

void run_daemon(TelemPostDaemon *daemon)
{
        int ret;
        /* retry_attempt of zero indicates we don't need to retry */
        int retry_attempt = 0;
        int spool_process_time = spool_process_time_config();
        bool daemon_recycling_enabled = daemon_recycling_enabled_config();
        time_t last_spool_run_time = time(NULL);
        time_t last_record_received = time(NULL);

        assert(daemon);
        assert(daemon->pollfds);
        assert(daemon->pollfds[signlfd].fd);
        assert(daemon->pollfds[watchfd].fd);

        /* If we failed to send spooled records, indicate we need to retry */
        if (daemon->bypass_http_post_ts != 0) {
                retry_attempt = 1;
        }

        while (1) {
                int retry_delay = spool_process_time;
                malloc_trim(0);

                /* check if we need to retry sending spooled records */
                if (retry_attempt > 0) {
                        retry_delay = retry_attempt * retry_attempt;
                        daemon->bypass_http_post_ts = 0;
                        telem_log(LOG_INFO, "Record delivery failed will retry in %d seconds",
                                  retry_delay);
                }

                ret = poll(daemon->pollfds, NFDS, retry_delay * 1000);
                if (ret == -1) {
                        telem_perror("Failed to poll daemon file descriptors");
                        break;
                } else if (ret != 0) {

                        if (daemon->pollfds[signlfd].revents != 0) {
                                struct signalfd_siginfo fdsi;
                                ssize_t s;
                                s = read(daemon->sfd, &fdsi, sizeof(struct signalfd_siginfo));
                                if (s != sizeof(struct signalfd_siginfo)) {
                                        telem_perror("Error while reading from the signal"
                                                     "file descriptor");
                                        exit(EXIT_FAILURE);
                                }

                                if (fdsi.ssi_signo == SIGTERM || fdsi.ssi_signo == SIGINT) {
                                        telem_log(LOG_INFO, "Received either a \
                                                                     SIGINT/SIGTERM signal\n");
                                        break;
                                }
                        } else if (daemon->pollfds[watchfd].revents != 0) {

                                int ret = 0;
                                ssize_t i = 0;
                                ssize_t length = 0;
                                char buffer[BUFFER_LEN];

                                length = read(daemon->fd, buffer, BUFFER_LEN);
                                if (length < 0) {
                                        telem_perror("Error while reading from inotify"
                                                     "watcher");
                                        exit(EXIT_FAILURE);
                                }

                                while (i < length) {
                                        struct inotify_event *event = (struct inotify_event *)&buffer[i];

                                        if (event->len) {
                                                if (event->mask & IN_CLOSE_WRITE && !(event->mask & IN_ISDIR)) {
                                                        char *record_name = NULL;

                                                        /* Retrieve foldername from watch id?  */
                                                        ret = asprintf(&record_name, "%s/%s", spool_dir_config(), event->name);
                                                        if (ret == -1) {
                                                                telem_log(LOG_ERR, "Failed to allocate memory for record full path, aborting\n");
                                                                exit(EXIT_FAILURE);
                                                        }
                                                        /* Process inotify event */
                                                        if (process_staged_record(record_name, daemon)) {
                                                                unlink(record_name);
                                                        }
                                                        free(record_name);
                                                        last_record_received = time(NULL);
                                                }
                                        }

                                        i += (ssize_t)EVENT_SIZE + event->len;
                                }
                        }
                } else {
                        time_t now = time(NULL);
                        /* time to recycle the daemon has elapsed*/
                        if (daemon_recycling_enabled &&
                            difftime(now, last_record_received) >= TM_DAEMON_EXIT_TIME) {
                                /* Exit */
                                telem_log(LOG_INFO, "Telemetry post daemon exiting for recycling\n");
                                break;
                        }

                        /* Check if this was a retry attempt */
                        if (retry_attempt > 0) {
                                /* Stop attempting retries if successful, increment counter if not */
                                if (staging_records_loop(daemon) == 0) {
                                        retry_attempt = 0;
                                } else {
                                        retry_attempt++;
                                }
                                /* Give up if counter reaches MAX_RETRY_ATTEMPTS */
                                if (retry_attempt == MAX_RETRY_ATTEMPTS) {
                                        telem_log(LOG_ERR, "Record deliver failed after %d attempts",
                                                  MAX_RETRY_ATTEMPTS);
                                        retry_attempt = 0;
                                }
                        }

                        /* Check spool  */
                        if (difftime(now, last_spool_run_time) >= spool_process_time) {
                                spool_records_loop(&(daemon->current_spool_size));
                                last_spool_run_time = time(NULL);
                        }
                }

                /* Check journal records and prune if needed */
                ret = prune_journal(daemon->record_journal, JOURNAL_TMPDIR);
                if (ret != 0) {
                        telem_log(LOG_WARNING, "Unable to prune journal\n");
                }
        }
}

void close_daemon(TelemPostDaemon *daemon)
{

        if (daemon->fd) {
                if (daemon->wd) {
                        inotify_rm_watch(daemon->fd, daemon->wd);
                }
                close(daemon->fd);
        }

        close_journal(daemon->record_journal);
}

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
