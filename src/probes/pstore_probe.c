/*
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
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <inttypes.h>

#include "log.h"
#include "telemetry.h"
#include "oops_parser.h"
#include "nica/hashmap.h"

char *pstore_dump_path = PSTOREDIR;

static uint32_t version = 1;

static bool send_data(char *backtrace, char *class, uint32_t severity)
{
        struct telem_ref *handle = NULL;
        int ret;

        if ((ret = tm_create_record(&handle, severity, class, version)) < 0) {
                telem_log(LOG_ERR, "Failed to create record: %s",
                          strerror(-ret));
                return false;
        }

        if ((ret = tm_set_payload(handle, backtrace)) < 0) {
                telem_log(LOG_ERR, "Failed to set payload: %s", strerror(-ret));
                tm_free_record(handle);
                return false;
        }

        if ((ret = tm_send_record(handle)) < 0) {
                telem_log(LOG_ERR, "Failed to send record: %s", strerror(-ret));
                tm_free_record(handle);
                return false;
        }

        tm_free_record(handle);

        return true;
}

void handle_complete_oops_message(struct oops_log_msg *msg)
{
        nc_string *payload;

#ifdef DEBUG
        for (int i = 0; i < msg->length; i++) {
                printf("%s\n", msg->lines[i]);
        }
#endif
        payload = parse_payload(msg);

#ifdef DEBUG
        printf("Payload Parsed :%s\n", payload->str);
#endif
        send_data(payload->str, (char *)msg->pattern->classification, (uint32_t)msg->pattern->severity);
        nc_string_free(payload);
}

void handle_crash_dump(char *dump, size_t size)
{
        char *lnend;
        long sz = (long)size;

        oops_parser_init(handle_complete_oops_message);
        while (sz > 0) {
                lnend = memchr(dump, '\n', (size_t)sz);
                if (lnend != NULL) {
                        *lnend = '\0';
                        parse_single_line(dump, (size_t)(lnend - dump));
                        lnend++;
                        sz -= (lnend - dump);
                        dump = lnend;
                }
        }
        oops_parser_cleanup();

        return;
}

char *read_contents(char *filename, size_t *bytes)
{
        char *dump_file = NULL;
        FILE *fp = NULL;
        char *contents = NULL;
        long sz = 0;
        size_t size, bytes_read;
        char *hdr_end;
        size_t hdr_len;

        if (asprintf(&dump_file, "%s/%s", pstore_dump_path, filename) < 0) {
                goto end;
        }

        fp = fopen(dump_file, "r");
        if (fp == NULL) {
                telem_log(LOG_ERR, "Failed to open pstore dump file %s:%s\n", dump_file, strerror(errno));
                goto end;
        }

        if (fseek(fp, 0, SEEK_END) < 0) {
                telem_log(LOG_ERR, "Fseek failed for file %s:%s\n", dump_file, strerror(errno));
                goto end;
        }

        sz = ftell(fp);
        if (sz == -1L) {
                telem_perror("ftell failed");
                size = 0;
                goto end;
        }
        size = (size_t)sz;

        if (fseek(fp, 0, SEEK_SET) < 0) {
                telem_log(LOG_ERR, "Fseek failed for file %s:%s\n", dump_file, strerror(errno));
                goto end;
        }

        contents = malloc(size);
        if (!contents) {
                telem_log(LOG_ERR, "Call to malloc failed");
                goto end;
        }
        memset(contents, 0, size);

        bytes_read = fread(contents, sizeof(char), size, fp);

        if (bytes_read < size) {
                telem_log(LOG_ERR, "Error while reading file %s:%s\n", dump_file, strerror(errno));
                free(contents);
                contents = NULL;
                goto end;
        }

        hdr_end = memchr(contents, '\n', size);
        if (hdr_end) {
                //*hdr_end = '\0';
                // TODO: Check for valid header in the format Oops#1 Part4
                hdr_len = (size_t)(hdr_end + 1 - contents);
                memmove(contents, hdr_end + 1, size - hdr_len);
                size -= hdr_len;
        } else {
                // TODO: Handle this case
        }

        *bytes = size;
end:
        //Delete to file to recover space
        if (unlink(dump_file) == -1) {
                telem_log(LOG_ERR, "Failed to unlink file %s: %s\n", dump_file, strerror(errno));
        }
        if (fp) {
                fclose(fp);
        }

        free(dump_file);
        return contents;
}

/*
 * Ids are part of the file name.
 * Format : dmesg-efi-143994262108001
 * return (timestamp * 100 + part) * 1000 + count
 */
void parse_id(uint64_t id, int *count, int *part)
{
        *count = (int)(id % 1000);
        id /= 1000;
        *part = (int)(id % 100);
}

struct chunk_list {
        char *contents;
        int count;
        int part;
        size_t size;
        struct chunk_list *next;
};

int main(int argc, char **argv)
{
        DIR *pstore_dir;
        struct dirent *entry;
        struct chunk_list  *elem, *head;
        NcHashmapIter iter;
        void *key, *value;
        int max_part;
        size_t totalsize = 0;
        char *crash_dump = NULL;

        /*
         * Hash table used to store chunks belonging to a oops counter
         *  key -> count, value->head of linked list of chunks
         */
        NcHashmap *hash = nc_hashmap_new(nc_simple_hash, nc_simple_compare);

        pstore_dir = opendir(pstore_dump_path);
        if (pstore_dir == NULL) {
                telem_perror("Could not open pstore dump directory");
                exit(EXIT_FAILURE);
        }

        while ((entry = readdir(pstore_dir)) != NULL) {
                // Process only crash files. Skip . and ..
                if (entry->d_type != DT_REG) {
                        continue;
                }

                int part = 0, count = 0;
                uint64_t id = 0;

                // Look for only dmesg logs. Ignore other types of pstore dumps for now.
                if (sscanf(entry->d_name, "dmesg-efi-%" PRIu64, &id) == 1) {
                        parse_id(id, &count, &part);
                        #ifdef DEBUG
                        printf("Extracted count :%d, part : %d\n", count, part);
                        #endif
                } else {
                        continue;
                }

                elem = malloc(sizeof(*elem));
                if (!elem) {
                        exit(EXIT_FAILURE);
                }

                elem->part = part - 1;
                elem->count = count - 1;
                elem->size = 0;
                elem->contents = read_contents(entry->d_name, &(elem->size));
                elem->next = NULL;

                void *head = nc_hashmap_get(hash, NC_HASH_KEY(elem->count));
                if (head != NULL) {
                        elem->next = (struct chunk_list *)head;
                }

                nc_hashmap_put(hash, NC_HASH_KEY(elem->count), elem);
        }

        closedir(pstore_dir);

        nc_hashmap_iter_init(hash, &iter);
        while (nc_hashmap_iter_next(&iter, (void **)&key, (void **)&value)) {
                #ifdef DEBUG
                printf("Count in the hash: %d\n", NC_UNHASH_KEY(key));
                #endif

                head = (struct chunk_list *)value;
                elem = head;
                max_part = -1;
                totalsize = 0;

                while (elem) {
                        if (elem->part > max_part) {
                                max_part = elem->part;
                        }
                        totalsize += elem->size;
                        elem = elem->next;
                }

                assert(max_part > 0);

                // Make a contiguous array of chunks with the chunks ordered
                struct chunk_list **parts = (struct chunk_list **)malloc(sizeof(*parts) * ((size_t)max_part + 1));
                if (!parts) {
                        exit(EXIT_FAILURE);
                }
                memset(parts, 0, sizeof(*parts) * (size_t)(max_part + 1));

                // Copy chunks in their correct position
                for (elem = head; elem; elem = elem->next) {
                        parts[elem->part] = elem;
                }

                /*
                 *  Extra step to check if all chunks are present. Else give up and move to
                 * the next oops.
                 */
                int part;
                for (part = 0; part <= max_part; part++) {
                        elem = parts[part];
                        if (elem == NULL || elem->size == 0) {
                                /* Parts are missing */
                                telem_log(LOG_DEBUG, "Parts are missing from pstore\n");
                                break;
                        }
                }

                if (part < (max_part + 1)) {
                        free(parts);
                        continue;
                }

                /* Combine all the fragments to get a contiguous log message */
                crash_dump = malloc(totalsize);
                if (crash_dump == NULL) {
                        exit(EXIT_FAILURE);
                }

                size_t offset = 0;
                for (int part = max_part; part >= 0; part--) {
                        elem = parts[part];
                        memcpy(crash_dump + offset, elem->contents, elem->size);
                        offset += elem->size;
                }

                handle_crash_dump(crash_dump, totalsize);
                free(crash_dump);
                free(parts);
        }
        return 0;
}
