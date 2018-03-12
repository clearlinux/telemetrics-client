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

static int allocate_payload_buffer(char **payload)
{
        int ret = 0;

        *payload = (char *)malloc(MAX_PAYLOAD_SIZE);

        if (*payload == NULL) {
                goto out;
        }

        *payload = memset(*payload, 0, MAX_PAYLOAD_SIZE);
        ret = 1;

out:
        return ret;
}

int main(int argc, char **argv)
{
        struct telem_ref *tm_handle = NULL;
        char *classification = (char *)telem_record_class;
        char *payload = NULL;

        // Following vars are for arg parsing.
        int c;
        char *cfile = NULL;
        int opt_index = 0;
        int ret = 0;

        struct option opts[] = {
                { "config_file", 1, NULL, 'f' },
                { "help", 0, NULL, 'h' },
                { "version", 0, NULL, 'V' },
                { NULL, 0, NULL, 0 }
        };

        while ((c = getopt_long(argc, argv, "f:hHV", opts, &opt_index)) != -1) {
                switch (c) {
                        case 'f':
                                if (asprintf(&cfile, "%s", (char *)optarg) < 0) {
                                        exit(EXIT_FAILURE);
                                }

                                struct stat buf;
                                ret = stat(cfile, &buf);

                                /* check if the file exists and is a regular file */
                                if (ret == -1 || !S_ISREG(buf.st_mode)) {
                                        exit(EXIT_FAILURE);
                                }

                                tm_set_config_file(cfile);
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

        if (!(ret = allocate_payload_buffer(&payload))) {
                printf("Unable to allocate more memory.\n");
                goto fail;
        }

        if (!(ret = nc_b64enc_filename(bert_record_file, payload, MAX_PAYLOAD_SIZE))) {
                printf("Failed to read payload from: %s\n", bert_record_file);
                goto fail;
        }

        if ((ret = tm_create_record(&tm_handle, severity, classification,
                                    payload_version)) < 0) {
                printf("Failed to create record: %s\n", strerror(-ret));
                goto fail;
        }

        if ((ret = tm_set_payload(tm_handle, payload)) < 0) {
                printf("Failed to set record payload: %s\n", strerror(-ret));
                goto fail;
        }

        free(payload);

        if ((ret = tm_send_record(tm_handle)) < 0) {
                printf("Failed to send record to daemon: %s\n", strerror(-ret));
                goto fail;
        }

fail:
        free(cfile);
        tm_free_record(tm_handle);
        tm_handle = NULL;

        return ret;
}

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
