/*
 * This program is part of the Clear Linux Project
 *
 * Copyright 2015-2019 Intel Corporation
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
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <unistd.h>

#include "oops_parser.h"
#include "log.h"
#include "probe.h"

#define NUM_TAINTED_FLAGS 16

enum tm_severity {
        TM_LOW = 1,
        TM_MEDIUM,
        TM_HIGH,
        TM_CRITICAL,
};

struct oops_pattern oops_patterns_arr[] = {
        {
                "NETDEV WATCHDOG: ",
                "org.clearlinux/kernel/warning",
                TM_MEDIUM,
                false,
        },
        {
                "sysctl table check failed: ",
                "org.clearlinux/kernel/warning",
                TM_MEDIUM,
        },
        {
                "IRQ handler type mismatch for IRQ",
                "org.clearlinux/kernel/warning",
                TM_MEDIUM,
                false,
        },
        {
                "^ALSA (.*): BUG(.*)",
                "crash/kernel/bug",
                TM_MEDIUM,
                true,
        },
        {
                "irq [[:digit:]]+: nobody cared",
                "crash/kernel/warning",
                TM_MEDIUM,
                true,
        },
        {
                "WARNING: ",
                "org.clearlinux/kernel/warning",
                TM_MEDIUM,
                false,
        },
        {
                "general protection fault: ",
                "org.clearlinux/kernel/bug",
                TM_CRITICAL,
                false,
        },
        {
                "BUG: unable to handle kernel ",
                "org.clearlinux/kernel/bug",
                TM_CRITICAL,
                false,
        },
        {
                "BUG:",
                "org.clearlinux/kernel/bug",
                TM_CRITICAL,
                false,
        },
        {
                "kernel BUG at",
                "org.clearlinux/kernel/bug",
                TM_CRITICAL,
                false,
        },
        {
                "general protection fault:",
                "org.clearlinux/kernel/bug",
                TM_CRITICAL,
                false,
        },
        {
                "do_IRQ: stack overflow:",                  //revisit
                "org.clearlinux/kernel/stackoverflow",
                TM_CRITICAL,
                false,
        },
        {
                "RTNL: assertion failed",
                "org.clearlinux/kernel/warning",
                TM_MEDIUM,
                false,
        },
        {
                "Eeek! page_mapcount(page) went negative!",
                "org.clearlinux/kernel/bug",
                TM_CRITICAL,
                false,
        },
        {
                "near stack overflow (cur:",
                "org.clearlinux/kernel/bug",                  //revisit
                TM_CRITICAL,
                false,
        },
        {
                "double fault",
                "org.clearlinux/kernel/bug",
                TM_CRITICAL,
                false,
        },
        {
                "Badness",
                "org.clearlinux/kernel/bug",
                TM_CRITICAL,
                false,
        },
        {
                "list_del corruption.",
                "org.clearlinux/kernel/warning",
                TM_MEDIUM,
                false,
        },
        {
                "list_add corruption.",
                "org.clearlinux/kernel/warning",
                TM_MEDIUM,
                false,
        },
        {
                "ACPI Error:",
                "org.clearlinux/kernel/warning",
                TM_MEDIUM,
                false,
        },
};

static int oops_patterns_cnt = sizeof(oops_patterns_arr) / sizeof(struct oops_pattern);

static char *skip_log_level(char *line)
{
        char *start = line;

        if (*start == '<') {
                while (*start && *start != '>') {
                        start++;
                }
                if (*start) {
                        start++;
                }
        }
        return start;
}

static char *skip_timestamp(char *line)
{
        char *start = line;

        if (*start == '[') {
                while (*start && *start != ']') {
                        start++;
                }
                if (*start) {
                        start++;
                }
        }
        return start;
}

static char *skip_space(char *line)
{
        char *start = line;

        if (*start && isspace(*start)) {
                start++;
        }
        return start;
}

static char *skip_spaces(char *line)
{
        char *start = line;

        while (*start && isspace(*start)) {
                start++;
        }
        return start;
}

static bool starts_with(const char *line, const char *line_end, const char *substr)
{
        size_t substrlen = strlen(substr);

        if (substrlen <= (line_end - line) && memcmp(line, substr, substrlen) == 0) {
                return true;
        }
        return false;
}

static bool str_starts_with_casei(const char *line, const char *substr)
{
        size_t len = strlen(substr);

        if (strncasecmp(line, substr, len) == 0) {
                return true;
        }
        return false;
}

static bool pattern_matches(char *line, char *line_end, struct oops_pattern *pattern)
{
        if (pattern->is_regex) {
                return (regexec(&(pattern->regex), line, 0, NULL, 0) == 0);
        } else {
                return starts_with(line, line_end, pattern->begin_line);
        }
}

static void init_pattern_regex(void)
{
        for (int i = 0; i < oops_patterns_cnt; i++) {
                struct oops_pattern *pattern = &oops_patterns_arr[i];
                if (pattern->is_regex) {
                        regcomp(&(pattern->regex), pattern->begin_line,
                                REG_ICASE | REG_EXTENDED | REG_NOSUB);
                }
        }
}

static void free_pattern_regex(void)
{
        for (int i = 0; i < oops_patterns_cnt; i++) {
                struct oops_pattern *pattern = &oops_patterns_arr[i];
                if (pattern->is_regex) {
                        regfree(&(pattern->regex));
                }
        }
}

bool handle_entire_oops(char *buf, long size, struct oops_log_msg *msg)
{
        char *line_end;
        struct oops_pattern *pattern = NULL;
        int i;

        msg->length = 0;

        line_end = memchr(buf, '\n', (size_t)size);
        if (line_end == NULL) {
                return false;
        }

        *line_end = '\0';

        for (i = 0; i < oops_patterns_cnt; i++) {
                pattern = &oops_patterns_arr[i];
                if (pattern_matches(buf, line_end, pattern)) {
                        break;
                }
        }
        if (i == oops_patterns_cnt) {
                /* Nothin matched! */
                return false;
        }

        msg->pattern = pattern;
        msg->length = 0;
        msg->lines[msg->length] = strdup(buf);
        if (msg->lines[msg->length] == NULL) {
                exit(EXIT_FAILURE);
        }

        msg->length++;

        size = size - (line_end - buf + 1);
        buf = line_end + 1;

        while (size > 0) {
                line_end = memchr(buf, '\n', (size_t)size);
                if (line_end == NULL) {
                        line_end = buf + size;
                }

                *line_end = '\0';
                msg->lines[msg->length] = strndup(buf, (size_t)(line_end - buf + 1));
                if (msg->lines[msg->length] == NULL) {
                        exit(EXIT_FAILURE);
                }

                msg->length++;
                size -= (line_end - buf + 1);
                buf = line_end + 1;
        }
        return true;
}

void oops_msg_cleanup(struct oops_log_msg *msg)
{
        for (int i = 0; i < msg->length; i++) {
                free(msg->lines[i]);
        }
        msg->length = 0;
}

static struct oops_log_msg oops_msg;

static bool in_stack_dump = false;

static oops_handler_t oops_handler;

void oops_parser_init(oops_handler_t handler)
{
        oops_handler = handler;
        init_pattern_regex();
}

static void handle_msg_end(void)
{
        for (int i = 0; i < oops_msg.length; i++) {
                free(oops_msg.lines[i]);
        }
        oops_msg.length = 0;
}

void oops_parser_cleanup()
{
        handle_msg_end();
        free_pattern_regex();
}

void parse_single_line(char *line, size_t size)
{
        char *start;
        char *line_end;
        bool end_found = false;

        line_end = line + size;
        start = skip_log_level(line);
        start = skip_timestamp(start);
        start = skip_space(start);

        struct oops_pattern *pattern;
        if (oops_msg.length == 0) {
                for (int i = 0; i < oops_patterns_cnt; i++) {
                        pattern = &oops_patterns_arr[i];
                        if (!pattern_matches(start, line_end, pattern)) {
                                continue;
                        }
                        telem_log(LOG_DEBUG, "Oops start has been  detected\n");

                        oops_msg.pattern = pattern;
                        oops_msg.lines[oops_msg.length] = strndup(start, (size_t)(line_end - start));
                        if (oops_msg.lines[oops_msg.length] == NULL) {
                                //telem_perror("Failed to copy string");
                                exit(EXIT_FAILURE);
                                return;
                        } else {
                                oops_msg.length++;
                        }
                        in_stack_dump = false;
                        break;
                }
        } else {
                // If in the middle of oops
                if (oops_msg.length >= MAX_LINES) {
                        end_found = true;
                        // } else if (oops_msg.end_line && strstr(start, oops_msg.end_line)) {
                        //        end_found = false;
                } else if (strstr(start, "[ end trace")) {
                        end_found = true;
                } else if (!in_stack_dump) {
                        // This line indicates the beginning of a stack trace;
                        // the next line is most likely the topmost frame.
                        if (starts_with(start, line_end, "Call Trace:")) {
                                in_stack_dump = true;
                        }
                } else if (in_stack_dump) {
                        // In Linux 4.10+, the oops format changed, so we look
                        // for a single leading space character to indicate a
                        // stack frame line.
                        if (!(starts_with(start, line_end, " ")) ||
                            (strlen(start) < 8) ||
                            (strstr(start, "Code:") != NULL) ||
                            (strstr(start, "Instruction Dump::") != NULL)) {
                                in_stack_dump = false;
                                end_found = true;
                        }
                }

                /* if a new oops starts, this one has ended */
                if (strstr(start, "WARNING:") != NULL || strstr(start, "Unable to handle") != NULL) {
                        if (!oops_msg.pattern->is_regex || !str_starts_with_casei(oops_msg.pattern->begin_line, "^ALSA")) {
                                end_found = true;
                        }
                }

                if (end_found) {
                        oops_handler(&oops_msg);
                        handle_msg_end();
                } else {
                        oops_msg.lines[oops_msg.length] = strndup(start, (size_t)(line_end - start));
                        if (oops_msg.lines[oops_msg.length] == NULL) {
                                telem_perror("Failed to copy string");
                                return;
                        } else {
                                oops_msg.length++;
                        }
                }
        }
}

struct stack_frame {
        char *module;
        char *function;
        uint64_t addr;
        int64_t offset;
        struct stack_frame *next;
};

static void stack_frame_append(struct stack_frame **head, struct stack_frame **tail, char *start)
{
        /*
         * Format is:
         *
         *  [<ffffffffa1002128>] do_one_initcall+0xb8/0x1e0
         *  [<ffffffffa118784a>] ? __vunmap+0x9a/0x100
         *
         * OR
         *
         *  <IRQ>  [<ffffffff816606a8>] dump_stack+0x19/0x1b
         *
         * OR (in 4.10+)
         *
         *  do_one_initcall+0xb8/0x1e0
         *  ? __vunmap+0x9a/0x100
         *
         */

        struct stack_frame *frame = NULL;
        char *offset_ptr = NULL, *end = NULL;

        if (!start) {
                return;
        }

        // Skip leading spaces
        start = skip_spaces(start);

        //case: <IRQ>  [<ffffffff816606a8>] dump_stack+0x19/0x1b
        if (!strncmp(start, "<IRQ>", 5) || !strncmp(start, "<NMI>", 5) ||
            !strncmp(start, "<EOI>", 5) || !strncmp(start, "<<EOE>>", 7)) {
                while (*start && !isspace(*start)) {
                        start++;
                }
                if (start && *start == '\0') {
                        return;
                }

                // Skip intervening space
                start = skip_spaces(start);
        }

        frame = malloc(sizeof(struct stack_frame));
        if (!frame) {
                exit(EXIT_FAILURE);
        }

        frame->next = *head;
        *head = frame;

        if (*tail == NULL) {
                *tail = frame;
        }

        // Skip memory address parsing if the address is absent (as in Linux 4.10+).
        if (!strncmp(start, "[", 1)) {
                start++;

                if (*start == '<') {
                        start++;
                        frame->addr = (uint64_t)strtoll(start, (char **)&start, 16);
                } else {
                        frame->addr = 0;
                }

                // Skip up to the space after the closing bracket
                while (*start && !isspace(*start)) {
                        start++;
                }
                if (start && *start == '\0') {
                        return;
                }

                // Skip intervening space
                start = skip_spaces(start);
        } else {
                frame->addr = 0;
        }

        offset_ptr = strchr(start, '+');
        end = offset_ptr ? offset_ptr : (start + strlen(start));
        frame->function = strndup(start, (size_t)(end - start));
        if (!frame->function) {
                exit(EXIT_FAILURE);
        }

        start = end;

        /* Parse the offset */
        if (offset_ptr) {
                frame->offset = strtoll(offset_ptr + 1, (char **)&start, 16);
        } else {
                frame->offset = -1;
        }

        frame->module = "kernel";
}

static void stack_frame_free(struct stack_frame **head)
{
        struct stack_frame *frame;

        while (*head != NULL) {
                frame = (*head)->next;
                free(*head);
                *head = frame;
        }
}

/*
 * Function parses lines of the format :
 * CPU: 2 PID: 6429 Comm: insmod Tainted: P           OE  3.19.0-18-generic #18-Ubuntu$
 * CPU: 2 PID: 0 Comm: swapper/2 Not tainted  3.10.4-100.fc18.x86_64 #1
 * CPU: 3 PID: 0 Comm: swapper/3 Not tainted 4.0.5-300.fc22.x86_64 #1
 */
static void parse_kernel_cpu_line(char *line, char **kernel_version, char **tainted)
{
        char *end = NULL;

        end = strstr(line, " Not tainted ");
        if (end) {
                *tainted = strdup("Not tainted");
                line = end + strlen(" Not tainted");
        } else {
                end = strstr(line, "Tainted: ");
                if (end) {
                        *tainted = strndup(end + strlen("Tainted: "), NUM_TAINTED_FLAGS);
                        if (*tainted == NULL) {
                                exit(EXIT_FAILURE);
                        }

                        line = end + strlen("Tainted: ") + 1;
                }
        }

        end = strchr(line, '#');
        if (!end || !isspace(*(end - 1))) {
                return;
        }
        end -= 2;

        while (end >= line && !isspace(*end)) {
                end--;
        }

        if (end >= line) {
                *kernel_version = strdup(end + 1);
        }
}

static const char *registers[] = { "RIP", "RSP", "CR0", "CR2", "CR3", "CR4",
                                   "DR0", "DR1", "DR2", "DR3", "DR6", "DR7", "EFLAGS",
                                   "RAX", "RBX", "RCX", "RDX", "RSI", "RDI", "RBP",
                                   "R08", "R09", "R10", "R11",
                                   "R12", "R13", "R14", "R15",
                                   "FS", "GS", "knlGS", "CS", "ES", NULL, };

struct reg_s {
        char *reg_name;
        uint64_t reg_value;
        struct reg_s *next;
};

static struct reg_s *reg_head = NULL;

/* Parse lines conatining any register values
 * We handle lines in the various forms present, e.g.:

 * RIP: 0010:[<ffffffffc00b7012>]  [<ffffffffc00b7012>] my_oops_init+0x12/0x1000 [oops]
 *
 *  RSP: 0018:ffff8800543e3d18  EFLAGS: 00010292
 *
 * RAX: 0000000000000002 RBX: ffffffff81c1a080 RCX: 0000000000000002
 *
 * FS:  00007f5e7cab1700(0000) GS:ffff88014b240000(0000) knlGS:0000000000000000
 *
 * IP: [<ffffffff98231bc6>] sysrq_handle_crash+0x16/0x20
 *
 * EIP is at iret_exc+0x7d0/0xa59
 *
 * RIP: 0010:[<fff...1d8>]  [<fff...1d8>] sysrq_handle_crash+0x16/0x20
 *
 * RSP: 0018:ffff8800775f3e68  EFLAGS: 00010092
 *
 *  RSP <ffff9900628f3e40>
 *
 */

static void parse_registers(char *line)
{
        uint64_t value;
        struct reg_s *reg_entry = NULL;

        if (!line) {
                return;
        }

        while (*line) {
                while (isspace(*line)) {
                        line++;
                }
                if (*line == '\0') {
                        break;
                }

                int i;
                for (i = 0; registers[i] != NULL; i++) {
                        if (str_starts_with_casei(line, registers[i])) {
                                break;
                        }
                }

                if (registers[i] == NULL) {
                        //Register not found
                        break;
                }

                line = line + strlen(registers[i]);
                if (*line == ':') {
                        line++;
                }

                if (isspace(*line)) {
                        line++;
                }
                if (*line == '\0') {
                        break;
                }

                /*
                 * strtoll extracts the entire number if valid, else stores the first invalid
                 * character at line
                 * Try to extract the value first.
                 * case : RSP: 0018:ffff8800543e3d18  EFLAGS: 00010292
                 */
                value = (uint64_t)strtoll(line, &line, 16);

                //case : RSP: 0018:ffff8800775f3e68
                if (*line == ':') {
                        line++;
                        value = (uint64_t)strtoll(line, &line, 16);
                }

                // case: FS:  00007f5e7cab1700(0000) GS:ffff88014b240000(0000)
                if (*line == '(') {
                        // strchrnul returns first occurrence of ) or end of string
                        line = strchrnul(line, ')');
                        if (*line != ')') {
                                break;
                        }
                }
                //case: RSP <ffff9900628f3e40>
                else if (*line == '<') {
                        line++;
                        value = (uint64_t)strtoll(line, &line, 16);
                        if (*line != '>') {
                                continue;
                        }
                        line++;

                }
                // case: IP: [<ffffffff98231bc6>] sysrq_handle_crash+0x16/0x20
                else if (*line == '[' && *(line + 1) == '<') {
                        value = (uint64_t)strtoll(line, &line, 16);
                        if (*line != '>' && *(line + 1) != ']') {
                                continue;
                        }
                        line = line + 2;

                }
                //case: EIP is at iret_exc+0x7d0/0xa59
                else if (starts_with(line, line + strlen(line), "is at")) {
                        while (*line != '\0' && *line != '+') {
                                line++;
                        }
                        if (*line == '+') {
                                value = (uint64_t)strtoll(line, &line, 16);
                                line += strlen(line);
                        }
                } else if (*line != '\0' && !isspace(*line)) {
                        continue;
                }

                reg_entry = malloc(sizeof(struct reg_s));
                if (!reg_entry) {
                        exit(EXIT_FAILURE);
                }
                reg_entry->reg_name = (char *)registers[i];
                reg_entry->reg_value = value;

                if (reg_head == NULL) {
                        reg_head = reg_entry;
                        reg_entry->next = NULL;
                } else {
                        reg_entry->next = reg_head;
                        reg_head = reg_entry;
                }
        }

}

static void append_registers_to_bt(nc_string **backtrace)
{
        struct reg_s *reg_entry = reg_head;

        while (reg_entry != NULL) {
                // Global override for privacy filters
                if (access(TM_PRIVACY_FILTERS_OVERRIDE, F_OK) == 0) {
                        nc_string_append_printf(*backtrace, "Register %s: %" PRIx64 "\n", reg_entry->reg_name,
                                                reg_entry->reg_value);
                } else {
                        nc_string_append_printf(*backtrace, "Register %s: %s\n", reg_entry->reg_name,
                                                reg_entry->reg_value ? "Non-zero" : "Zero");
                }
                reg_entry = reg_entry->next;
        }

        while (reg_head != NULL) {
                reg_entry = reg_head->next;
                free(reg_head);
                reg_head = reg_entry;
        }
}

static nc_string *parse_backtrace(struct oops_log_msg *msg)
{
        struct stack_frame *head = NULL, *tail = NULL, *elem = NULL;
        //int in_stack_dump = 0;
        char *line = NULL;
        nc_string *backtrace = NULL;
        int frame_counter = 1;
        char *modules = NULL, *kernel_version = NULL, *tainted = NULL;
        // Since lines are processed from last to first, the stack trace lines
        // will appear first.
        bool in_trace = true;

        for (int i = msg->length - 1; i > 0; i--) {
                /* Check if this line is part of a stack trace */
                line = msg->lines[i];

                if (in_trace && starts_with(line, line + strlen(line), " ")) {
                        stack_frame_append(&head, &tail, line);
                        continue;
                }

                // We've reached the end of the stack trace
                if (starts_with(line, line + strlen(line), "Call Trace:")) {
                        in_trace = false;
                }

                // Stack trace lines must be consecutive, so if this point is
                // reached, we're outside the trace boundary.
                in_trace = false;

                if (str_starts_with_casei(line, "Modules linked in: ")) {
                        modules = line + strlen("Modules linked in: ");
                        continue;
                }

                if (str_starts_with_casei(line, "CPU: ") ||
                    str_starts_with_casei(line, "PID: ")) {
                        parse_kernel_cpu_line(line, &kernel_version, &tainted);
                        continue;
                }
                parse_registers(line);
        }

        backtrace = nc_string_dup("");
        if (kernel_version) {
                nc_string_append_printf(backtrace, "Kernel Version : %s\n", kernel_version);
                free(kernel_version);
        }

        if (tainted) {
                nc_string_append_printf(backtrace, "Tainted : %s\n", tainted);
                free(tainted);
        }

        if (modules) {
                nc_string_append_printf(backtrace, "Modules : %s\n", modules);
        }

        if (head) {
                nc_string_append_printf(backtrace, "Backtrace :\n");
        }

        if (reg_head) {
                append_registers_to_bt(&backtrace);
        }

        for (elem = head; elem != NULL; elem = elem->next, frame_counter++) {
                nc_string_append_printf(backtrace, "#%d %s - [%s]\n", frame_counter,
                                        elem->function ? elem->function : "???",
                                        elem->module);
        }

        stack_frame_free(&head);
        return backtrace;
}

nc_string *parse_payload(struct oops_log_msg *msg)
{
        nc_string *payload, *backtrace;

        payload = nc_string_dup("Crash Report:\n");
        nc_string_append_printf(payload, "Reason: %s\n", msg->lines[0]);
        backtrace = parse_backtrace(msg);
        nc_string_cat(payload, backtrace->str);
        nc_string_free(backtrace);
        return payload;

}

/*TODO implement a more informative reason line */
/*void parse_bugline(struct oops_log_msg msg) {

        return;
   }
 */

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
