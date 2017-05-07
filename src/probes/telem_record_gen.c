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
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>
#include <string.h>
#include <stdint.h>

#include "config.h"
#include "log.h"
#include "common.h"
#include "telemetry.h"

static char *config_file = NULL;
static gboolean version_p = FALSE;
static uint32_t severity = 1;
static char *opt_class = NULL;
static char *opt_payload = NULL;
static uint32_t payload_version = 1;
static char *opt_payload_file = NULL;

static GOptionEntry options[] = {
        { "config-file", 'f', 0, G_OPTION_ARG_FILENAME, &config_file,
          "Path to configuration file (not implemented yet)", NULL },
        { "version", 'V', 0, G_OPTION_ARG_NONE, &version_p,
          "Print the program version", NULL },
        { "severity", 's', 0, G_OPTION_ARG_INT, &severity,
          "Severity level (1-4) - (default 1)", NULL },
        { "class", 'c', 0, G_OPTION_ARG_STRING, &opt_class,
          "Classification level_1/level_2/level_3", NULL },
        { "payload", 'p', 0, G_OPTION_ARG_STRING, &opt_payload,
          "Record body (max size = 8k)", NULL },
        { "payload-file", 'P', 0, G_OPTION_ARG_STRING, &opt_payload_file,
          "File to read payload from", NULL },
        { "record-version", 'R', 0, G_OPTION_ARG_INT, &payload_version,
          "Version number for format of payload (default 1)", NULL },
        { NULL }
};

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

static void free_glib_strings(void)
{
        if (config_file) {
                g_free(config_file);
        }

        if (opt_class) {
                g_free(opt_class);
        }

        if (opt_payload) {
                g_free(opt_payload);
        }
}

int parse_options(int argc, char **argv)
{
        int ret = 0;
        GError *error = NULL;
        GOptionContext *context;

        context = g_option_context_new(
                "- create and send a custom telemetry record\n");
        g_option_context_add_main_entries(context, options, NULL);
        g_option_context_set_translate_func(context, NULL, NULL, NULL);

        if (!g_option_context_parse(context, &argc, &argv, &error)) {
                printf("Failed to parse options: %s\n", error->message);
                goto fail;
        }

        ret = 1;

fail:
        if (context) {
                g_option_context_free(context);
        }

        return ret;
}

int validate_opts(void)
{
        size_t i;
        int ret = 0;

        /* classification */
        if (opt_class == NULL) {
                fprintf(stderr, "Error: Classification required. See --help.\n");
                return ret;

        }
        i = strlen(opt_class);

        if ((i == 0) || (i > 120)) {
                fprintf(stderr, "Error: Valid size for classification "
                        "is 1-120 chars\n");
                return ret;
        }

        if (!g_str_is_ascii(opt_class)) {
                fprintf(stderr, "Error: Non-ascii characters detected "
                        "in classification - aborting\n");
                return ret;
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

        if (version_p) {
                printf(PACKAGE_VERSION "\n");
                goto success;
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

success:
        ret = EXIT_SUCCESS;
fail:
        free_glib_strings();

        return ret;
}

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */

