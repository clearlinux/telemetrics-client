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

#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Default configuration settings */
#define DEFAULT_SERVER_ADDR BACKEND_ADDR
#define DEFAULT_SOCKET_PATH "/run/telem-0"
#define DEFAULT_SPOOL_DIR LOCALSTATEDIR "/spool/telemetry"
#define DEFAULT_RATE_LIMIT_STRATEGY "spool"
#define DEFAULT_CAINFO ""
#define DEFAULT_TIDHEADER "X-Telemetry-TID: 6907c830-eed9-4ce9-81ae-76daf8d88f0f"

#define DEFAULT_RECORD_EXPIRY 1200
#define DEFAULT_SPOOL_MAX_SIZE 5120
#define DEFAULT_SPOOL_PROCESS_TIME 120
#define DEFAULT_RECORD_WINDOW_LENGTH 15
#define DEFAULT_BYTE_WINDOW_LENGTH 20
#define DEFAULT_RECORD_BURST_LIMIT 1000
#define DEFAULT_BYTE_BURST_LIMIT -1

#define DEFAULT_RATE_LIMIT_ENABLED true
#define DEFAULT_DAEMON_RECYCLING_ENABLED true
#define DEFAULT_RECORD_RETENTION_ENABLED false
#define DEFAULT_RECORD_SERVER_DELIVERY_ENABLED true

#define TM_MAX_WINDOW_LENGTH (1 /*h*/ * 60 /*m*/)

enum config_str_keys {
        CONF_SERVER_ADDR = 0,
        CONF_SOCKET_PATH,
        CONF_SPOOL_DIR,
        CONF_RATE_LIMIT_STRATEGY,
        CONF_CAINFO,
        CONF_TIDHEADER,
        CONF_STR_MAX
};

enum config_int_keys {
        CONF_RECORD_EXPIRY = 0,
        CONF_SPOOL_MAX_SIZE,
        CONF_SPOOL_PROCESS_TIME,
        CONF_RECORD_WINDOW_LENGTH,
        CONF_BYTE_WINDOW_LENGTH,
        CONF_RECORD_BURST_LIMIT,
        CONF_BYTE_BURST_LIMIT,
        CONF_INT_MAX
};

enum config_bool_keys {
        CONF_RATE_LIMIT_ENABLED = 0,
        CONF_DAEMON_RECYCLING_ENABLED,
        CONF_RECORD_RETENTION_ENABLED,
        CONF_RECORD_SERVER_DELIVERY_ENABLED,
        CONF_BOOL_MAX
};

typedef struct configuration {
        char *strValues[CONF_STR_MAX];
        int64_t intValues[CONF_INT_MAX];
        bool boolValues[CONF_BOOL_MAX];
        bool initialized;
        char *config_file;
} configuration;

/* Sets the configuration file to be used later */
int set_config_file(const char *filename);

/* Gets the configuration currently in use */
const char *get_config_file(void);

/* Gets the configuration specified via command line or NULL */
const char *get_cmd_line_config_file(void);

/* Sets all default configuration values to a given config */
bool set_default_config_values(struct configuration *config);

/* Parses the ini format config file */
bool read_config_from_file(char *filename, struct configuration *config);

/* Causes the daemon to read the configuration file */
void reload_config(void);

/* Getters for the configuration values */

/* Gets the server address to send the telemetry records */
const char *server_addr_config(void);

/* Gets the path for the unix domain socket */
const char *socket_path_config(void);

/* Gets the path for the spool directory */
const char *spool_dir_config(void);

/* Gets the record expiry time */
int64_t record_expiry_config(void);

/*
 * Gets the maximum size of the spool. Once the spool reaches the maximum size,
 * new records are dropped
 */
int64_t spool_max_size_config(void);

/* Get the time ainterval for processing records in the spool */
int spool_process_time_config(void);

/* Gets the record burst limit */
int64_t record_burst_limit_config(void);

/* Gets the record window length */
int record_window_length_config(void);

/* Gets the byte burst limit */
int64_t byte_burst_limit_config(void);

/* Gets the byte window length */
int byte_window_length_config(void);

/* Gets whether rate limiting is enabled */
bool rate_limit_enabled_config(void);

/* Gets strategy for record if rate limits are met */
const char *rate_limit_strategy_config(void);

/* Gets CAINFO */
const char *get_cainfo_config(void);

/* Gets tidheader */
const char *get_tidheader_config(void);

/* Gets whether recycling is enabled */
bool daemon_recycling_enabled_config(void);

/* Gets whether local retention of records is enabled */
bool record_retention_enabled_config(void);

/* Gets whether records should be sent to server_addr */
bool record_server_delivery_enabled_config(void);

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
