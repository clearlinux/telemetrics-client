/*
 * This program is part of the Clear Linux Project
 *
 * Copyright 2019 Intel Corporation
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

#include <stdio.h>
#include <errno.h>
#ifdef ANDROID
#include <log/log.h>
#endif
#include "config.h"

#ifdef TM_LOG_SYSTEMD
#ifdef HAVE_SYSTEMD_SD_JOURNAL_H

/* Certain static analysis tools do not understand GCC's __INCLUDE_LEVEL__
 * macro; the conditional definition below is used to fix the build with
 * systemd's _sd-common.h, included by the public systemd headers, which rely on
 * the macro being defined and having an accurate value.
 */
#ifndef __INCLUDE_LEVEL__
# define __INCLUDE_LEVEL__ 2
#endif

#include <systemd/sd-journal.h>
#endif
#else
#include <string.h>
#include <syslog.h>
#endif

#ifdef DEBUG
#define telem_debug(...) do { \
                telem_log(LOG_DEBUG, "%s():[%d] ", __func__, __LINE__); \
                telem_log(LOG_DEBUG, __VA_ARGS__); \
} while (0);
#else
#define telem_debug(...) do {} while (0);
#endif /* DEBUG */

/*
   Acceptable values for priority:

   LOG_EMERG      system is unusable
   LOG_ALERT      action must be taken immediately
   LOG_CRIT       critical conditions
   LOG_ERR        error conditions
   LOG_WARNING    warning conditions
   LOG_NOTICE     normal, but significant, condition
   LOG_INFO       informational message
   LOG_DEBUG      debug-level message
 */

#ifdef TM_LOG_SYSTEMD
#ifdef HAVE_SYSTEMD_SD_JOURNAL_H
#define telem_log(priority, ...) do { \
                if (priority <= MAX_LOG_LEVEL) { \
                        sd_journal_print(priority, __VA_ARGS__); \
                } \
} while (0);
#endif
#endif
#ifdef TM_LOG_SYSLOG
#define telem_log(priority, ...) do { \
                if (priority <= MAX_LOG_LEVEL) { \
                        syslog(priority, __VA_ARGS__); \
                } \
} while (0);
#endif
#ifdef TM_LOG_STDERR
#define telem_log(priority, ...) do { \
                if (priority <= MAX_LOG_LEVEL) { \
                        fprintf(stderr, __VA_ARGS__); \
                } \
} while (0);
#endif

#ifdef TM_LOG_SYSTEMD
#define telem_perror(msg) sd_journal_perror(msg)
#endif
#ifdef TM_LOG_SYSLOG
#define telem_perror(msg) syslog(LOG_ERR, msg ": %s\n", strerror(errno))
#endif
#ifdef TM_LOG_STDERR
#define telem_perror(msg) fprintf(stderr, "ERROR: " msg ": %s\n", strerror(errno))
#endif

#ifdef ANDROID
#ifdef telem_log
#undef telem_log
#endif
#ifdef telem_perror
#undef telem_perror
#endif
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#ifdef LOG_PRI
#undef LOG_PRI
#endif
#define LOG_TAG "telemetrics-client"
#define LOG_PRI(priority, tag, ...) android_printLog(priority, tag, __VA_ARGS__)
#define telem_log(priority, ...) \
    switch (priority) \
    { \
        case LOG_EMERG: ALOGE(__VA_ARGS__); break; \
        case LOG_ALERT: ALOGE(__VA_ARGS__); break; \
        case LOG_CRIT: ALOGE(__VA_ARGS__); break; \
        case LOG_ERR: ALOGE(__VA_ARGS__); break; \
        case LOG_WARNING: ALOGW(__VA_ARGS__); break; \
        case LOG_DEBUG: ALOGD(__VA_ARGS__); break; \
        case LOG_NOTICE: ALOGD(__VA_ARGS__); break; \
        case LOG_INFO: ALOGI(__VA_ARGS__); break; \
    }
#define telem_perror(msg) telem_log(LOG_ERR, msg)
#endif

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
