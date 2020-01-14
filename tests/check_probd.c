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
#include "configuration_check.h"
#include "telemdaemon.h"
#include "common.h"

TelemDaemon tdaemon;

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

void setup(void)
{
        char *config_file = ABSTOPSRCDIR "/src/data/example.conf";
        set_config_file(config_file);

        initialize_probe_daemon(&tdaemon);
}

void teardown(void)
{
        free_config_file();
}

START_TEST(check_daemon_is_initialized)
{
        setup();

        ck_assert(tdaemon.nfds == 0);
        ck_assert(tdaemon.pollfds == NULL);

        teardown();
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
        add_pollfd(&tdaemon, 1, events);
        add_pollfd(&tdaemon, 2, events);
        add_pollfd(&tdaemon, 3, events);
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

        /* Cleaning alloctions */
        free(tdaemon.pollfds);
        teardown();
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

        teardown();
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

        teardown();
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

        teardown();
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

        teardown();
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

        ssize_t ret = write(server_fd, buf, 2 * sizeof(uint32_t) + size + 1);
        ck_assert(ret != -1);
        processed = handle_client(&tdaemon, 0, cl);
        ck_assert(processed == true);

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
                        "bios_version: Qemu\n"
                        "event_id: 3a2d799826edc6266d72824d2aac6763\n";
        char *post_body = "test message";

        set_up_socket_pair(&client_fd, &server_fd);
        cl = add_client(&(tdaemon.client_head), client_fd);
        ck_assert_msg(cl != NULL, "failed to malloc client");
        add_pollfd(&tdaemon, client_fd, POLLIN | POLLPRI);

        record = get_serialized_record(headers, post_body, &record_size);
        ssize_t ret = write(server_fd, record, record_size);
        ck_assert(ret == record_size);

        processed = handle_client(&tdaemon, 0, cl);
        ck_assert(processed == true);
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

        record = get_serialized_record(headers, post_body, &record_size);
        ssize_t ret = write(server_fd, record, record_size);
        ck_assert(ret == record_size);

        processed = handle_client(&tdaemon, 0, cl);
        ck_assert(processed == true);
        ck_assert_msg(is_client_list_empty(&(tdaemon.client_head)), "Failed to remove client with incorrect headers\n");
        ck_assert_msg(tdaemon.nfds == 0, "Failed to remove poll fd for client with incorrect headers\n");
        close(server_fd);
        free(record);

        teardown();
}
END_TEST

Suite *config_suite(void)
{
        // A suite is comprised of test cases, defined below
        Suite *s = suite_create("probd");

        // Individual unit tests are added to "test cases"
        TCase *t = tcase_create("probd");
        tcase_add_test(t, check_add_del_poll_fd);
        tcase_add_test(t, check_daemon_is_initialized);
        tcase_add_test(t, check_add_remove_client);
        tcase_add_test(t, check_handle_client_with_no_data);
        tcase_add_test(t, check_handle_client_with_incorrect_data);
        tcase_add_test(t, check_handle_client_with_incorrect_size);
        tcase_add_test(t, check_handle_client_with_correct_size);
        tcase_add_test(t, check_process_record_with_correct_size_and_data);
        tcase_add_test(t, check_process_record_with_incorrect_headers);

        suite_add_tcase(s, t);

        return s;
}

int main(void)
{
        Suite *s;
        SRunner *sr;
        int failed;

        s = config_suite();
        sr = srunner_create(s);

        // Use the TAP driver for now, so that each
        // unit test will PASS/FAIL in the log output.
        srunner_set_log(sr, NULL);
        srunner_set_tap(sr, "-");

        // set CK_NOFORK to attach gdb
        // srunner_set_fork_status(sr, CK_NOFORK);
        srunner_run_all(sr, CK_SILENT);
        failed = srunner_ntests_failed(sr);
        srunner_free(sr);

        // if you want the TAP driver to report a hard error based
        // on certain conditions (e.g. number of failed tests, etc.),
        // return non-zero here instead.
        return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
