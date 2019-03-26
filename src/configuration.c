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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "configuration.h"
#include "util.h"
#include "common.h"

#include "nica/inifile.h"

static char *config_file = NULL;
static char *default_config_file = DATADIR "/defaults/telemetrics/telemetrics.conf";
static char *etc_config_file = "/etc/telemetrics/telemetrics.conf";
static NcHashmap *keyfile = NULL;
static bool cmd_line_cfg = false;

/* Conf strings, integers, and booleans expected in the conf file */
static const char *config_key_str[] = { "server",
                                        "socket_path",
                                        "spool_dir",
                                        "rate_limit_strategy",
                                        "cainfo",
                                        "tidheader"};

static const char *config_key_int[] = { "record_expiry",
                                        "spool_max_size",
                                        "spool_process_time",
                                        "record_window_length",
                                        "byte_window_length",
                                        "record_burst_limit",
                                        "byte_burst_limit" };

static const char *config_key_bool[] = { "rate_limit_enabled",
                                         "daemon_recycling_enabled",
                                         "record_retention_enabled",
                                         "record_server_delivery_enabled" };

static const char *config_str_default[] = { DEFAULT_SERVER_ADDR,
                                            DEFAULT_SOCKET_PATH,
                                            DEFAULT_SPOOL_DIR,
                                            DEFAULT_RATE_LIMIT_STRATEGY,
                                            DEFAULT_CAINFO,
                                            DEFAULT_TIDHEADER };

static const bool config_bool_default[] = { DEFAULT_RATE_LIMIT_ENABLED,
                                            DEFAULT_DAEMON_RECYCLING_ENABLED,
                                            DEFAULT_RECORD_RETENTION_ENABLED,
                                            DEFAULT_RECORD_SERVER_DELIVERY_ENABLED };

static const int config_int_default[] = { DEFAULT_RECORD_EXPIRY,
                                          DEFAULT_SPOOL_MAX_SIZE,
                                          DEFAULT_SPOOL_PROCESS_TIME,
                                          DEFAULT_RECORD_WINDOW_LENGTH,
                                          DEFAULT_BYTE_WINDOW_LENGTH,
                                          DEFAULT_RECORD_BURST_LIMIT,
                                          DEFAULT_BYTE_BURST_LIMIT };


static struct configuration config = { { 0 }, { 0 }, { 0 }, false, NULL };

static int validate_config_file(const char *f)
{
        struct stat sbuf;

        if (f[0] != '/') {
                return -EINVAL;
        }

        /* check if the file exists and is a regular file */
        if (stat(f, &sbuf) != 0) {
                return -errno;
        }

        if (!S_ISREG(sbuf.st_mode)) {
                return -EINVAL;
        }

        return 0;

}

const char *get_config_file(void)
{
        return config_file;
}

const char *get_cmd_line_config_file(void)
{
        if (cmd_line_cfg == true) {
                return config_file;
        }
        return NULL;
}

int set_config_file(const char *filename)
{
        int ret;

        if ((ret = validate_config_file(filename)) == 0) {
                char *filename1 = strdup(filename);
                if (filename1 == NULL) {
                    ret = -errno;
                } else {
                    config_file = filename1;
                    cmd_line_cfg = true;
                }
        }

        return ret;
}

bool set_default_config_values(struct configuration *config)
{
        for (int i = 0; i < CONF_STR_MAX; i++) {
                config->strValues[i] = strdup(config_str_default[i]);
                        if (config->strValues[i] == NULL) {
                                fprintf(stderr, "ERR: could not set config item %s: %s\n", config_key_str[i], strerror(errno));
                                return false;
                        }
        }
        for (int i = 0; i < CONF_INT_MAX; i++) {
                config->intValues[i] = config_int_default[i];
        }
        for (int i = 0; i < CONF_BOOL_MAX; i++) {
                config->boolValues[i] = config_bool_default[i];
        }
        return true;
}

bool read_config_from_file(char *config_file, struct configuration *config)
{
        if (keyfile != NULL) {
                nc_hashmap_free(keyfile);
        }

        keyfile = nc_ini_file_parse(config_file);
        if (!keyfile) {
#ifdef DEBUG
                fprintf(stderr, "ERR: Failed to read config file\n");
#endif
                return false;
        } else {
                for (int i = 0; i < CONF_STR_MAX; i++) {
                        char *ptr;
                        ptr = nc_hashmap_get(nc_hashmap_get(keyfile, "settings"), config_key_str[i]);
                        if (ptr) {
                                config->strValues[i] = strdup(ptr);
                                if (config->strValues[i] == NULL) {
                                        return false;
                                }
                        } else {
                                config->strValues[i] = strdup(config_str_default[i]);
                        }

                }

                for (int i = 0; i < CONF_INT_MAX; i++) {
                        char *ptr;
                        ptr = nc_hashmap_get(nc_hashmap_get(keyfile, "settings"), config_key_int[i]);
                        if (ptr) {
                                errno = 0;
                                config->intValues[i] = strtoll(ptr, NULL, 10);
                                if (errno != 0) {
                                        return false;
                                }
                        } else {
                                config->intValues[i] = config_int_default[i];
                        }
                }

                for (int i = 0; i < CONF_BOOL_MAX; i++) {
                        char *ptr;
                        ptr = nc_hashmap_get(nc_hashmap_get(keyfile, "settings"), config_key_bool[i]);
                        if (ptr) {
                                if ((strcasecmp(ptr, "TRUE") == 0) || (strcmp(ptr, "1") == 0)) {
                                        config->boolValues[i] = true;
                                } else if ((strcasecmp(ptr, "FALSE") == 0) || (strcmp(ptr, "0") == 0)) {
                                        config->boolValues[i] = false;
                                } else {
                                        return false;
                                }
                        } else {
                                config->boolValues[i] = config_bool_default[i];
                        }
                }
        }

        return true;
}

static void initialize_config(void)
{
        if (config.initialized) {
                return;
        }

        /* No config file provided on command line */
        if (!config_file) {
                if (access(etc_config_file, R_OK) == 0) {
                        config_file = etc_config_file;
                } else {
                        if (access(default_config_file, R_OK) == 0) {
                                config_file = default_config_file;
                        }
                }
        }

        if (config_file) {
                if (!read_config_from_file(config_file, &config)) {
                        /* Error while parsing file  */
#ifdef DEBUG
                        fprintf(stderr, "ERR: Error while parsing configuration file\n");
#endif
                        exit(EXIT_FAILURE);
                }
        }
        else {
                if (!set_default_config_values(&config)) {
                        exit(EXIT_FAILURE);
                }
        }

        config.initialized = true;
}

void reload_config(void)
{
        config.initialized = false;

        if (!cmd_line_cfg) {
                config_file = NULL;
        }
        initialize_config();
}

__attribute__((destructor))
void free_configuration(void)
{
        if (!config.initialized) {
                return;
        }

        for (int i = 0; i < CONF_STR_MAX; i++) {
                free(config.strValues[i]);
        }

        if (cmd_line_cfg) {
                free(config_file);
        }
}

const char *server_addr_config()
{
        initialize_config();
        return (const char *)config.strValues[CONF_SERVER_ADDR];
}

const char *socket_path_config()
{
        initialize_config();
        return (const char *)config.strValues[CONF_SOCKET_PATH];
}

const char *spool_dir_config()
{
        initialize_config();
        return (const char *)config.strValues[CONF_SPOOL_DIR];
}

const char *get_cainfo_config()
{
        initialize_config();
        return (const char *)config.strValues[CONF_CAINFO];
}

const char *get_tidheader_config()
{
        initialize_config();
        return (const char *)config.strValues[CONF_TIDHEADER];
}

int64_t record_expiry_config()
{
        initialize_config();
        int64_t val = 0;
        int64_t clamp = LONG_MAX / 60;

        val = config.intValues[CONF_RECORD_EXPIRY];

        /* This value is elsewhere converted to seconds (multiplied by 60)
         * for comparison to time-stamps. Therefore we need to clamp this
         * value to LONG_INT_MAX/60 so as to not overflow later.
         */

        if (val > clamp) {
                val = clamp;
        }

        return (val < 0) ? -1 : val;
}

int64_t spool_max_size_config()
{
        initialize_config();
        int64_t val = 0;
        int64_t clamp = LONG_MAX / 1024;

        val = config.intValues[CONF_SPOOL_MAX_SIZE];

        /* This value is later converted to bytes for comparison purposes,
         * so we must clamp this to LONG_MAX/1024 to avoid overflow
         */

        if (val > clamp) {
                val = clamp;
        }

        return (val < 0) ? -1 : val;
}

int spool_process_time_config()
{
        initialize_config();
        int64_t val = 0;

        val = config.intValues[CONF_SPOOL_PROCESS_TIME];

        if (val < TM_SPOOL_RUN_MIN) {
                /* Spool loop should not run more frequently than 2 min */
                val = TM_SPOOL_RUN_MIN;

        } else if (val > TM_SPOOL_RUN_MAX) {
                /* Spool loop should run at least once an hour */
                val = TM_SPOOL_RUN_MAX;
        }

        return (int)val;
}

int64_t record_burst_limit_config()
{
        initialize_config();
        return config.intValues[CONF_RECORD_BURST_LIMIT];
}

int record_window_length_config()
{
        initialize_config();
        int64_t val = 0;

        val =  config.intValues[CONF_RECORD_WINDOW_LENGTH];

        //WINDOW LENGTH MUST BE INT BETWEEN (0-59)
        return (val < 0 || val >= TM_MAX_WINDOW_LENGTH) ? -1 : (int)val;
}

int64_t byte_burst_limit_config()
{
        initialize_config();
        return config.intValues[CONF_BYTE_BURST_LIMIT];
}

int byte_window_length_config()
{
        initialize_config();
        int64_t val = 0;

        val = config.intValues[CONF_BYTE_WINDOW_LENGTH];

        //WINDOW LENGTH MUST BE INT BETWEEN (0-59)
        return (val < 0 || val >= TM_MAX_WINDOW_LENGTH) ? -1 : (int)val;
}

bool rate_limit_enabled_config()
{
        initialize_config();
        return config.boolValues[CONF_RATE_LIMIT_ENABLED];
}

const char *rate_limit_strategy_config()
{
        initialize_config();
        char *val = NULL;
        size_t k = 0;

        val = config.strValues[CONF_RATE_LIMIT_STRATEGY];
        k = strlen(val);

        for (int i = 0; i < k; i++) {
                val[i] = (char)tolower(val[i]);
        }

        /* default strategy is "spool". */

        if ((strcmp(val, "spool") != 0) && (strcmp(val, "drop") != 0)) {
                val = "spool";
        }

        return val;
}

bool daemon_recycling_enabled_config(void)
{
        initialize_config();
        return config.boolValues[CONF_DAEMON_RECYCLING_ENABLED];
}

bool record_retention_enabled_config(void)
{
        initialize_config();
        return config.boolValues[CONF_RECORD_RETENTION_ENABLED];
}

bool record_server_delivery_enabled_config(void)
{
        initialize_config();
        return config.boolValues[CONF_RECORD_SERVER_DELIVERY_ENABLED];
}
/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
