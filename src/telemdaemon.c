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

#include "iorecord.h"
#include "telemdaemon.h"
#include "common.h"
#include "util.h"
#include "log.h"
#include "configuration.h"

void initialize_probe_daemon(TelemDaemon *daemon)
{
        client_list_head head;
        LIST_INIT(&head);
        daemon->nfds = 0;
        daemon->pollfds = NULL;
        daemon->client_head = head;
        daemon->machine_id_override = NULL;
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

static void stage_record(char *filepath, char *headers[], char *body, char *cfg_file)
{
        int tmpfd;
        FILE *tmpfile = NULL;

#ifdef DEBUG
        fprintf(stderr, "DEBUG: [%s] filepath:%s\n",__func__, filepath);
        fprintf(stderr, "DEBUG: [%s] body:%s\n",__func__, body);
        fprintf(stderr, "DEBUG: [%s] cfg:%s\n",__func__, cfg_file);
#endif
        // Use default path if not provided
        if (filepath == NULL) {
                telem_log(LOG_ERR, "filepath value must be provided, aborting\n");
                exit(EXIT_FAILURE);
        }

        tmpfd = mkstemp(filepath);
        if (!tmpfd) {
                telem_perror("Error opening staging file");
                close(tmpfd);
                if (unlink(filepath)) {
                        telem_perror("Error deleting staging file");
                }
                goto clean_exit;
        }

        // open file
        tmpfile = fdopen(tmpfd, "a");
        if (!tmpfile) {
                telem_perror("Error opening temp stage file");
                close(tmpfd);
                if (unlink(filepath)) {
                        telem_perror("Error deleting temp stage file");
                }
                goto clean_exit;
        }

        // write cfg info if exists
        if (cfg_file != NULL) {
                fprintf(tmpfile, "%s%s\n", CFG_PREFIX, cfg_file);
        }

        // write headers
        for (int i = 0; i < NUM_HEADERS; i++) {
                fprintf(tmpfile, "%s\n", headers[i]);
        }

        //write body
        fprintf(tmpfile, "%s\n", body);
        fflush(tmpfile);
        fclose(tmpfile);

clean_exit:

        return;
}

void process_record(TelemDaemon *daemon, client *cl)
{
        int i = 0;
        int ret = 0;
        char *headers[NUM_HEADERS];
        char *tok = NULL;
        size_t header_size = 0;
        size_t message_size = 0;
        char *temp_headers = NULL;
        char *msg;
        char *body;
        char *recordpath = NULL;
        char *cfg_file = NULL;;
        size_t cfg_info_size = 0;
        uint8_t *buf;

        buf = cl->buf;

        /* Check for an optional CFG_PREFIX in the first 32 bits */
        if (*(uint32_t *)buf == CFG_PREFIX_32BIT) {
                char *cfg  = (char *)cl->buf;

                cfg_file = cfg + CFG_PREFIX_LENGTH;
                cfg_info_size = CFG_PREFIX_LENGTH + strlen(cfg_file) + 1;
#ifdef DEBUG
                fprintf(stderr, "DEBUG: [%s] cfg_file: %s\n", __func__, cfg_file);
#endif
        }

        buf += cfg_info_size;
        header_size = *(uint32_t *)buf;
        message_size = cl->size - (cfg_info_size + header_size);
#ifdef DEBUG
        fprintf(stderr, "DEBUG: [%s] cl->size: %zu\n", __func__, cl->size);
        fprintf(stderr, "DEBUG: [%s] header_size: %zu\n", __func__, header_size);
        fprintf(stderr, "DEBUG: [%s] message_size: %zu\n", __func__, message_size);
        fprintf(stderr, "DEBUG: [%s] cfg_info_size: %zu\n", __func__, cfg_info_size);
#endif
        assert(message_size > 0);      //TODO:Check for min and max limits
        msg = (char *)buf + sizeof(uint32_t);

        /* Copying the headers as strtok modifies the orginal buffer */
        temp_headers = strndup(msg, header_size);
        tok = strtok(temp_headers, "\n");

        for (i = 0; i < NUM_HEADERS; i++) {
                const char *header_name = get_header_name(i);

                if (get_header(tok, header_name, &headers[i])) {
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

        /* Save record to stage */
        ret = asprintf(&recordpath, "%s/XXXXXX", spool_dir_config());
        if (ret == -1) {
                telem_log(LOG_ERR, "Failed to allocate memory for record name in staging folder, aborting\n");
                exit(EXIT_FAILURE);
        }

        stage_record(recordpath, headers, body, cfg_file);
        free(recordpath);
end:
        free(temp_headers);
        for (int k = 0; k < i; k++)
                free(headers[k]);
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

int machine_id_write(char *new_id)
{
        FILE *fp;
        int ret = 0;
        int result = -1;

        fp = fopen(TM_MACHINE_ID_FILE, "w");
        if (fp == NULL) {
                telem_perror("Could not open machine id file");
                goto file_error;
        }

        ret = fprintf(fp, "%s", new_id);
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
        int result = 0;
        char *new_id = NULL;

        if ((result = get_random_id(&new_id)) != 0) {
                goto rand_err;
        }

        result = machine_id_write(new_id);
rand_err:
        free(new_id);

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
