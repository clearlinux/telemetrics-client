/*
 * This program is part of the Clear Linux Project
 *
 * Copyright 2017 Intel Corporation
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


#define OUT_MAX_LEN 100

#include <stdio.h>
#include <check.h>
#include "nica/b64enc.h"


START_TEST(check_nc_b64_no_overflow)
{
        size_t n = 2;
        char out[OUT_MAX_LEN];
        const char *filename = TOPSRCDIR "/tests/nc_b64enc_test_files/foobar";

        ck_assert_msg(!nc_b64enc_filename(filename, &out[0], n), "Should quit since buffer smaller than content");
}
END_TEST


START_TEST(check_nc_b64_enc_n_file_empty)
{
        size_t n = OUT_MAX_LEN;
        char out[OUT_MAX_LEN];
        char *result = "\0";
        const char *filename = TOPSRCDIR "/tests/nc_b64enc_test_files/empty";

        ck_assert_msg(nc_b64enc_filename(filename, &out[0], n), "Error opening file");
        ck_assert_str_eq(out, result);
}
END_TEST


START_TEST(check_nc_b64_enc_n_file_fo)
{
        size_t n = OUT_MAX_LEN;
        char out[OUT_MAX_LEN];
        char *result = "Zm8=";
        const char *filename = TOPSRCDIR "/tests/nc_b64enc_test_files/fo";

        ck_assert_msg(nc_b64enc_filename(filename, &out[0], n), "Error opening file");
        ck_assert_str_eq(out, result);
}
END_TEST

START_TEST(check_nc_b64_enc_n_file_foob)
{
        size_t n = OUT_MAX_LEN;
        char out[OUT_MAX_LEN];
        char *result = "Zm9vYg==";
        const char *filename = TOPSRCDIR "/tests/nc_b64enc_test_files/foob";

        ck_assert_msg(nc_b64enc_filename(filename, &out[0], n), "Error opening file");
        ck_assert_str_eq(out, result);
}
END_TEST

START_TEST(check_nc_b64_enc_n_file_foobar)
{
        size_t n = OUT_MAX_LEN;
        char out[OUT_MAX_LEN];
        char *result = "Zm9vYmFyZm9vYmFy";
        const char *filename = TOPSRCDIR "/tests/nc_b64enc_test_files/foobar";

        ck_assert_msg(nc_b64enc_filename(filename, &out[0], n), "Error opening file");
        ck_assert_str_eq(out, result);
}
END_TEST

START_TEST(check_nc_b64_enc_n_file_long_text)
{
        size_t out_len = 8000;
        char out[out_len];
        const char *expected_out = \
"TG9yZW0gaXBzdW0gZG9sb3Igc2l0IGFtZXQsIG5vbnVtbXkgdml0YWUsIGluIHZpdmFtdXMgc3Vz\n"
"cGVuZGlzc2UgYWMuIFNlbXBlciBzZWQgcGhhcmV0cmEgc2NlbGVyaXNxdWUuIEVnZXQgZWxlaWZl\n"
"bmQgYW1ldCB2ZWwgbnVuYyB2b2x1dHBhdCBjdXJzdXMsIGF1Y3RvciBwbGF0ZWEgcHJldGl1bSwg\n"
"bWF0dGlzIHJpc3VzIGZhY2lsaXNpcyBmYXVjaWJ1cywgc3VzcGVuZGlzc2UgZGlhbSwgbGVjdHVz\n"
"IG1ldHVzIG51bGxhLiBOdW5jIGp1c3RvIGZhY2lsaXNpIG5hbSBmZWxpcyB2ZWwgbGFvcmVldCwg\n"
"bmliaCBsZW8gbWFzc2Egc3VzcGVuZGlzc2UgYWNjdW1zYW4gY29udmFsbGlzLCBzZWQgcXVpcyBw\n"
"ZWxsZW50ZXNxdWUgZWdlc3RhcywgbG9yZW0gYW50ZSBtb3JiaSBtYXR0aXMgdml0YWUuIFByYWVz\n"
"ZW50IGluIHJ1dHJ1bSBlZ2V0IGFjLCB2aXZhbXVzIHBlZGUgc3VzcGVuZGlzc2UgbGVjdHVzIG51\n"
"bGxhbSwgZXQgcGVkZSBldSBvZGlvIHBlZGUsIGV0aWFtIGxlbyBlZ2VzdGFzIGluLCBub251bW15\n"
"IHNlbXBlci4gQSBhY2N1bXNhbiBkdWksIHZlc3RpYnVsdW0gcmlkaWN1bHVzIGV0IGluLCBqdXN0\n"
"byBldCBwbGFjZXJhdCBkdWlzIHV0IHZpdmFtdXMgbGliZXJvLiBWZXN0aWJ1bHVtIGVnZXQgdXQg\n"
"bW9sbGlzLCBudW5jIGEgc29sbGljaXR1ZGluLCBmcmluZ2lsbGEgZXJvcyBwb3N1ZXJlIG1vbGxp\n"
"cyBhYyBuYXRvcXVlIHBlZGUsIHJpc3VzIG51bGxhIHJ1dHJ1bSB0dXJwaXMgYSB2aXRhZSBpZC4g\n"
"SW5jZXB0b3MgdGluY2lkdW50IHF1aWRlbSBhcmN1IHR1cnBpcyBudW5jIHNvbGxpY2l0dWRpbi4g\n"
"UGhhc2VsbHVzIG1ldHVzIGVyYXQgcGxhY2VyYXQsIGluIGVyYXQgbG9yZW0gbWF1cmlzIG51bGxh\n"
"LCBhdCBsaWJlcm8gZXJhdCBzdXNwZW5kaXNzZSBzYXBpZW4gbW9udGVzIHJob25jdXMsIG1hZ25h\n"
"IHF1YW0uIEp1c3RvIHByYWVzZW50IGxhY3VzIG1hc3NhLCByZXByZWhlbmRlcml0IG51bmMsIHJp\n"
"c3VzIGRpZ25pc3NpbSBmZWxpcyBub24sIGRpZ25pc3NpbSBvZGlvIHRlbGx1cyBhbGlxdWFtLCBl\n"
"bmltIHNvZGFsZXMgdGVtcG9yIHF1aXNxdWUgc29kYWxlcyBtYWduaXMgcG9ydHRpdG9yLiBMaWd1\n"
"bGEgc2VkIHJpc3VzLCBldSBlc3QgcXVpc3F1ZSwgY2xhc3Mgbm9uIGF1dGUgZXUsIHBsYXRlYSBz\n"
"Y2VsZXJpc3F1ZSBpZCBpbnRlZ2VyIGRpZ25pc3NpbW9zIGludGVnZXIgc3VzY2lwaXQuClZlaGlj\n"
"dWxhIGVpdXMgbmVjLCBjb252YWxsaXMgbWFlY2VuYXMgbGVjdHVzIHB1cnVzIHF1aXNxdWUgYWxp\n"
"cXVhbSwgZXQgZXQsIGxhY2luaWEgZGlzIGRpZ25pc3NpbSBsb3JlbS4gVm9sdXB0YXRlbSB1dCwg\n"
"dml2ZXJyYSBhZW5lYW4gcGhhc2VsbHVzIGZlbGlzLCBmZXJtZW50dW0gbGlndWxhIGVnZXQgdWxs\n"
"YW1jb3JwZXIgYW1ldCwgbWF1cmlzIGxlbyBsdWN0dXMuIEV0IGV0aWFtIHNlbXBlciBwaGFyZXRy\n"
"YSBuaWJoIG1pIG1hdXJpcy4gSGVuZHJlcml0IHNlZCBhbGlxdWFtLCBlZ2V0IGlkIHRlbGx1cyBt\n"
"YWduYSwgc2VkIGxhb3JlZXQgdWx0cmljZXMgZW5pbSwgbW9sbGlzIHN1c3BlbmRpc3NlIGluLiBF\n"
"dSBudWxsYW0gbG9yZW0gc2l0IGV0aWFtLCBvcmNpIGluIGxpYmVyby4gTnVuYyBpcHN1bSBtYXVy\n"
"aXMgZXQgc2VtIGhhYy4gRGljdHVtIGZhdWNpYnVzIGRpcyB2aXRhZSBpbiB2b2x1dHBhdCBtb3Ji\n"
"aSwgYSBkaWN0dW0sIHF1aXNxdWUgbWF1cmlzLCBhdWN0b3IgbmVjIHZlbCBwZWxsZW50ZXNxdWUg\n"
"dWxsYW1jb3JwZXIgZXRpYW0uIEN1bSBsaWJlcm8gd2lzaSBhY2N1bXNhbiBhbGlxdWFtIGNvbnNl\n"
"Y3RldHVlciB0ZWxsdXMsIHBoYXNlbGx1cyBjdXJhZSwgYXQgaWxsdW0gYW50ZSBwcmV0aXVtIG5p\n"
"YmggbW9yYmkuIFNlZCBibGFuZGl0IHB1bHZpbmFyIHB1bHZpbmFyLiBEaWN0dW0gZXN0IG51bmMg\n"
"ZWxlaWZlbmQsIHZlbGl0IHZlbGl0IHRlbXBvciBhY2N1bXNhbiBsb2JvcnRpcyBsYW9yZWV0IGNv\n"
"bmd1ZS4gU2l0IGFtZXQsIHNlZCBuaWJoIHBvcnJvIG5lcXVlIGF1Y3RvciBoeW1lbmFlb3MgcG9z\n"
"dWVyZSwgb2RpbyBudWxsYSBibGFuZGl0IGNvbmd1ZSBlbGl0LiBJZCBmYXVjaWJ1cyBldCB0ZW1w\n"
"dXMgbWFsZXN1YWRhIHBsYXRlYS4=";
        const char *filename = TOPSRCDIR "/tests/nc_b64enc_test_files/long_text";

        ck_assert_msg(nc_b64enc_filename(filename, &out[0], out_len), "Error opening file");
        ck_assert_str_eq(out, expected_out);
}
END_TEST

START_TEST(check_nc_b64_enc_n_filehandler_foobar)
{
        size_t n = OUT_MAX_LEN;
        char out[OUT_MAX_LEN];
        char *result = "Zm9vYmFyZm9vYmFy";
        const char *filename = TOPSRCDIR "/tests/nc_b64enc_test_files/foobar";
        FILE *fh = NULL;

        fh = fopen(filename, "rb");

        if (fh == NULL) {
                ck_abort_msg("Unable to open foobar file");
        }

        ck_assert_msg(nc_b64enc_file(fh, &out[0], n), "Result does not match");
        ck_assert_str_eq(out, result);
}
END_TEST

Suite *config_suite(void)
{
        // A suite is comprised of test cases, defined below
        Suite *s = suite_create("nc_b64enc");

        // Individual unit tests are added to "test cases"
        TCase *t = tcase_create("nc_b64enc");
        tcase_add_test(t, check_nc_b64_no_overflow);
        tcase_add_test(t, check_nc_b64_enc_n_file_empty);
        tcase_add_test(t, check_nc_b64_enc_n_file_fo);
        tcase_add_test(t, check_nc_b64_enc_n_file_foob);
        tcase_add_test(t, check_nc_b64_enc_n_file_foobar);
        tcase_add_test(t, check_nc_b64_enc_n_file_long_text);
        tcase_add_test(t, check_nc_b64_enc_n_filehandler_foobar);

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


