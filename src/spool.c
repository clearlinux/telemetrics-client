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
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#include <stdbool.h>
#include <errno.h>

#include "spool.h"
#include "telemdaemon.h"
#include "log.h"
#include "configuration.h"
#include "util.h"
#include "common.h"

int directory_filter(const struct dirent *entry)
{
        if ((strcmp(entry->d_name, ".") == 0) || (strcmp(entry->d_name, "..") == 0)) {
                return 0;
        } else {
                return 1;
        }
}

bool is_spool_valid()
{
        char spool_dir[PATH_MAX] = { 0 };
        int ret;
        struct stat buf;

        strncpy(spool_dir, spool_dir_config(), PATH_MAX - 1);

        /* Need to check for buffer for appending file name later */
        if (strlen(spool_dir) > (PATH_MAX - 7)) {
                telem_log(LOG_ERR, "Spool directory path too long\n");
                return false;
        }

        ret = stat(spool_dir, &buf);

        if (ret || !S_ISDIR(buf.st_mode)) {
                telem_log(LOG_ERR, "Spool directory not valid\n");
                return false;
        }

        // Check if the spool dir is writable
        if (access(spool_dir, W_OK) != 0) {
                telem_log(LOG_ERR, "Spool dir is not writable\n");
                return false;
        }

        return true;
}

long get_spool_dir_size()
{
        long dir_size = get_directory_size(spool_dir_config());
        if (dir_size == -ENOMEM) {
                exit(EXIT_FAILURE);
        }
        return dir_size;
}

void spool_records_loop(long *current_spool_size)
{
        const char *spool_dir_path;
        int numentries;
        struct dirent **namelist;
        int records_processed = 0;
        int records_sent = 0;

        spool_dir_path = spool_dir_config();
        numentries = scandir(spool_dir_path, &namelist, directory_filter, NULL);

        if (numentries == 0) {
                telem_log(LOG_DEBUG, "No entries in spool\n");
                return;
        } else if (numentries < 0) {
                telem_perror("Error while scanning spool");
                return;
        }

        qsort_r(namelist, (size_t)numentries, sizeof(struct dirent *),
                spool_record_compare, (void *)spool_dir_path);

        for (int i = 0; i < numentries; i++) {
                telem_log(LOG_DEBUG, "Processing spool record: %s\n",
                          namelist[i]->d_name);
                process_spooled_record(spool_dir_path, namelist[i]->d_name,
                                       &records_processed, &records_sent, current_spool_size);

                /* If the first send attempt fails, we assume that future send
                 * attempts may also fail, so abort early.
                 */
                if (records_sent == 0) {
                        break;
                }

                if (records_processed == TM_SPOOL_MAX_PROCESS_RECORDS) {
                        break;
                }
        }

        for (int i = 0; i < numentries; i++) {
                free(namelist[i]);
        }
        free(namelist);
}

void process_spooled_record(const char *spool_dir, char *name,
                            int *records_processed, int *records_sent,
                            long *current_spool_size)
{
        char *record_name;
        int ret;
        struct stat buf;
        time_t current_time = time(NULL);
        bool post_succeeded = true;

        if (!strcmp(name, ".") || !strcmp(name, "..")) {
                return;
        }

        ret = asprintf(&record_name, "%s/%s", spool_dir, name);
        if (ret == -1) {
                telem_log(LOG_ERR, "Unable to allocate memory for"
                          " record name in spool, exiting\n");
                exit(EXIT_FAILURE);
        }

        (*records_processed)++;
        ret = stat(record_name, &buf);
        if (ret == -1) {
                telem_perror("Unable to stat record in spool");
                free(record_name);
                return;
        }

        /*
         * If file is a regular file , if uid is diff than process uid
         * or if mtime is greater than record expiry delete the file
         */

        if (record_expiry_config() == -1) {
                telem_log(LOG_ERR, "Invalid record expiry value\n");
                exit(EXIT_FAILURE);
        }
        if (!S_ISREG(buf.st_mode) ||
            (current_time - buf.st_mtime > (record_expiry_config() * 60)) ||
            (buf.st_uid  != getuid())) {
                unlink(record_name);
        } else if (post_succeeded && *records_sent <= TM_SPOOL_MAX_SEND_RECORDS) {
                transmit_spooled_record(record_name, &post_succeeded, buf.st_size);

                if (!post_succeeded) {
                        telem_log(LOG_DEBUG, "Unable to connect to the server\n");
                } else {
                        telem_log(LOG_DEBUG, "Spool record %s transmitted\n",
                                  record_name);
                        (*records_sent)++;

                        /* if spooled record is sent, deduct from tm_spool_dir_size */
                        if (*current_spool_size > 0) {
                                *current_spool_size -= (buf.st_blocks * 512);
                        }
                        /*
                         * If getting the directory size failed earlier due to
                         * EMFILE/ENFILE, try to calculate again.
                         * EMFILE - too many file descriptors in use by process
                         * ENFILE - too many files are open in the system
                         */
                        if (*current_spool_size < 0) {
                                *current_spool_size = get_spool_dir_size();
                        }
                }
        }
        free(record_name);
}

void transmit_spooled_record(char *record_path, bool *post_succeeded, long size)
{
        FILE *fp = NULL;
        char *headers[NUM_HEADERS];
        char *payload = NULL;
        int i, k;
        char line[LINE_MAX] = { 0 };
        long offset;

        fp = fopen(record_path, "r");
        if (fp == NULL) {
                telem_log(LOG_ERR, "Unable to open file %s in spool\n", record_path);
                return;
        }

        for (i = 0; i < NUM_HEADERS; i++) {
                const char *header_name = get_header_name(i);
                if (!fgets(line, LINE_MAX, fp)) {
                        telem_log(LOG_ERR, "Error while parsing record file\n");
                        fclose(fp);
                        return;
                }
                //Get rid of trailing newline
                strtok(line, "\n");
                if (get_header(line, header_name, &headers[i], strlen(header_name))) {
                        continue;
                } else {
                        telem_log(LOG_ERR, "transmit_spooled_record: Incorrect"
                                  " headers in record\n");
                        goto read_error;
                }
        }

        offset = ftell(fp);

        if (offset == -1) {
                telem_perror("Failed to get file offset of record payload");
                goto read_error;
        }

        size = size - offset + 1;
        payload = malloc((size_t)size);

        if (!payload) {
                telem_log(LOG_ERR, "Could not allocate memory for payload\n");
                fclose(fp);
                return;
        }
        memset(payload, 0, (size_t)size);

        //read rest of file
        size_t newlen = fread(payload, sizeof(char), (size_t)size, fp);
        if (newlen == 0) {
                telem_perror("Error reading spool file");
                goto read_error;
        }

        *post_succeeded = post_record_http(headers, payload, false);
        if (*post_succeeded) {
                unlink(record_path);
        }
read_error:
        if (payload) {
                free(payload);
        }

        if (fp) {
                fclose(fp);
        }

        for (k = 0; k < i; k++) {
                free(headers[k]);
        }
}

int spool_record_compare(const void *entrya, const void *entryb, void *path)
{
        int ret;
        struct stat statentrya, statentryb;
        char *patha, *pathb;

        const struct dirent **direntrya = (const struct dirent **)entrya;
        const struct dirent **direntryb = (const struct dirent **)entryb;

        const char *dirpath = (const char *)path;
        ret = asprintf(&patha, "%s/%s", dirpath, (*direntrya)->d_name);
        if (ret == -1) {
                telem_log(LOG_ERR, "Unable to allocate memory while sorting, exiting\n");
                exit(EXIT_FAILURE);
        }

        ret = stat(patha, &statentrya);
        if (ret == -1) {
                telem_log(LOG_ERR, "Unable to stat %s: %s\n", patha, strerror(errno));
                free(patha);
                return 0;
        }

        ret = asprintf(&pathb, "%s/%s", dirpath, (*direntryb)->d_name);
        if (ret == -1) {
                telem_log(LOG_ERR, "Unable to allocate memory while sorting, exiting\n");
                exit(EXIT_FAILURE);
        }

        ret = stat(pathb, &statentryb);
        if (ret == -1) {
                telem_log(LOG_ERR, "Unable to stat %s: %s\n", patha, strerror(errno));
                free(pathb);
                return 0;
        }

        free(patha);
        free(pathb);

        if (statentrya.st_mtime < statentryb.st_mtime) {
                return -1;
        } else if (statentrya.st_mtime > statentryb.st_mtime) {
                return 1;
        }

        return 0;

}

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
