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

/************************/
/* BEGIN INT UNIT TESTS */
/************************/

START_TEST(succeeds)
{
        ck_assert_int_eq(2, 2);
}
END_TEST

START_TEST(fails)
{
        ck_assert_int_eq(1, 2);
}
END_TEST

/************************/
/*  END INT UNIT TESTS  */
/************************/

Suite *telemetry_suite(void)
{
        // A suite is comprised of test cases, defined below
        Suite *s = suite_create("telemetry");

        // Individual unit tests are added to "test cases"
        TCase *t = tcase_create("integer equality");
        tcase_add_test(t, succeeds);
        //tcase_add_test(t, fails);

        // add more TCases here

        suite_add_tcase(s, t);

        return s;
}

int main(void)
{
        int failed;
        Suite *s;
        SRunner *sr;

        s = telemetry_suite();
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
