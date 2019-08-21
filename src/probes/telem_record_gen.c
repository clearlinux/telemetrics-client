/*
 * This program is part of the Clear Linux Project
 *
 * Copyright 2019 Intel Corporation
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

static uint32_t severity = 1;
static char *opt_class = NULL;
static char *opt_payload = NULL;
static uint32_t payload_version = 1;
static char *opt_payload_file = NULL;
static char *opt_event_id = NULL;
static bool opt_echo = false;
static bool opt_nopost = false;

static const struct option prog_opts[] = {
        { "help", no_argument, 0, 'h' },
        { "echo", no_argument, 0, 'o' },
        { "no-post", no_argument, 0, 'n' },
        { "version", no_argument, 0, 'V' },
        { "severity", required_argument, 0, 's' },
        { "class", required_argument, 0, 'c' },
        { "payload", required_argument, 0, 'p' },
        { "payload-file", required_argument, 0, 'P' },
        { "record-version", required_argument, 0, 'R' },
        { "event-id", required_argument, 0, 'e' },
        { "config-file", required_argument, 0, 'f' },
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
        printf("  -V, --version         Print the program version\n");
        printf("  -s, --severity        Severity level (1-4) - (default 1)\n");
        printf("  -c, --class           Classification level_1/level_2/level_3 (required)\n");
        printf("  -p, --payload         Record body (max size = 8k) (required)\n");
        printf("  -P, --payload-file    File to read payload from\n");
        printf("  -R, --record-version  Version number for format of payload (default 1)\n");
        printf("  -e, --event-id        Event id to use in the record\n");
        printf("  -o, --echo            Echo record to stdout\n");
        printf("  -n, --no-post         Do not post record just print\n");
        printf("  -f, --config_file     Specify a configuration file other than default\n");
        printf("\n");
}

static bool parse_options(int argc, char **argv)
{
        bool ret = false;
        char *endptr = NULL;
        long unsigned int tmp = 0;

        int opt;
        while ((opt = getopt_long(argc, argv, "hc:Vs:c:p:P:R:e:onf:", prog_opts, NULL)) != -1) {
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
                                if (opt_class != NULL) {
                                    free(opt_class);
                                }
                                opt_class = strdup(optarg);
                                break;
                        case 'p':
                                if (opt_payload != NULL) {
                                    free(opt_payload);
                                }
                                opt_payload = strdup(optarg);
                                break;
                        case 'P':
                                if (opt_payload_file != NULL) {
                                    free(opt_payload_file);
                                }
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
                                if (opt_event_id != NULL) {
                                    free(opt_event_id);
                                }
                                opt_event_id = strdup(optarg);
                                if (opt_event_id == NULL) {
                                        goto fail;
                                }
                                break;
                        case 'o':
                                opt_echo = true;
                                break;
                        case 'n':
                                opt_nopost = true;
                                break;
                        case 'f':
                                if (tm_set_config_file(optarg) != 0) {
                                    telem_log(LOG_ERR, "Configuration file path not valid\n");
                                    exit(EXIT_FAILURE);
                                }
                                break;
                }
        }

        ret = true;
fail:
        return ret;
}

static bool validate_opts(void)
{
        size_t i, len;
        int x, slashes = 0;
        const char alphab[] = EVENT_ID_ALPHAB;
        bool ret = false;

        /* classification */
        if (opt_class == NULL) {
                fprintf(stderr, "Error: Classification required. See --help.\n");
                return ret;
        }

        len = strlen(opt_class);

        if ((len == 0) || (len > MAX_CLASS_LENGTH)) {
                fprintf(stderr, "Error: Valid size for classification "
                        "is 1-%d chars\n", MAX_CLASS_LENGTH);
                return ret;
        }

        for (int c = 0; c < len; c++) {
                if (isascii(opt_class[c]) == 0) {
                        fprintf(stderr, "Error: Non-ascii characters detected "
                                "in classification - aborting\n");
                        return ret;
                }
        }

        for (i = 0, x = 0; i <= (len - 1); i++, x++) {
                if (opt_class[i] == '/') {
                        slashes++;
                        x = 0;
                } else {
                        if (x > MAX_SUBCAT_LENGTH) {
                                fprintf(stderr, "Error: Classification strings"
                                        " between slashes should have at most"
                                        " %d chars\n", MAX_SUBCAT_LENGTH);
                                return ret;
                        }
                }
        }

        if (slashes != 2) {
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

        return true;
}

static bool get_payload_from_file(char **payload)
{
        bool ret = false;
        FILE *fp = NULL;
        size_t bytes_in = 0;

        fp = fopen(opt_payload_file, "r");

        if (!fp) {
                fprintf(stderr, "Error: file %s could not be opened "
                        "for reading\n", opt_payload_file);
                goto out;
        }

        bytes_in = fread(*payload, 1,
                         MAX_PAYLOAD_LENGTH - 1, fp);

        /* if fread fails */
        if (bytes_in == 0 && ferror(fp) != 0) {
                goto out;
        }

        ret = true;
out:
        if (fp) {
                fclose(fp);
        }
        return ret;

}

static void get_payload_from_opt(char **payload)
{
        size_t len = 0;

        len = strlen(opt_payload);
        if (len >= MAX_PAYLOAD_LENGTH) {
                len = MAX_PAYLOAD_LENGTH - 1;
        }

        /* "payload" is pre-allocated zeroed buffer of MAX_PAYLOAD_LENGTH */
        memcpy(*payload, opt_payload, len);
}

static void get_payload_from_stdin(char **payload)
{
        size_t bytes_in = 0;
        int c;

        bytes_in = fread(*payload, 1, MAX_PAYLOAD_LENGTH - 1, stdin);

        if (bytes_in == MAX_PAYLOAD_LENGTH - 1) {
                /* Throw away the rest of stdin */
                while (EOF != (c = fgetc(stdin))) {
                        continue;
                }
        }

}

static bool get_payload(char **payload)
{
        bool ret = false;

        *payload = (char *)calloc(sizeof(char), MAX_PAYLOAD_LENGTH);

        if (*payload == NULL)
                return false;

        if (opt_payload_file) {

                if (get_payload_from_file(payload)) {
                        ret = true;
                }

        } else if (opt_payload) {

                get_payload_from_opt(payload);
                ret = true;

        } else {
                get_payload_from_stdin(payload);
                ret = true;
        }

        if (ret == false) {
                free(*payload);
                *payload = NULL;
        }

        return ret;
}

static int instanciate_record(struct telem_ref **t_ref, char *payload)
{
        int ret = 0;

        if ((ret = tm_create_record(t_ref, (uint32_t)severity,
                                    opt_class, payload_version)) < 0) {
                goto out1;
        }

        if (opt_event_id) {
                if ((ret = tm_set_event_id(*t_ref, opt_event_id)) < 0) {
                        goto out1;
                }
        }

        if ((ret = tm_set_payload(*t_ref, payload)) < 0) {
                goto out1;
        }
out1:
        return ret;
}

static int send_record(char *payload)
{
        struct telem_ref *t_ref = NULL;
        int ret = 0;

        if ((ret = instanciate_record(&t_ref, payload)) < 0) {
                goto out;
        }

        if ((ret = tm_send_record(t_ref)) < 0) {
                if (ret == -ECONNREFUSED) {
                        fprintf(stdout, "Unable to send record make sure to opt-in to telemetry first 'telemctl opt-in'\n");
                }
                goto out;
        }

out:
        tm_free_record(t_ref);

        return ret;
}

static int print_record(char *payload)
{
        struct telem_ref *t_ref = NULL;
        int ret;

        if ((ret = instanciate_record(&t_ref, payload)) == 0) {
                int i;
                for (i = 0; i < NUM_HEADERS; i++) {
                        fprintf(stdout, "%s", t_ref->record->headers[i]);
                }
                fprintf(stdout, "%s\n", t_ref->record->payload);
        }

        tm_free_record(t_ref);
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

        if (opt_echo == true) {
                print_record(payload);
        }

        if (opt_nopost == false && send_record(payload) != 0) {
                goto fail;
        }

        ret = EXIT_SUCCESS;
fail:
        free(opt_class);
        free(opt_payload);
        free(opt_event_id);
        free(payload);

        return ret;
}

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
