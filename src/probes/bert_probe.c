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

struct acpi_generic_error_data_entry {
    uint8_t section_type[16];
    uint32_t error_severity;
    uint16_t revision;
    uint8_t validation_bits;
    uint8_t flags;
    uint32_t error_data_length;
    uint8_t fru_ld[16];
    uint8_t fru_text[16];
    uint64_t timestamp;
    uint8_t data[1];        // error_data_length
} __attribute__((packed));

struct acpi_generic_error_status_block {
    // Bit [0] - Uncorrectable Error Valid
    // Bit [1] - Correctable Error Valid
    // Bit [2] - Multiple Uncorrectable Errors
    // Bit [3] - Multiple Correctable Errors
    // Bit [13:4] - Error Data Entry Count: Number of Error Data Entries found in the Data section.
    // Bit [31:14] - Reserved
    uint32_t block_status;
    uint32_t raw_data_offset;
    // Length in bytes of the generic error data
    uint32_t data_length;
    uint32_t error_severity;
    // The information contained in this field is a collection of zero
    // or more Generic Error Data Entrie
    struct acpi_generic_error_data_entry data[];
} __attribute__((packed));

static int check_bert_file(const char *filename, char *buff, size_t buff_size) {

        int ret = EXIT_FAILURE;
        size_t len;
        uint32_t block_status;
        FILE *fh = fopen(filename, "rb");

        if (fh == NULL) {
                telem_log(LOG_ERR, "Failed to open: %s\n", filename);
                goto done;
        }

        len = fread(&block_status, 1, sizeof(block_status), fh);
        if (ferror(fh) || (len != sizeof(block_status))){
                telem_log(LOG_ERR, "Error reading block_status: %s\n", filename);
                goto done;
        }

        /* Check the "Error Data Entry Count", if 0 then nothing
         * to report */

        if ((block_status & 0x3FFF) != 0) {
                rewind(fh);

                /* nc_b64enc_file returns 1 in success and 0 in failure */
                if (nc_b64enc_file(fh, buff, buff_size) == 0) {
                        telem_log(LOG_ERR, "Failed to read payload from: %s\n", filename);
                        goto done;
                }
        }

        ret = EXIT_SUCCESS;
done:
        if (fh) {
                fclose(fh);
        }

        return ret;
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
                telem_log(LOG_ERR, "Unable to allocate more memory.\n");
                return -ENOMEM;
        }

        ret = check_bert_file(bert_record_file, payload, MAX_PAYLOAD_LENGTH);
        if (ret == EXIT_FAILURE) {
                goto done;
        } else {
                if (payload[0] == 0) {
                        /* Nothing to report */
                        goto done;
                }
        }

        /* Valid payload, send the record */
        if ((ret = tm_create_record(&tm_handle, severity, classification,
                                    payload_version)) < 0) {
                telem_log(LOG_ERR, "Failed to create record: %s\n", strerror(-ret));
                goto done;
        }

        if ((ret = tm_set_payload(tm_handle, payload)) < 0) {
                telem_log(LOG_ERR, "Failed to set record payload: %s\n", strerror(-ret));
                goto done;
        }

        if ((ret = tm_send_record(tm_handle)) < 0) {
                telem_log(LOG_ERR, "Failed to send record to daemon: %s\n", strerror(-ret));
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
