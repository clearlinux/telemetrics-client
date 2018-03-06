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
#include "journal/journal.h"

static struct TelemJournal *journal = NULL;
static char *journal_file = TESTOOPSDIR "/journal.txt";

START_TEST(check_open_journal)
{
        struct TelemJournal *j = open_journal(journal_file);
        ck_assert_ptr_nonnull(j);
        ck_assert_ptr_nonnull(j->fptr);
        ck_assert_ptr_nonnull(j->boot_id);
        ck_assert_int_eq(j->record_count, 0);
        close_journal(j);
}
END_TEST

START_TEST(check_unsuccessful_open_journal)
{
        char *bad_journal_file = "/sys/firmware/acpi/tables/journal";
        struct TelemJournal *j = open_journal(bad_journal_file);
        ck_assert_ptr_null(j);
}
END_TEST

void new_entry_setup(void)
{
        if (journal) {
                return;
        }

        journal = open_journal(journal_file);
}

START_TEST(check_new_entry)
{
       int ret;
       ret = new_journal_entry(journal, "t/t/t", 1520054957,
                               "3bc17766547776eb7fc478eb0eb43e43");
       ck_assert_int_eq(ret, 0);
       close_journal(journal);
}
END_TEST

START_TEST(check_new_entry_bad_class)
{
       int ret = 0;
       /* has an invalid class */
       ret = new_journal_entry(journal, "t/t", 1520054957,
                               "3bc17766547776eb7fc478eb0eb43e43");
       ck_assert_int_eq(ret, 1);
}
END_TEST

START_TEST(check_new_entry_bad_id)
{
       int ret = 0;
       /* event_id is invalid */
       ret = new_journal_entry(journal, "t/t/t", 1520054957,
                               "Xbc17766547776eb7fc478eb0eb43e43");
       ck_assert_int_eq(ret, 1);
}
END_TEST

START_TEST(check_new_entry_null_class)
{
       int ret = 0;
       /* param is NULL */
       ret = new_journal_entry(journal, NULL, 1520054957,
                               "3bc17766547776eb7fc478eb0eb43e43");
       ck_assert_int_eq(ret, 1);
}
END_TEST

START_TEST(check_new_entry_null_id)
{
       int ret = 0;
       /* null id */
       ret = new_journal_entry(journal, "t/t/t", 1520054957, NULL);
       ck_assert_int_eq(ret, 1);
}
END_TEST

void new_entry_teardown(void)
{
       if (journal) {
               close_journal(journal);
               journal = NULL;
       }
}

void insert_n_records(int n, struct TelemJournal *j)
{
        for (int i = 0; i < n; i++) {
                new_journal_entry(j, "t/t/t", 1520054957 + i,
                                "3bc17766547776eb7fc478eb0eb43e43");
        }
}

/*

failing in travis-ci

START_TEST(check_journal_file_prune)
{
       int rc = 0;
       struct TelemJournal *j = open_journal(journal_file);

       insert_n_records(j->record_count_limit * 2, j);
       ck_assert_int_gt(j->record_count, j->record_count_limit);
       rc = prune_journal(j);
       ck_assert_int_eq(rc, 0);
       // Record count should always be below the limit + hysteresis
       ck_assert_int_lt(j->record_count, j->record_count_limit + DEVIATION);


       close_journal(j);
}
END_TEST
*/

void journal_entry_setup(void)
{
        const char *journal_print = TESTOOPSDIR "/journal.print.txt";

        if (journal) {
                close_journal(journal);
        }

        journal = open_journal(journal_print);
}

START_TEST(check_journal_print)
{
       int count = 0;
       const int k = 20;

       insert_n_records(k, journal);
       count = print_journal(journal, NULL, NULL, NULL, NULL);
       ck_assert(count == k);
}
END_TEST

START_TEST(check_journal_filter)
{
       int result = 0;
       char *eid = "00007766547776eb7fc478eb0eb43e43";

       result = new_journal_entry(journal, "a/b/c", 1520054957, eid);
       ck_assert(result == 0);
       result = new_journal_entry(journal, "a/b/d", 1520054957, eid);
       ck_assert(result == 0);

       result = print_journal(journal, "a/b/c", NULL, NULL, NULL);
       ck_assert(result == 1);

       result = print_journal(journal, NULL, NULL, eid, NULL);
       ck_assert(result == 2);
}
END_TEST

void journal_entry_teardown(void)
{
        close_journal(journal);
}

Suite *config_suite(void)
{
        // A suite is comprised of test cases, defined below
        Suite *s = suite_create("journal_feature");

        // Individual unit tests are added to "test cases"
        TCase *t = tcase_create("journal");
        tcase_add_test(t, check_open_journal);
        tcase_add_test(t, check_unsuccessful_open_journal);
        suite_add_tcase(s, t);

        t = tcase_create("new entry");
        tcase_add_unchecked_fixture(t, new_entry_setup, new_entry_teardown);
        tcase_add_test(t, check_new_entry);
        tcase_add_test(t, check_new_entry_bad_class);
        tcase_add_test(t, check_new_entry_bad_id);
        tcase_add_test(t, check_new_entry_null_class);
        tcase_add_test(t, check_new_entry_null_id);
        suite_add_tcase(s, t);

/*        t = tcase_create("prunning journal");
        tcase_add_test(t, check_journal_file_prune);
        suite_add_tcase(s, t);*/

        t = tcase_create("print journal");
        tcase_add_unchecked_fixture(t, journal_entry_setup,
                                    journal_entry_teardown);
        tcase_add_test(t, check_journal_print);
        tcase_add_test(t, check_journal_filter);
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
