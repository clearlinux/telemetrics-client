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
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <unistd.h>

#include "configuration.h"
#include "telemdaemon.h"
#include "common.h"

TelemDaemon tdaemon;
static bool post_was_called = false;

char *get_serialized_record(char *headers, char *post_body, size_t *record_size)
{
        size_t totalsize, headersize, payloadsize, offset;
        char *data;

        offset = 0;
        headersize = strlen(headers);
        payloadsize = strlen(post_body);
        totalsize = headersize + payloadsize;

        *record_size = 2 * sizeof(uint32_t) + totalsize + 1;
        data = malloc(*record_size);
        memset(data, 0, *record_size);
        memcpy(data, &totalsize, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        memcpy(data + offset, &headersize, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        memcpy(data + offset, headers, headersize);
        offset += headersize;

        memcpy(data + offset, post_body, payloadsize + 1);
        return data;
}

bool dummy_post(char *headers[], char *body, bool spool)
{
        post_was_called = true;
        return post_was_called;
}

bool (*post_record_ptr)(char *[], char *, bool) = dummy_post;

void setup(void)
{
        char *config_file = ABSTOPSRCDIR "/src/data/example.conf";
        set_config_file(config_file);

        initialize_daemon(&tdaemon);
}

START_TEST(check_daemon_is_initialized)
{
        setup();

        ck_assert(tdaemon.nfds == 0);
        ck_assert(tdaemon.pollfds == NULL);
        ck_assert(tdaemon.rate_limit_enabled == true);
        ck_assert(tdaemon.record_burst_limit == 100);
        ck_assert(tdaemon.record_window_length == 15);
        ck_assert(tdaemon.byte_burst_limit == 1000);
        ck_assert(tdaemon.byte_window_length == 20);
        ck_assert_str_eq(tdaemon.rate_limit_strategy, "spool");
}
END_TEST

int get_poll_fd(TelemDaemon *tdaemon, nfds_t i)
{
        if (i >= 0 && i < tdaemon->nfds) {
                return tdaemon->pollfds[i].fd;
        }
        return -1;
}

START_TEST(check_add_del_poll_fd)
{
        setup();

        int fd = 1;
        int short events = 1;
        add_pollfd(&tdaemon, fd, events);
        add_pollfd(&tdaemon, ++fd, events);
        add_pollfd(&tdaemon, ++fd, events);
        ck_assert_msg(tdaemon.nfds == 3, "Failed to add pollfd");

        fd = get_poll_fd(&tdaemon, 0);
        ck_assert(fd == 1);
        fd = get_poll_fd(&tdaemon, 1);
        ck_assert(fd == 2);
        fd = get_poll_fd(&tdaemon, 2);
        ck_assert(fd == 3);

        del_pollfd(&tdaemon, 1);
        ck_assert_msg(tdaemon.nfds == 2, "Failed to delete pollfd");
        fd = get_poll_fd(&tdaemon, 1);
        ck_assert(fd == 3);

        del_pollfd(&tdaemon, 1);
        ck_assert_msg(tdaemon.nfds == 1, "Failed to delete pollfd");
        fd = get_poll_fd(&tdaemon, 1);
        ck_assert(fd == -1);

        del_pollfd(&tdaemon, 0);
        ck_assert_msg(tdaemon.nfds == 0, "Failed to delete pollfd");
        fd = get_poll_fd(&tdaemon, 0);
        ck_assert(fd == -1);
}
END_TEST

START_TEST(check_add_remove_client)
{
        setup();

        client *cl, *cl1, *cl2, *cl3;
        cl1 = add_client(&(tdaemon.client_head), 1);
        cl2 = add_client(&(tdaemon.client_head), 2);
        cl3 = add_client(&(tdaemon.client_head), 3);

        int i = 3;
        LIST_FOREACH(cl, &(tdaemon.client_head), client_ptrs) {
                ck_assert_msg((cl->fd == i), "Actual value is %d\n", cl->fd);
                i--;
        }

        remove_client(&(tdaemon.client_head), cl1);
        remove_client(&(tdaemon.client_head), cl2);
        remove_client(&(tdaemon.client_head), cl3);
        //ck_assert(tdaemon.client_head.lh_first == NULL);
        ck_assert_msg(is_client_list_empty(&(tdaemon.client_head)), "Failed to remove clients\n");
}
END_TEST

void set_up_socket_pair(int *client, int *server)
{
        int sv[2];
        int ret;

        ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
        ck_assert_msg(ret == 0, "Failed to create socket pair\n");
        ck_assert_msg((fcntl(sv[0], F_SETFL, O_NONBLOCK) == 0),
                      "Failed to set socket to non-blocking\n");
        ck_assert_msg((fcntl(sv[1], F_SETFL, O_NONBLOCK) == 0),
                      "Failed to set socket to non-blocking\n");
        *client = sv[0];
        *server = sv[1];
}

START_TEST(check_handle_client_with_no_data)
{
        setup();

        client *cl;

        int server_fd;
        int client_fd;
        bool processed;

        set_up_socket_pair(&client_fd, &server_fd);
        cl = add_client(&(tdaemon.client_head), client_fd);
        ck_assert_msg(cl != NULL, "failed to malloc client");
        add_pollfd(&tdaemon, client_fd, POLLIN | POLLPRI);

        processed = handle_client(&tdaemon, 0, cl);
        ck_assert(processed == false);
        ck_assert_msg(is_client_list_empty(&(tdaemon.client_head)), "Failed to remove client with no data\n");
        ck_assert_msg(tdaemon.nfds == 0, "Failed to remove poll fd for client with n data\n");
        close(server_fd);
}
END_TEST

START_TEST(check_handle_client_with_incorrect_data)
{
        setup();

        client *cl;
        char *buf = "test";
        int server_fd;
        int client_fd;
        bool processed;

        set_up_socket_pair(&client_fd, &server_fd);
        cl = add_client(&(tdaemon.client_head), client_fd);
        ck_assert_msg(cl != NULL, "failed to malloc client");
        add_pollfd(&tdaemon, client_fd, POLLIN | POLLPRI);

        ssize_t ret = write(server_fd, buf, 2);
        ck_assert(ret == 2);

        processed = handle_client(&tdaemon, 0, cl);
        ck_assert(processed == false);
        ck_assert_msg(is_client_list_empty(&(tdaemon.client_head)), "Failed to remove client with no data\n");
        ck_assert_msg(tdaemon.nfds == 0, "Failed to remove poll fd for client with n data\n");
        close(server_fd);
}
END_TEST

START_TEST(check_handle_client_with_incorrect_size)
{
        setup();

        client *cl;
        int server_fd;
        int client_fd;
        bool processed;
        char *data = "test";
        char buf[4096];

        set_up_socket_pair(&client_fd, &server_fd);
        cl = add_client(&(tdaemon.client_head), client_fd);
        ck_assert_msg(cl != NULL, "failed to malloc client");
        add_pollfd(&tdaemon, client_fd, POLLIN | POLLPRI);

        int size = 10;
        memset(buf, 0, 4096);
        memcpy(buf, &size, RECORD_SIZE_LEN);
        memcpy(buf + RECORD_SIZE_LEN, data, sizeof(uint32_t));
        ssize_t ret = write(server_fd, buf, 2);
        ck_assert(ret == 2);

        processed = handle_client(&tdaemon, 0, cl);
        ck_assert(processed == false);
        ck_assert_msg(is_client_list_empty(&(tdaemon.client_head)), "Failed to remove client with no data\n");
        ck_assert_msg(tdaemon.nfds == 0, "Failed to remove poll fd for client with n data\n");
        close(server_fd);
}
END_TEST

START_TEST(check_handle_client_with_correct_size)
{
        setup();

        client *cl;
        int server_fd;
        int client_fd;
        bool processed;
        char *data = "testing hello world message for handle client check";
        char buf[256];

        set_up_socket_pair(&client_fd, &server_fd);
        cl = add_client(&(tdaemon.client_head), client_fd);
        ck_assert_msg(cl != NULL, "failed to malloc client");
        add_pollfd(&tdaemon, client_fd, POLLIN | POLLPRI);

        size_t size = strlen(data);
        memset(buf, 0, 256);
        memcpy(buf, &size, RECORD_SIZE_LEN);
        memcpy(buf + RECORD_SIZE_LEN, &size, sizeof(uint32_t));
        memcpy(buf + 2 * sizeof(uint32_t), data, strlen(data) + 1);
        ck_assert(strcmp(data, buf + 2 * sizeof(int32_t)) == 0);

        post_was_called = false;
        ssize_t ret = write(server_fd, buf, 2 * sizeof(uint32_t) + size + 1);
        ck_assert(ret != -1);
        processed = handle_client(&tdaemon, 0, cl);
        ck_assert(processed == true);
        ck_assert(post_was_called == false);

        ck_assert_msg(is_client_list_empty(&(tdaemon.client_head)), "Failed to remove client with no data\n");
        ck_assert_msg(tdaemon.nfds == 0, "Failed to remove poll fd for client with n data\n");
        close(server_fd);
}
END_TEST

START_TEST(check_process_record_with_correct_size_and_data)
{
        setup();

        client *cl;
        int server_fd, client_fd;
        bool processed;
        char *record;
        size_t record_size;
        char *headers = "record_format_version: 1\nclassification: crash/kernel/bug\nseverity: 0\n"
                        "machine_id: 1234\ncreation_timestamp: 1418672344\narch:x86_64\n"
                        "host_type: macbookpro\nbuild: 200\nkernel_version: 3.15\n"
                        "payload_format_version: 1\n"
                        "system_name: clear-linux-os\n"
                        "board_name: Qemu|Intel\n"
                        "cpu_model: Intel(R) Core(TM) i7-5650U CPU @ 2.20GHz\n"
                        "bios_version: Qemu\n";
        char *post_body = "test message";

        set_up_socket_pair(&client_fd, &server_fd);
        cl = add_client(&(tdaemon.client_head), client_fd);
        ck_assert_msg(cl != NULL, "failed to malloc client");
        add_pollfd(&tdaemon, client_fd, POLLIN | POLLPRI);

        post_was_called = false;
        record = get_serialized_record(headers, post_body, &record_size);
        ssize_t ret = write(server_fd, record, record_size);
        ck_assert(ret == record_size);

        processed = handle_client(&tdaemon, 0, cl);
        ck_assert(processed == true);
        ck_assert(post_was_called == true);
        ck_assert_msg(is_client_list_empty(&(tdaemon.client_head)), "Failed to remove client with correct data\n");
        ck_assert_msg(tdaemon.nfds == 0, "Failed to remove poll fd for client with correct data\n");
        close(server_fd);
        free(record);
}
END_TEST

START_TEST(check_process_record_with_incorrect_headers)
{
        setup();

        client *cl;
        int server_fd, client_fd;
        bool processed;
        char *record;
        size_t record_size;
        char *headers = "record_format_version: 1\nclassificatioooon: crash/kernel/bug\nseverity: 0\n"
                        "machine_id: 1234\ncreation_timestamp: 1418672344\narch:x86_64\n"
                        "host_type: macbookpro\nbuild: 200\nkernel_version: 3.15\n";
        char *post_body = "test message";

        set_up_socket_pair(&client_fd, &server_fd);
        cl = add_client(&(tdaemon.client_head), client_fd);
        ck_assert_msg(cl != NULL, "failed to malloc client");
        add_pollfd(&tdaemon, client_fd, POLLIN | POLLPRI);

        post_was_called = false;
        record = get_serialized_record(headers, post_body, &record_size);
        ssize_t ret = write(server_fd, record, record_size);
        ck_assert(ret == record_size);

        processed = handle_client(&tdaemon, 0, cl);
        ck_assert(processed == true);
        ck_assert(post_was_called == false);
        ck_assert_msg(is_client_list_empty(&(tdaemon.client_head)), "Failed to remove client with incorrect headers\n");
        ck_assert_msg(tdaemon.nfds == 0, "Failed to remove poll fd for client with incorrect headers\n");
        close(server_fd);
        free(record);
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
        Suite *s = suite_create("daemon");

        // Individual unit tests are added to "test cases"
        TCase *t = tcase_create("daemon");
        tcase_add_test(t, check_add_del_poll_fd);
        tcase_add_test(t, check_daemon_is_initialized);
        tcase_add_test(t, check_add_remove_client);
        tcase_add_test(t, check_handle_client_with_no_data);
        tcase_add_test(t, check_handle_client_with_incorrect_data);
        tcase_add_test(t, check_handle_client_with_incorrect_size);
        tcase_add_test(t, check_handle_client_with_correct_size);
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
