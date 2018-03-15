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

#include <check.h>
#include "configuration.h"

START_TEST(check_read_config_for_invalid_file)
{
        char *config_file = "somefile";
        configuration config;

        int ret = read_config_from_file(config_file, &config);
        ck_assert(ret == false);
}
END_TEST

START_TEST(check_read_valid_config)
{
        char *config_file = TOPSRCDIR "/src/data/example.conf";
        configuration config = { { 0 }, { 0 }, { 0 }, false, NULL };

        int ret = read_config_from_file(config_file, &config);
        ck_assert(ret == true);

        ck_assert_str_eq(config.strValues[CONF_SERVER_ADDR], "http://127.0.0.1");
        ck_assert_str_eq(config.strValues[CONF_SOCKET_PATH], "/tmp/test_telem_socket");
        ck_assert_str_eq(config.strValues[CONF_SPOOL_DIR], "/tmp/spool");
        ck_assert_int_eq(config.intValues[CONF_RECORD_EXPIRY], 1200);
        ck_assert_int_eq(config.intValues[CONF_SPOOL_MAX_SIZE], 1024);
        ck_assert_int_eq(config.intValues[CONF_SPOOL_PROCESS_TIME], 900);
        ck_assert_int_eq(config.intValues[CONF_RECORD_BURST_LIMIT], 100);
        ck_assert_int_eq(config.intValues[CONF_RECORD_WINDOW_LENGTH], 15);
        ck_assert_int_eq(config.intValues[CONF_BYTE_BURST_LIMIT], 1000);
        ck_assert_int_eq(config.intValues[CONF_BYTE_WINDOW_LENGTH], 20);
        ck_assert(config.boolValues[CONF_RATE_LIMIT_ENABLED] == true);
        ck_assert_str_eq(config.strValues[CONF_RATE_LIMIT_STRATEGY], "spool");
        ck_assert_str_eq(config.strValues[CONF_CAINFO], "/tmp/cacert.crt");
        ck_assert_str_eq(config.strValues[CONF_TIDHEADER],
                         "X-Telemetry-TID: 6907c830-eed9-4ce9-81ae-76daf8d88f0f");
        ck_assert(config.boolValues[CONF_DAEMON_RECYCLING_ENABLED] == true);

}
END_TEST

START_TEST(check_read_valid_config_defaults)
{
        char *config_file = TOPSRCDIR "/src/data/example.conf";
        configuration config = { { 0 }, { 0 }, { 0 }, false, NULL };

        int ret = read_config_from_file(config_file, &config);
        ck_assert(ret == true);

        // RECORD_RETENTION_ENABLED_DEFAULT = false
        ck_assert(config.boolValues[CONF_RECORD_RETENTION_ENABLED] == RECORD_RETENTION_ENABLED_DEFAULT);
        // RECORD_SERVER_DELIVERY_ENABLED_DEFAULT = true
        ck_assert(config.boolValues[CONF_RECORD_SERVER_DELIVERY_ENABLED] == RECORD_SERVER_DELIVERY_ENABLED_DEFAULT);
}
END_TEST

START_TEST(check_read_valid_config_record_retention_delivery)
{
        char *config_file = TOPSRCDIR "/src/data/example.1.conf";
        configuration config = { { 0 }, { 0 }, { 0 }, false, NULL };

        int ret = read_config_from_file(config_file, &config);
        ck_assert(ret == true);

        ck_assert(config.boolValues[CONF_RECORD_RETENTION_ENABLED] == true);
        ck_assert(config.boolValues[CONF_RECORD_SERVER_DELIVERY_ENABLED] == false);
}
END_TEST

START_TEST(check_config_initialised)
{
        char *config_file = ABSTOPSRCDIR "/src/data/example.conf";

        set_config_file(config_file);

        ck_assert_str_eq(server_addr_config(), "http://127.0.0.1");
        ck_assert_str_eq(socket_path_config(), "/tmp/test_telem_socket");
        ck_assert_str_eq(spool_dir_config(), "/tmp/spool");
        ck_assert_int_eq(record_expiry_config(), 1200);
        ck_assert_int_eq(spool_max_size_config(), 1024);
        ck_assert_int_eq(spool_process_time_config(), 900);
        ck_assert_int_eq(record_burst_limit_config(), 100);
        ck_assert_int_eq(record_window_length_config(), 15);
        ck_assert_int_eq(byte_burst_limit_config(), 1000);
        ck_assert_int_eq(byte_window_length_config(), 20);
        ck_assert(rate_limit_enabled_config() == true);
        ck_assert_str_eq(rate_limit_strategy_config(), "spool");
        ck_assert_str_eq(get_cainfo_config(), "/tmp/cacert.crt");
        ck_assert_str_eq(get_tidheader_config(),
                         "X-Telemetry-TID: 6907c830-eed9-4ce9-81ae-76daf8d88f0f");
        ck_assert(daemon_recycling_enabled_config() == true);
}
END_TEST

Suite *config_suite(void)
{
        // A suite is comprised of test cases, defined below
        Suite *s = suite_create("configuration");

        // Individual unit tests are added to "test cases"
        TCase *t = tcase_create("config");
        tcase_add_test(t, check_read_config_for_invalid_file);
        tcase_add_test(t, check_read_valid_config);
        tcase_add_test(t, check_read_valid_config_defaults);
        tcase_add_test(t, check_read_valid_config_record_retention_delivery);
        tcase_add_test(t, check_config_initialised);

        // add more TCases here

        suite_add_tcase(s, t);

        return s;
}

int main(void)
{
        Suite *s;
        SRunner *sr;

        s = config_suite();
        sr = srunner_create(s);

        // Use the TAP driver for now, so that each
        // unit test will PASS/FAIL in the log output.
        srunner_set_log(sr, NULL);
        srunner_set_tap(sr, "-");

        srunner_run_all(sr, CK_SILENT);
        // failed = srunner_ntests_failed(sr);
        srunner_free(sr);

        // if you want the TAP driver to report a hard error based
        // on certain conditions (e.g. number of failed tests, etc.),
        // return non-zero here instead.
        return 0;
}

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
