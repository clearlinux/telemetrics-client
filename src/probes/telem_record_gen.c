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
#include <ctype.h>
#include <getopt.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string.h>
#include <stdint.h>

#include "config.h"
#include "log.h"
#include "common.h"
#include "telemetry.h"

static char *config_file = NULL;
static uint32_t severity = 1;
static char *opt_class = NULL;
static char *opt_payload = NULL;
static uint32_t payload_version = 1;
static char *opt_payload_file = NULL;
static char *opt_event_id = NULL;

static const struct option prog_opts[] = {
        { "help", no_argument, 0, 'h' },
        { "config-file", required_argument, 0, 'f' },
        { "version", no_argument, 0, 'V' },
        { "severity", required_argument, 0, 's' },
        { "class", required_argument, 0, 'c' },
        { "payload", required_argument, 0, 'p' },
        { "payload-file", required_argument, 0, 'P' },
        { "record-version", required_argument, 0, 'R' },
        { "event-id", required_argument, 0, 'e' },
        { 0, 0, 0, 0 }
};

static void print_help(void)
{
        printf("Usage:\n");
        printf("  telem-record-gen [OPTIONS] - create and send a custom telemetry record\n");
        printf("\n");
        printf("Help Options:\n");
        printf("  -h, --help            Show help options\n");
        printf("\n");
        printf("Application Options:\n");
        printf("  -f, --config-file     Path to configuration file (not implemented yet)\n");
        printf("  -V, --version         Print the program version\n");
        printf("  -s, --severity        Severity level (1-4) - (default 1)\n");
        printf("  -c, --class           Classification level_1/level_2/level_3\n");
        printf("  -p, --payload         Record body (max size = 8k)\n");
        printf("  -P, --payload-file    File to read payload from\n");
        printf("  -R, --record-version  Version number for format of payload (default 1)\n");
        printf("  -e, --event-id        Event id to use in the record\n");
        printf("\n");
}

const unsigned int count_chars(const char *check, const char character)
{
        unsigned int count = 0U;
        if (check == NULL) {
                return count;
        }
        for (; *check != '\0'; ++check) {
                if (*check == character) {
                        ++count;
                }
        }
        return count;
}

int parse_options(int argc, char **argv)
{
        int ret = 0;
        char *endptr = NULL;
        long unsigned int tmp = 0;

        int opt;
        while ((opt = getopt_long(argc, argv, "hc:Vs:c:p:P:R:e:", prog_opts, NULL)) != -1) {
                switch (opt) {
                        case 'h':
                                print_help();
                                exit(EXIT_SUCCESS);
                        case 'V':
                                printf(PACKAGE_VERSION "\n");
                                exit(EXIT_SUCCESS);
                        case 's':
                                errno = 0;

                                tmp = strtoul(optarg, &endptr, 10);

                                if (errno != 0) {
                                        telem_perror("Failed to convert severity number");
                                        goto fail;
                                }
                                if (endptr && *endptr != '\0') {
                                        telem_log(LOG_ERR, "Invalid severity number. Must be an integer\n");
                                        goto fail;
                                }

                                severity = (uint32_t)tmp;
                                break;
                        case 'c':
                                opt_class = strdup(optarg);
                                break;
                        case 'p':
                                opt_payload = strdup(optarg);
                                break;
                        case 'P':
                                opt_payload_file = strdup(optarg);
                                break;
                        case 'R':
                                errno = 0;

                                tmp = strtoul(optarg, &endptr, 10);

                                if (errno != 0) {
                                        telem_perror("Failed to convert record-version number");
                                        goto fail;
                                }
                                if (endptr && *endptr != '\0') {
                                        telem_log(LOG_ERR, "Invalid record-version number. Must be an integer\n");
                                        goto fail;
                                }

                                payload_version = (uint32_t)tmp;
                                break;
                        case 'e':
                                opt_event_id = strdup(optarg);
                                if (opt_event_id == NULL) {
                                        goto fail;
                                }
                                break;
                }
        }

        ret = 1;
fail:
        return ret;
}

int validate_opts(void)
{
        size_t len;
        int ret = 0;
        const char alphab[] = EVENT_ID_ALPHAB;

        /* classification */
        if (opt_class == NULL) {
                fprintf(stderr, "Error: Classification required. See --help.\n");
                return ret;

        }
        len = strlen(opt_class);

        if ((len == 0) || (len > 120)) {
                fprintf(stderr, "Error: Valid size for classification "
                        "is 1-120 chars\n");
                return ret;
        }

        for (int c = 0; c < len; c++) {
                if (isascii(opt_class[c]) == 0) {
                        fprintf(stderr, "Error: Non-ascii characters detected "
                                "in classification - aborting\n");
                        return ret;
                }
        }

        if (count_chars(opt_class, '/') != 2) {
                fprintf(stderr, "Error: Classification needs to be in "
                        "most/to/least specific format, 2 \'/\' required.\n");
                return ret;
        }

        /* Severity */
        if ((severity) < 1 || (severity > 4)) {
                fprintf(stderr, "Error: Valid range for severity is 1-4\n");
                return ret;
        }

        /* Event id  */
        if (opt_event_id) {
                size_t result = 0;
                if ((result = strlen(opt_event_id)) != EVENT_ID_LEN) {
                        fprintf(stderr, "Error: event_id length %zu it should have %d characters\n", result, EVENT_ID_LEN);
                        return ret;
                }
                if ((result = strspn(opt_event_id, alphab)) != EVENT_ID_LEN) {
                        fprintf(stderr, "Error: event_id contains invalid character '%c' at position %zu, valid characters are %s\n",
                                opt_event_id[result], result, alphab);
                        return ret;
                }
        }

        return 1;
}

int allocate_payload_buffer(char **payload)
{
        int ret = 0;

        *payload = (char *)malloc(MAX_PAYLOAD_SIZE);

        if (*payload == NULL) {
                goto out1;
        }

        *payload = memset(*payload, 0, MAX_PAYLOAD_SIZE);
        ret = 1;

out1:
        return ret;
}

int get_payload_from_file(char **payload)
{
        int ret = 1;
        FILE *fp = NULL;
        size_t bytes_in = 0;

        fp = fopen(opt_payload_file, "r");

        if (!fp) {
                fprintf(stderr, "Error: file %s could not be opened "
                        "for reading\n", opt_payload_file);
                goto out;
        }

        bytes_in = fread(*payload, 1,
                         MAX_PAYLOAD_SIZE - 1, fp);

        /* if fread fails */
        if (bytes_in == 0 && ferror(fp) != 0) {
                ret = 0;
                goto out;
        }

out:
        if (fp) {
                fclose(fp);
        }
        return ret;

}

void get_payload_from_opt(char **payload)
{
        size_t len = 0;

        len = strlen(opt_payload);
        if (len >= MAX_PAYLOAD_SIZE) {
                len = MAX_PAYLOAD_SIZE - 1;
        }
        strncpy(*payload, opt_payload, len);
}

void get_payload_from_stdin(char **payload)
{
        size_t bytes_in = 0;
        int c;

        bytes_in = fread(*payload, 1, MAX_PAYLOAD_SIZE - 1, stdin);

        if (bytes_in == MAX_PAYLOAD_SIZE - 1) {
                /* Throw away the rest of stdin */
                while (EOF != (c = fgetc(stdin))) {
                        continue;
                }
        }

}

int get_payload(char **payload)
{
        int ret = 0;

        if (!allocate_payload_buffer(payload)) {
                goto out1;
        }

        if (opt_payload_file) {

                if (get_payload_from_file(payload)) {
                        ret = 1;
                }

        } else if (opt_payload) {

                get_payload_from_opt(payload);
                ret = 1;

        } else {
                get_payload_from_stdin(payload);
                ret = 1;
        }

        if (ret == 0) {
                free(*payload);
                *payload = NULL;
        }

out1:
        return ret;
}

int send_record(char *payload)
{
        struct telem_ref *t_ref;
        int ret = 0;

        if (tm_create_record(&t_ref, (uint32_t)severity,
                             opt_class, payload_version) < 0) {
                goto out1;
        }

        if (opt_event_id && tm_set_event_id(t_ref, opt_event_id) < 0) {
                goto out2;
        }

        if (tm_set_payload(t_ref, payload) < 0) {
                goto out2;
        }

        if (tm_send_record(t_ref) >= 0) {
                ret = 1;
        }

out2:
        tm_free_record(t_ref);
out1:
        return ret;
}

int main(int argc, char **argv)
{
        int ret = EXIT_FAILURE;
        char *payload = NULL;

        if (!parse_options(argc, argv)) {
                goto fail;
        }

        if (!validate_opts()) {
                goto fail;
        }

        if (!get_payload(&payload)) {
                goto fail;
        }

        if (!send_record(payload)) {
                goto fail;
        }

        ret = EXIT_SUCCESS;
fail:
        free(config_file);
        free(opt_class);
        free(opt_payload);
        free(opt_event_id);

        return ret;
}

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
