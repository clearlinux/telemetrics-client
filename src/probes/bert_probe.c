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

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <getopt.h>
#include "common.h"
#include "telemetry.h"
#include "nica/b64enc.h"

#include "config.h"
#include "log.h"

static const char bert_record_file[] = "/sys/firmware/acpi/tables/data/BERT";
static const char telem_record_class[] = "org.clearlinux/bert/debug";
static uint32_t severity = 2;
static uint32_t payload_version = 1;

void print_usage(char *prog)
{
        printf("%s: Usage\n", prog);
        printf("  -f,  --config_file    Specify a configuration file other than default\n");
        printf("  -h,  --help           Display this help message\n");
        printf("  -V,  --version        Print the program version\n");
}

int main(int argc, char **argv)
{
        struct telem_ref *tm_handle = NULL;
        char *classification = (char *)telem_record_class;
        char *payload;
        int ret;

        // Following vars are for arg parsing.
        int c;
        int opt_index = 0;
        struct option opts[] = {
                { "config_file", 1, NULL, 'f' },
                { "help", 0, NULL, 'h' },
                { "version", 0, NULL, 'V' },
                { NULL, 0, NULL, 0 }
        };

        while ((c = getopt_long(argc, argv, "f:hV", opts, &opt_index)) != -1) {
                switch (c) {
                        case 'f':
                                if (tm_set_config_file(optarg) != 0) {
                                    telem_log(LOG_ERR, "Configuration file"
                                                  " path not valid\n");
                                    exit(EXIT_FAILURE);
                                }
                                break;
                        case 'h':
                                print_usage(argv[0]);
                                exit(EXIT_SUCCESS);
                        case 'V':
                                printf(PACKAGE_VERSION "\n");
                                exit(EXIT_SUCCESS);
                        case '?':
                                exit(EXIT_FAILURE);
                }
        }

        payload = calloc(sizeof(char), MAX_PAYLOAD_LENGTH);
        if (!payload) {
                printf("Unable to allocate more memory.\n");
                return -ENOMEM;
        }

        if (!(ret = nc_b64enc_filename(bert_record_file, payload, MAX_PAYLOAD_LENGTH))) {
                printf("Failed to read payload from: %s\n", bert_record_file);
                ret = EXIT_FAILURE;
                goto done;
        }

        if ((ret = tm_create_record(&tm_handle, severity, classification,
                                    payload_version)) < 0) {
                printf("Failed to create record: %s\n", strerror(-ret));
                goto done;
        }

        if ((ret = tm_set_payload(tm_handle, payload)) < 0) {
                printf("Failed to set record payload: %s\n", strerror(-ret));
                goto done;
        }

        if ((ret = tm_send_record(tm_handle)) < 0) {
                printf("Failed to send record to daemon: %s\n", strerror(-ret));
                goto done;
        }

        ret = EXIT_SUCCESS;
done:
        free(payload);
        tm_free_record(tm_handle);
        tm_handle = NULL;

        return ret;
}

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
