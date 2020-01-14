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
#include <check.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "telemetry.h"

static struct telem_ref *ref = NULL;
static char *original_event_id = NULL;

void create_setup(void)
{
        int ret;

        /* We've already called tm_create_record() */
        if (ref) {
                return;
        }
        ret = tm_create_record(&ref, 1, "t/t/t", 2000);

        /* FIXME: it would be better to SKIP the tests in this case, because
         * they depend on an opt-in state, but I haven't figured out how to SKIP
         * with the libcheck API...
         */
        ck_assert_msg(ret != -ECONNREFUSED,
                      "First time opt-in required to run test");
}

START_TEST(record_create_non_null)
{
        ck_assert_msg(ref != NULL, "Returned unintialized reference");
        ck_assert_msg(ref->record != NULL, "Returned unintialized record in reference");
}
END_TEST

START_TEST(record_create_header_size)
{
        ck_assert_msg(ref->record->header_size > 0, "Found zero-length header");
}
END_TEST

START_TEST(record_create_severity)
{
        char *result;
        if (asprintf(&result, "%s: %u\n", TM_SEVERITY_STR, 1) < 0) {
                return;
        }
        ck_assert_str_eq(ref->record->headers[TM_SEVERITY], result);
        free(result);
}
END_TEST

START_TEST(record_create_classification)
{
        char *result;
        if (asprintf(&result, "%s: %s\n", TM_CLASSIFICATION_STR, "t/t/t") < 0) {
                return;
        }
        ck_assert_str_eq(ref->record->headers[TM_CLASSIFICATION], result);
        free(result);
}
END_TEST

START_TEST(record_create_version)
{
        char *result;
        if (asprintf(&result, "%s: %u\n", TM_PAYLOAD_VERSION_STR, 2000) < 0) {
                return;
        }
        ck_assert_str_eq(ref->record->headers[TM_PAYLOAD_VERSION], result);
        free(result);
}
END_TEST

void create_teardown(void)
{
        if (ref) {
                tm_free_record(ref);
                ref = NULL;
        }
}

START_TEST(is_opt_in)
{
        int ret;
        ret = tm_is_opted_in();
        /* Smoke testing function */
        ck_assert_msg(ret == 0 || ret == 1,
                      "Something wrong with opt-in check");
}
END_TEST

START_TEST(record_create_invalid_class1)
{
        int ret;
        ret = tm_create_record(&ref, 1, "t/t", 2000);
        ck_assert_msg(ret != -ECONNREFUSED,
                      "First time opt-in required to run test");
        ck_assert(ret == -EINVAL);
}
END_TEST

START_TEST(record_create_invalid_class2)
{
        int ret;
        ret = tm_create_record(&ref, 1, "t/t/t/t", 2000);
        ck_assert_msg(ret != -ECONNREFUSED,
                      "First time opt-in required to run test");
        ck_assert(ret == -EINVAL);
}
END_TEST

START_TEST(record_create_severity_underflow)
{
        char *result;
        int ret;

        // Severity of 0 is too low; raise it to 1, the minimum
        ret = tm_create_record(&ref, 0, "a/a/a", 2000);
        ck_assert_msg(ret != -ECONNREFUSED,
                      "First time opt-in required to run test");

        if (asprintf(&result, "%s: %u\n", TM_SEVERITY_STR, 1) < 0) {
                return;
        }
        ck_assert_str_eq(ref->record->headers[TM_SEVERITY], result);
        free(result);

        create_teardown();
}
END_TEST

START_TEST(record_create_severity_overflow)
{
        char *result;
        int ret;

        // Severity of 5 is too high; lower it to 4, the maximum
        ret = tm_create_record(&ref, 5, "b/b/b", 2000);
        ck_assert_msg(ret != -ECONNREFUSED,
                      "First time opt-in required to run test");

        if (asprintf(&result, "%s: %u\n", TM_SEVERITY_STR, 4) < 0) {
                return;
        }
        ck_assert_str_eq(ref->record->headers[TM_SEVERITY], result);
        free(result);

        create_teardown();
}
END_TEST

void event_id_setup(void)
{
        int ret;

        if (ref) {
                return;
        }

        ret = tm_create_record(&ref, 1, "t/t/t", 2000);
        original_event_id = strdup(ref->record->headers[TM_EVENT_ID]);
        if (!original_event_id) {
                return;
        }
        ck_assert_msg(ret != -ECONNREFUSED,
                      "First time opt-in required to run test");
}

START_TEST(record_set_event_id)
{
        int ret;
        char *event_id = "aaaaaa00000033333344444466666622";
        char *result;
        if (asprintf(&result, "%s: %s\n", TM_EVENT_ID_STR, event_id) < 0) {
                return;
        }
        ck_assert_str_ne(ref->record->headers[TM_EVENT_ID], result);
        ret = tm_set_event_id(ref, event_id);
        ck_assert_int_eq(ret, 0);
        ck_assert_str_eq(ref->record->headers[TM_EVENT_ID], result);
        free(result);
}
END_TEST

START_TEST(record_set_event_id_invalid_chars)
{
        int ret;
        char *event_id = "aaaaaa000000333333444444666666ZZ";
        ret = tm_set_event_id(ref, event_id);
        ck_assert_int_eq(ret, -1);
        ck_assert_str_eq(ref->record->headers[TM_EVENT_ID], original_event_id);
}
END_TEST

START_TEST(record_set_event_id_null)
{
        int ret;
        char *event_id = NULL;
        ret = tm_set_event_id(ref, event_id);
        ck_assert_int_eq(ret, -1);
        ck_assert_str_eq(ref->record->headers[TM_EVENT_ID], original_event_id);
}
END_TEST

START_TEST(record_set_event_id_short)
{
        int ret;
        char *event_id = "aaaa";
        ret = tm_set_event_id(ref, event_id);
        ck_assert_int_eq(ret, -1);
        ck_assert_str_eq(ref->record->headers[TM_EVENT_ID], original_event_id);
}
END_TEST

START_TEST(record_set_event_id_long)
{
        int ret;
        char *event_id = "0000000000000000000000000000000000000000000";
        ret = tm_set_event_id(ref, event_id);
        ck_assert_int_eq(ret, -1);
        ck_assert_str_eq(ref->record->headers[TM_EVENT_ID], original_event_id);
}
END_TEST

void event_id_teardown(void)
{
        // Free record
        create_teardown();
        free(original_event_id);
}

Suite *lib_suite(void)
{
        Suite *s = suite_create("libtelemetry");

        TCase *t = tcase_create("record creation");
        tcase_add_unchecked_fixture(t, create_setup, create_teardown);
        tcase_add_test(t, record_create_non_null);
        tcase_add_test(t, record_create_header_size);
        tcase_add_test(t, record_create_severity);
        tcase_add_test(t, record_create_classification);
        tcase_add_test(t, record_create_version);
        suite_add_tcase(s, t);

        t = tcase_create("Opt-in");
        tcase_add_test(t, is_opt_in);
        suite_add_tcase(s, t);

        t = tcase_create("invalid classification");
        tcase_add_test(t, record_create_invalid_class1);
        tcase_add_test(t, record_create_invalid_class2);
        suite_add_tcase(s, t);

        t = tcase_create("clamped severity range");
        tcase_add_test(t, record_create_severity_underflow);
        tcase_add_test(t, record_create_severity_overflow);
        suite_add_tcase(s, t);

        t = tcase_create("event id");
        tcase_add_unchecked_fixture(t, event_id_setup, event_id_teardown);
        tcase_add_test(t, record_set_event_id);
        tcase_add_test(t, record_set_event_id_invalid_chars);
        tcase_add_test(t, record_set_event_id_null);
        tcase_add_test(t, record_set_event_id_short);
        tcase_add_test(t, record_set_event_id_long);
        suite_add_tcase(s, t);

        return s;
}

int main(void)
{
        Suite *s;
        SRunner *sr;
        int failed;

        s = lib_suite();
        sr = srunner_create(s);

        srunner_set_log(sr, NULL);
        srunner_set_tap(sr, "-");

        // set CK_NOFORK to attach gdb
        // srunner_set_fork_status(sr, CK_NOFORK);
        srunner_run_all(sr, CK_SILENT);
        failed = srunner_ntests_failed(sr);
        srunner_free(sr);

        return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
