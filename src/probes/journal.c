/*
 * This program is part of the Clear Linux Project
 *
 * Copyright 2015-2017 Intel Corporation
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

/* Certain static analysis tools do not understand GCC's __INCLUDE_LEVEL__
 * macro; the conditional definition below is used to fix the build with
 * systemd's _sd-common.h, included by the public systemd headers, which rely on
 * the macro being defined and having an accurate value.
 */
#ifndef __INCLUDE_LEVEL__
# define __INCLUDE_LEVEL__ 2
#endif

#include <systemd/sd-journal.h>
#include <systemd/sd-id128.h>

#include "config.h"
#include "log.h"
#include "telemetry.h"

#define BOOT_ID_LEN 33

static GString *payload = NULL;
static uint32_t severity = 2;
static uint32_t payload_version = 1;
static char error_class[30] = "org.clearlinux/journal/error";
static sd_journal *journal = NULL;

static inline void tm_journal_err(const char *msg, int ret)
{
        telem_log(LOG_ERR, "%s: %s\n", msg, strerror(-ret));
}

static inline void tm_journal_match_err(int ret)
{
        telem_log(LOG_ERR, "sd_journal_add_match() failed: %s\n", strerror(-ret));
}

static void add_to_payload(const void *data, size_t length)
{
        if (payload != NULL) {
                g_string_append_printf(payload, "%.*s", (int)length,
                                       (char *)data);
        } else {
                payload = g_string_new(NULL);
                g_string_printf(payload, "%.*s", (int)length,
                                (char *)data);
        }
}

static bool send_data(char *class)
{
        struct telem_ref *handle = NULL;
        int ret;

        if ((ret = tm_create_record(&handle, severity, class,
                                    payload_version)) < 0) {
                telem_log(LOG_ERR, "Failed to create record: %s",
                          strerror(-ret));
                goto fail;
        }

        gchar *payload_str = g_string_free(payload, FALSE);
        payload = NULL;

        if ((ret = tm_set_payload(handle, (char *)payload_str)) < 0) {
                telem_log(LOG_ERR, "Failed to set payload: %s", strerror(-ret));
                free(payload_str);
                tm_free_record(handle);
                goto fail;
        }

        free(payload_str);

        if ((ret = tm_send_record(handle)) < 0) {
                telem_log(LOG_ERR, "Failed to send record: %s", strerror(-ret));
                tm_free_record(handle);
                goto fail;
        }

        tm_free_record(handle);
        return true;
fail:
        return false;
}

static int read_new_entries(sd_journal *journal)
{
        int ret;
        const void *data;
        size_t length;
        int num_entries = 0;

        while ((ret = sd_journal_next(journal)) > 0) {
                ret = sd_journal_get_data(journal, "MESSAGE", &data, &length);
                if (ret < 0) {
                        tm_journal_err("Failed to read journal entry", ret);
                        return -1;
                }

                add_to_payload(data, length);

                num_entries++;
        }

        /* Since the newest entry in the journal might not match our filters,
         * the 0 return code either indicates that we've processed the last
         * journal entry, or it was skipped, and we're now positioned at the
         * very end of the journal.
         */
        if (ret == 0) {
                return num_entries;
        } else {
                tm_journal_err("Failed to advance journal pointer", ret);
                return -1;
        }
}

static bool process_existing_entries(sd_journal *journal)
{
        int ret;

        ret = sd_journal_seek_head(journal);
        if (ret < 0) {
                tm_journal_err("Failed to seek to start of journal", ret);
                return false;
        }

        ret = read_new_entries(journal);
        if (ret < 0) {
                return false;
        }

        if (!payload) {
                telem_log(LOG_DEBUG, "No existing entries found\n");
                return true;
        }

        if (!send_data(error_class)) {
                return false;
        }

        return true;
}

static bool get_boot_id(char **data)
{
        sd_id128_t boot_id_128;
        char boot_id[BOOT_ID_LEN];
        int r;

        // We are only interested in messages from the current boot
        r = sd_id128_get_boot(&boot_id_128);
        if (r < 0) {
                tm_journal_err("sd_id128_get_boot failed", r);
                return false;
        }

        sd_id128_to_string(boot_id_128, boot_id);
        if (asprintf(data, "%s%s", "_BOOT_ID=", boot_id) < 0) {
                abort();
        }

        return true;
}

static bool add_filters(sd_journal *journal)
{
        char *data = NULL;
        int r;

        if (!get_boot_id(&data)) {
                return false;
        }

        r = sd_journal_add_match(journal, data, 0);
        if (r < 0) {
                tm_journal_match_err(r);
                return false;
        }
        free(data);

        r = sd_journal_add_match(journal, "PRIORITY=1", 0);
        if (r < 0) {
                tm_journal_match_err(r);
                return false;
        }

        r = sd_journal_add_match(journal, "PRIORITY=2", 0);
        if (r < 0) {
                tm_journal_match_err(r);
                return false;
        }

        r = sd_journal_add_match(journal, "PRIORITY=3", 0);
        if (r < 0) {
                tm_journal_match_err(r);
                return false;
        }

        return true;
}

static bool process_journal(void)
{
        int r;

        r = sd_journal_open(&journal, SD_JOURNAL_LOCAL_ONLY);
        if (r < 0) {
                tm_journal_err("Failed to open journal", r);
                return false;
        }

        if (!add_filters(journal)) {
                return false;
        }

        if (!process_existing_entries(journal)) {
                return false;
        }

        struct pollfd pfd;
        pfd.fd = sd_journal_get_fd(journal);
        pfd.events = (short int)sd_journal_get_events(journal);

        // Now wait for new journal entries
        while (true) {
                r = poll(&pfd, 1, -1);
                if (r < 0) {
                        telem_perror("Polling failed on the journal socket");
                        return false;
                } else if (r == 0) {
                        // unreachable, since we don't set a timeout
                        telem_log(LOG_ERR, "poll timed out\n");
                        return false;
                } else {
                        r = sd_journal_process(journal);
                        // we don't care about the NOP or INVALIDATE cases
                        if (r == SD_JOURNAL_APPEND) {
                                r = read_new_entries(journal);
                                if (r == 0) {
                                        continue;
                                } else if (r < 0) {
                                        return false;
                                }

                                if (!send_data(error_class)) {
                                        return false;
                                }
                        }
                }
        }
}

static gchar *config_file = NULL;
static gboolean version_p = FALSE;

static GOptionEntry options[] = {
        { "config-file", 'f', 0, G_OPTION_ARG_FILENAME, &config_file,
          "Path to configuration file (not implemented yet)", NULL },
        { "version", 'V', 0, G_OPTION_ARG_NONE, &version_p,
          "Print the program version", NULL },
        { NULL }
};

static void free_glib_strings(void)
{
        if (config_file) {
                g_free(config_file);
        }
}

int main(int argc, char **argv)
{
        int ret = EXIT_FAILURE;
        GError *error = NULL;
        GOptionContext *context;

        context = g_option_context_new("- collect data from systemd journal");
        g_option_context_add_main_entries(context, options, NULL);
        g_option_context_set_translate_func(context, NULL, NULL, NULL);
        if (!g_option_context_parse(context, &argc, &argv, &error)) {
                g_print("Failed to parse options: %s\n", error->message);
                goto fail;
        }

        if (version_p) {
                g_print(PACKAGE_VERSION "\n");
                goto success;
        }

        if (!process_journal()) {
                goto fail;
        }

success:
        ret = EXIT_SUCCESS;
fail:
        free_glib_strings();

        if (journal) {
                sd_journal_close(journal);
        }

        if (context) {
                g_option_context_free(context);
        }

        if (payload) {
                g_string_free(payload, TRUE);
        }

        return ret;
}

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
