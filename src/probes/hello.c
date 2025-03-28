/*
 * This program is part of the Clear Linux Project
 *
 * Copyright 2015-2019 Intel Corporation
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
#include <locale.h>
#include <sys/sysinfo.h>
#include <dirent.h>
#include "common.h"
#include "telemetry.h"
#include "config.h"
#include "log.h"

#define PAYLOAD_LOCALE  0x01
#define PAYLOAD_UPTIME  0x02
#define PAYLOAD_BUNDLES 0x04

static void print_usage(char *prog)
{
        printf("%s: Usage\n", prog);
        printf("  -f,  --config_file    Configuration file. This overides the other parameters\n");
        printf("  -H,  --heartbeat      Create a \"heartbeat\" record instead\n");
        printf("  -h,  --help           Display this help message\n");
        printf("  -l,  --locale         Include locale in the record\n");
        printf("  -u,  --uptime         Include uptime in the record\n");
        printf("  -b,  --bundles        Include installed bundles in the record\n");
        printf("  -V,  --version        Print the program version\n");
}

static long get_uptime(void)
{
        struct sysinfo s_info;
        if (sysinfo(&s_info) == 0) {
                return s_info.uptime;
        }
        return 0;
}

static int nodots(const struct dirent *dp)
{
        return (dp->d_name[0] != '.');
}

static char *create_payload(unsigned options)
{
        int n, buflen;
        char *ppos, *payload;

        payload = calloc(sizeof(char), MAX_PAYLOAD_LENGTH);
        if (payload == NULL) {
                return NULL;
        }

        ppos = payload;
        buflen = MAX_PAYLOAD_LENGTH;

        n = snprintf(ppos, (size_t)buflen, "hello\n");
        buflen -= n, ppos += n;

        if (options & PAYLOAD_LOCALE) {
                n = snprintf(ppos, (size_t)buflen, "LC_ALL: %s\n", setlocale(LC_ALL, NULL));
                buflen -= n, ppos += n;
        }

        if (options & PAYLOAD_UPTIME) {
                n = snprintf(ppos, (size_t)buflen, "uptime: %ld\n", get_uptime());
                buflen -= n, ppos += n;
        }

        if (options & PAYLOAD_BUNDLES) {
                int num_bundles, i;
                struct dirent **entries;

                num_bundles = scandir("/usr/share/clear/bundles", &entries, nodots, alphasort);
                if (num_bundles < 0) {
                        telem_perror("scandir failed");
                        free(payload);
                        return NULL;
                }
                n = snprintf(ppos, (size_t)buflen, "\nBundles (%d):\n", num_bundles);
                buflen -= n, ppos += n;

                for (i = 0; i < num_bundles; i++) {
                        n = snprintf(ppos, (size_t)buflen, "%s\n", entries[i]->d_name);
                        /* Test if truncated output */
                        if (n >= buflen) {
                                int lastix = MAX_PAYLOAD_LENGTH-1;
                                payload[lastix] = '\0';
                                payload[lastix-1] = '.';
                                payload[lastix-2] = '.';
                                payload[lastix-3] = '.';
                                payload[lastix-4] = '\n';
                                break;
                        }
                        buflen -= n, ppos += n;
                }

                for (i= 0; i < num_bundles; i++) {
                        free(entries[i]);
                }

                free(entries);
        }

        return payload;
}

int main(int argc, char **argv)
{
        uint32_t severity = 1;
        uint32_t payload_version = 1;
        char classification[30] = "org.clearlinux/hello/world";
        struct telem_ref *tm_handle = NULL;
        char *payload;
        char *str;
        int ret;
        unsigned payload_options = 0;

        // Following vars are for arg parsing.
        int c;
        int opt_index = 0;

        struct option opts[] = {
                { "config_file", 1, NULL, 'f' },
                { "heartbeat", 0, NULL, 'H' },
                { "help", 0, NULL, 'h' },
                { "version", 0, NULL, 'V' },
                { "locale", 0, NULL, 'l' },
                { "uptime", 0, NULL, 'u' },
                { "bundles", 0, NULL, 'b' },
                { NULL, 0, NULL, 0 }
        };

        while ((c = getopt_long(argc, argv, "f:hHVlub", opts, &opt_index)) != -1) {
                switch (c) {
                        case 'f':
                                if (tm_set_config_file(optarg) != 0) {
                                    telem_log(LOG_ERR, "Configuration file"
                                                  " path not valid\n");
                                    exit(EXIT_FAILURE);
                                }
                                break;
                        case 'H':
                                str = "org.clearlinux/heartbeat/ping";
                                memcpy(classification, str, strlen(str) + 1);
                                break;
                        case 'l':
                                payload_options |= PAYLOAD_LOCALE;
                                break;
                        case 'u':
                                payload_options |= PAYLOAD_UPTIME;
                                break;
                        case 'b':
                                payload_options |= PAYLOAD_BUNDLES;
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

        setlocale(LC_ALL, "");
        payload = create_payload(payload_options);
        if (payload == NULL) {
                exit(EXIT_FAILURE);
        }

        if ((ret = tm_create_record(&tm_handle, severity, classification,
                                    payload_version)) < 0) {
                telem_log(LOG_ERR, "Failed to create record: %s\n",
                       strerror(-ret));
                ret = EXIT_FAILURE;
                goto done;
        }

        if ((ret = tm_set_payload(tm_handle, payload)) < 0) {
                telem_log(LOG_ERR, "Failed to set record payload: %s\n",
                        strerror(-ret));
                ret = EXIT_FAILURE;
                goto done;
        }

        free(payload);

        if ((ret = tm_send_record(tm_handle)) < 0) {
                telem_log(LOG_ERR, "Failed to send record to daemon: %s\n",
                        strerror(-ret));
                ret = EXIT_FAILURE;
                goto done;
        } else {
                printf("Successfully sent record to daemon.\n");
                ret = EXIT_SUCCESS;
        }
done:
        tm_free_record(tm_handle);
        tm_handle = NULL;

        return ret;
}

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
