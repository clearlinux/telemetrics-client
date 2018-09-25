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

#include <check.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <unistd.h>

#include "configuration.h"
#include "telempostdaemon.h"
#include "common.h"

TelemPostDaemon tdaemon;

bool dummy_post(char *json_str)
{
        return true;
}

bool (*post_record_ptr)(char *) = dummy_post;

void setup(void)
{
        char *config_file = ABSTOPSRCDIR "/src/data/example.conf";
        set_config_file(config_file);

        initialize_daemon(&tdaemon);
}

START_TEST(check_daemon_is_initialized)
{
        setup();

        ck_assert(tdaemon.rate_limit_enabled == true);
        ck_assert(tdaemon.record_burst_limit == 100);
        ck_assert(tdaemon.record_window_length == 15);
        ck_assert(tdaemon.byte_burst_limit == 1000);
        ck_assert(tdaemon.byte_window_length == 20);
        ck_assert_str_eq(tdaemon.rate_limit_strategy, "spool");
}
END_TEST

START_TEST(check_handle_client_with_no_data)
{
        setup();

        bool success;
        char *filename = ABSTOPSRCDIR "/tests/telempostd/empty_message";

        success = process_staged_record(filename, false, &tdaemon);
        // Return true to remove corrupted record
        ck_assert(success == true);
}
END_TEST

START_TEST(check_handle_client_with_incorrect_data)
{
        setup();

        bool success;
        char *filename = ABSTOPSRCDIR "/tests/telempostd/incorrect_message";

        success = process_staged_record(filename, false, &tdaemon);
        // Return true to remove corrupted record
        ck_assert(success == true);
}
END_TEST

START_TEST(check_process_record_with_correct_size_and_data)
{
        setup();

        bool success;
        char *filename = ABSTOPSRCDIR "/tests/telempostd/correct_message";

        success = process_staged_record(filename, false, &tdaemon);
        ck_assert(success == true);
}
END_TEST

START_TEST(check_process_record_with_incorrect_headers)
{
        setup();

        bool success;
        char *filename = ABSTOPSRCDIR "/tests/telempostd/incorrect_headers";

        success = process_staged_record(filename, false, &tdaemon);
        // Return true to remove corrupted record
        ck_assert(success == true);
}
END_TEST

START_TEST(check_rate_limit_enabled_functions)
{
        setup();

        bool record_check_passed = true;
        bool byte_check_passed = true;
        bool record_burst_enabled = true;
        bool byte_burst_enabled = true;
        tdaemon.record_burst_limit = 4;
        tdaemon.byte_burst_limit = 2999;
        tdaemon.rate_limit_enabled = true;

        bool burst_limit_enabled(int64_t burst_limit)
        {
                return (burst_limit > -1) ? true : false;
        }

        /* Checks whether record burst rate limiting is enabled */
        if (burst_limit_enabled(tdaemon.record_burst_limit)) {
                record_check_passed = false;
        } else {
                /* If not enabled, does not need to be checked */
                record_burst_enabled = false;
        }
        /* Checks whether byte burst rate limiting is enabled */
        if (burst_limit_enabled(tdaemon.byte_burst_limit)) {
                byte_check_passed = false;
        } else {
                /* If not enabled, does not need to be checked */
                byte_burst_enabled = false;
        }
        if (!record_burst_enabled && !byte_burst_enabled) {
                tdaemon.rate_limit_enabled = false;
        }

        ck_assert(record_check_passed == false);
        ck_assert(byte_check_passed == false);
        ck_assert(tdaemon.rate_limit_enabled == true);
}
END_TEST

START_TEST(check_rate_limit_records_that_pass)
{
        setup();

        int current_minute = 3;
        tdaemon.record_burst_array[1] = 10;
        tdaemon.record_burst_array[7] = 10;
        tdaemon.record_burst_array[10] = 20;
        tdaemon.record_burst_array[55] = 10;
        tdaemon.record_burst_limit = 30;
        tdaemon.record_window_length = 15;

        bool checker;
        checker = rate_limit_check(current_minute, tdaemon.record_burst_limit,
                                   tdaemon.record_window_length, tdaemon.record_burst_array, 1);

        ck_assert(checker == true);
}
END_TEST

START_TEST(check_rate_limit_records_that_do_not_pass)
{
        setup();

        int current_minute = 24;
        tdaemon.record_burst_array[3] = 10;
        tdaemon.record_burst_array[4] = 10;
        tdaemon.record_burst_array[6] = 10;
        tdaemon.record_burst_array[27] = 10;
        tdaemon.record_burst_limit = 30;
        tdaemon.record_window_length = 30;

        bool checker;
        checker = rate_limit_check(current_minute, tdaemon.record_burst_limit,
                                   tdaemon.record_window_length, tdaemon.record_burst_array, 1);

        ck_assert(checker == false);
}
END_TEST

START_TEST(check_rate_limit_bytes_that_pass)
{
        setup();

        size_t incValue = 20000;

        int current_minute = 3;
        tdaemon.byte_burst_array[1] = 32000;
        tdaemon.byte_burst_array[7] = 10000;
        tdaemon.byte_burst_array[10] = 45000;
        tdaemon.byte_burst_array[55] = 11000;
        tdaemon.byte_burst_limit = 64000;
        tdaemon.byte_window_length = 15;

        bool checker;
        checker = rate_limit_check(current_minute, tdaemon.byte_burst_limit,
                                   tdaemon.byte_window_length, tdaemon.byte_burst_array, incValue);

        ck_assert(checker == true);
}
END_TEST

START_TEST(check_rate_limit_bytes_that_do_not_pass)
{
        setup();

        size_t incValue = 80000;

        int current_minute = 15;
        tdaemon.byte_burst_array[1] = 32000;
        tdaemon.byte_burst_array[7] = 10000;
        tdaemon.byte_burst_array[10] = 32300;
        tdaemon.byte_burst_array[55] = 31000;
        tdaemon.byte_burst_limit = 100000;
        tdaemon.byte_window_length = 15;

        bool checker;
        checker = rate_limit_check(current_minute, tdaemon.byte_burst_limit,
                                   tdaemon.byte_window_length, tdaemon.byte_burst_array, incValue);

        ck_assert(checker == false);
}
END_TEST

START_TEST(check_update_record_array)
{
        setup();
        int current_minute = 7;
        tdaemon.record_window_length = 15;
        rate_limit_update(current_minute, tdaemon.record_window_length,
                          tdaemon.record_burst_array, TM_RECORD_COUNTER);

        rate_limit_update(current_minute, tdaemon.record_window_length,
                          tdaemon.record_burst_array, TM_RECORD_COUNTER);

        current_minute = 45;
        rate_limit_update(current_minute, tdaemon.record_window_length,
                          tdaemon.record_burst_array, TM_RECORD_COUNTER);

        current_minute = 47;
        rate_limit_update(current_minute, tdaemon.record_window_length,
                          tdaemon.record_burst_array, TM_RECORD_COUNTER);
        rate_limit_update(current_minute, tdaemon.record_window_length,
                          tdaemon.record_burst_array, TM_RECORD_COUNTER);

        for (int i = 0; i < 60; i++) {
                if (i != 45 && i != 47) {
                        ck_assert_msg(tdaemon.record_burst_array[i] == 0, "array at %d is %d\n",
                                      i, tdaemon.record_burst_array[i]);
                } else {
                        ck_assert_msg(tdaemon.record_burst_array[45] == 1, "array: %d is %d\n",
                                      i, tdaemon.record_burst_array[i]);
                        ck_assert_msg(tdaemon.record_burst_array[47] == 2, "array: %d is %d\n",
                                      i, tdaemon.record_burst_array[i]);
                }
        }
}
END_TEST

START_TEST(check_update_byte_array)
{
        setup();

        int current_minute = 7;
        tdaemon.byte_window_length = 15;
        size_t record_size = 32000;

        rate_limit_update(current_minute, tdaemon.byte_window_length,
                          tdaemon.byte_burst_array, record_size);
        rate_limit_update(current_minute, tdaemon.byte_window_length,
                          tdaemon.byte_burst_array, record_size);

        current_minute = 45;
        rate_limit_update(current_minute, tdaemon.byte_window_length,
                          tdaemon.byte_burst_array, record_size);

        current_minute = 47;
        rate_limit_update(current_minute, tdaemon.byte_window_length,
                          tdaemon.byte_burst_array, record_size);
        rate_limit_update(current_minute, tdaemon.byte_window_length,
                          tdaemon.byte_burst_array, record_size);
        rate_limit_update(current_minute, tdaemon.byte_window_length,
                          tdaemon.byte_burst_array, record_size);

        for (int i = 0; i < 60; i++) {
                if (i != 45 && i != 47) {
                        ck_assert_msg(tdaemon.byte_burst_array[i] == 0, "array at %d is %d\n",
                                      i, tdaemon.record_burst_array[i]);
                } else {
                        ck_assert_msg(tdaemon.byte_burst_array[45] == 32000, "bytearray: %d is %d\n",
                                      i, tdaemon.record_burst_array[i]);
                        ck_assert_msg(tdaemon.byte_burst_array[47] == 96000, "bytearray: %d is %d\n",
                                      i, tdaemon.record_burst_array[i]);
                }
        }
}
END_TEST

START_TEST(check_strategy_spool_option)
{
        setup();

        tdaemon.rate_limit_strategy = "spool";
        bool do_spool = true;
        char *ret = NULL;
        bool record_sent = false;

        // Get rate-limit strategy
        do_spool = spool_strategy_selected(&tdaemon);

        // Drop record
        if (!record_sent && !do_spool) {
                ret = "dropped";
        }
        // Spool Record
        else if (!record_sent && do_spool) {
                ret = "spooled";
        } else {
                ret = "sent";
        }

        ck_assert_str_eq(ret, "spooled");

}
END_TEST

START_TEST(check_strategy_drop_option)
{
        setup();

        tdaemon.rate_limit_strategy = "drop";
        bool do_spool = true;
        char *ret = NULL;
        bool record_sent = false;

        // Get rate-limit strategy
        do_spool = spool_strategy_selected(&tdaemon);

        // Drop record
        if (!record_sent && !do_spool) {
                ret = "dropped";
        }
        // Spool Record
        else if (!record_sent && do_spool) {
                ret = "spooled";
        } else {
                ret = "sent";
        }

        ck_assert_str_eq(ret, "dropped");

}
END_TEST

START_TEST(check_strategy_if_record_sent)
{
        setup();

        tdaemon.rate_limit_strategy = "drop";
        bool do_spool = true;
        char *ret = NULL;
        bool record_sent = true;

        // Get rate-limit strategy
        do_spool = spool_strategy_selected(&tdaemon);

        // Drop record
        if (!record_sent && !do_spool) {
                ret = "dropped";
        }
        // Spool Record
        else if (!record_sent && do_spool) {
                ret = "spooled";
        } else {
                ret = "sent";
        }

        ck_assert_str_eq(ret, "sent");

}
END_TEST

Suite *config_suite(void)
{
        // A suite is comprised of test cases, defined below
        Suite *s = suite_create("postd");

        // Individual unit tests are added to "test cases"
        TCase *t = tcase_create("postd");
        tcase_add_test(t, check_daemon_is_initialized);
        tcase_add_test(t, check_handle_client_with_no_data);
        tcase_add_test(t, check_handle_client_with_incorrect_data);
        tcase_add_test(t, check_process_record_with_correct_size_and_data);
        tcase_add_test(t, check_process_record_with_incorrect_headers);
        tcase_add_test(t, check_rate_limit_enabled_functions);
        tcase_add_test(t, check_rate_limit_records_that_pass);
        tcase_add_test(t, check_rate_limit_records_that_do_not_pass);
        tcase_add_test(t, check_rate_limit_bytes_that_pass);
        tcase_add_test(t, check_rate_limit_bytes_that_do_not_pass);
        tcase_add_test(t, check_update_record_array);
        tcase_add_test(t, check_update_byte_array);
        tcase_add_test(t, check_strategy_spool_option);
        tcase_add_test(t, check_strategy_drop_option);
        tcase_add_test(t, check_strategy_if_record_sent);

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
