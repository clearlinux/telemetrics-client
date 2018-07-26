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

#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/utsname.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <limits.h>
#include <inttypes.h>
#include <ctype.h>

#include "util.h"
#include "common.h"
#include "configuration.h"
#include "telemetry.h"

/**
 * Return a file descriptor to either site's version file
 * or the distributions version file, in that order of
 * preference.
 *
 * @return A file descriptor.
 *
 */
static int version_file(void)
{
        int fd;

        fd = open(TM_SITE_VERSION_FILE, O_RDONLY);
        if (fd < 0) {
                fd = open(TM_DIST_VERSION_FILE, O_RDONLY);
        }

        return fd;
}

/**
 * Helper function for the set_*_header functions that actually sets
 * the header and increments the header size. These headers become attributes
 * on an HTTP_POST transaction.
 *
 * @param dest The header string, which will be "prefix: dest\n"
 * @param prefix Identifies the header.
 * @param value The value of this particular header.
 * @param header_size Is incremented if header is successfully initialized.
 *
 * @return 0 if successful, or a negative errno-style value if not.
 *
 */
static int set_header(char **dest, const char *prefix, char *value, size_t *header_size)
{
        int rc;

        rc = asprintf(dest, "%s: %s\n", prefix, value);

        if (rc < 0) {
#ifdef DEBUG
                fprintf(stderr, "CRIT: Out of memory\n");
#endif
                return -ENOMEM;
        } else {
                *header_size += (size_t)rc;
                return 0;
        }

}

/**
 * Sets the severity header.  Severity is clamped to a ranage of 1-4.
 *
 * @param t_ref Reference to a Telemetry record.
 * @param severity Severity of issue this telemetry record pertains to.
 *     Severity is clamped to a range of 1-4.
 *
 * @return 0 if successful, or a negative errno-style value if not.
 *
 */
static int set_severity_header(struct telem_ref *t_ref, uint32_t severity)
{
        char *buf = NULL;
        int status = 0;
        int rc = 0;

        /* clamp severity to 1-4 */

        if (severity > 4) {
                severity = 4;
        }

        if (severity < 1) {
                severity = 1;
        }

        rc = asprintf(&buf, "%" PRIu32, severity);

        if (rc < 0) {
                return -ENOMEM;
        } else {
                status = set_header(&(t_ref->record->headers[TM_SEVERITY]),
                                    TM_SEVERITY_STR, buf, &(t_ref->record->header_size));
                free(buf);
        }

        return status;
}

/**
 * Sets the classification header.
 *
 * @param t_ref Reference to a Telemetry record.
 * @param classification Classification header is confirmed to
 *     to at least contain two / characters, and should be of the format
 *     <string>/<string>/<string>.
 *
 * @return 0 if successful, or a negative errno-style value if not.
 *
 */
static int set_classification_header(struct telem_ref *t_ref, char *classification)
{

        if (validate_classification(classification) == 1) {
                return -EINVAL;
        }

        return set_header(&(t_ref->record->headers[TM_CLASSIFICATION]),
                          TM_CLASSIFICATION_STR, classification,
                          &(t_ref->record->header_size));
}

/**
 * Sets the record_format header.The record_format is a constant that
 * defined in common.h, and identifies the format/structure of the
 * record. The record_format is mainly for use by the back-end record
 * parsing.
 *
 * @param t_ref Reference to a Telemetry record.
 *
 * @return 0 if successful, or a negative errno-style value if not.
 *
 */
static int set_record_format_header(struct telem_ref *t_ref)
{
        char *buf = NULL;
        int rc;
        int status = 0;

        rc = asprintf(&buf, "%" PRIu32, RECORD_FORMAT_VERSION);

        if (rc < 0) {
                return -ENOMEM;
        } else {
                status = set_header(&(t_ref->record->headers[TM_RECORD_VERSION]),
                                    TM_RECORD_VERSION_STR, buf,
                                    &(t_ref->record->header_size));

                free(buf);
        }

        return status;
}

/**
 * Sets the architecture header. Specifically the 'machine' field
 * of struct utsname, or 'unknown' if that field cannot be read.
 *
 * @param t_ref Telemetry Record reference obtained from tm_create_record.
 *
 * @return 0 if successful, or a negative errno-style value if not.
 *
 */
static int set_arch_header(struct telem_ref *t_ref)
{
        struct utsname uts_buf;
        char *buf = NULL;
        int rc = 0;
        int status = 0;

        rc = uname(&uts_buf);

        if (rc < 0) {
                rc = asprintf(&buf, "unknown");
        } else {
                rc = asprintf(&buf, "%s", uts_buf.machine);
        }

        if (rc < 0) {
                return -ENOMEM;
        } else {
                status = set_header(&(t_ref->record->headers[TM_ARCH]), TM_ARCH_STR,
                                    buf, &(t_ref->record->header_size));

                free(buf);
        }

        return status;
}

/**
 * Sets the system name header. Specifically, what comes after 'ID=' in the
 * os-release file that lives in either /etc or the stateless dist-provided
 * location.
 *
 * @param t_ref Telemetry Record reference obtained from tm_create_record.
 *
 * @return 0 if successful, or a negative errno-style value if not.
 *
 */
static int set_system_name_header(struct telem_ref *t_ref)
{
        FILE *fs = NULL;
        int fd;
        char buf[SMALL_LINE_BUF] = { 0 };
        char name[SMALL_LINE_BUF] = { 0 };

        fd = version_file();
        if (fd == -1) {
#ifdef DEBUG
                fprintf(stderr, "WARNING: Cannot find os-release file\n");
#endif
                sprintf(name, "unknown");
        } else {
                fs = fdopen(fd, "r");
                while (fgets(buf, SMALL_LINE_BUF, fs)) {
                        if (sscanf(buf, "ID=%s", name) < 1) {
                                continue;
                        } else {
                                break;
                        }
                }

                if (strlen(name) == 0) {
#ifdef DEBUG
                        fprintf(stderr, "WARNING: Cannot find os-release field: ID\n");
#endif
                        sprintf(name, "unknown");
                }

                fclose(fs);
        }

        return set_header(&(t_ref->record->headers[TM_SYSTEM_NAME]),
                          TM_SYSTEM_NAME_STR, name,
                          &(t_ref->record->header_size));

}

/**
 * Sets the header reporting the system build number for the OS.  This
 * comes from os-release on Clear Linux. On non-Clear systems this field
 * is probably not defined in the os-release file, in which case we report 0.
 *
 * @param t_ref Telemetry Record reference obtained from tm_create_record.
 *
 * @return 0 if successful, or a negative errno-style value if not.
 *
 */
static int set_system_build_header(struct telem_ref *t_ref)
{
        FILE *fs = NULL;
        int fd;
        char buf[SMALL_LINE_BUF] = { 0 };
        char version[40] = { 0 };

        fd = version_file();
        if (fd == -1) {
#ifdef DEBUG
                fprintf(stderr, "WARNING: Cannot find build version file\n");
#endif
                sprintf(version, "0");
        } else {
                fs = fdopen(fd, "r");
                while (fgets(buf, SMALL_LINE_BUF, fs)) {
                        if (sscanf(buf, "VERSION_ID=%s", version) < 1) {
                                continue;
                        } else {
                                break;
                        }
                }

                if (strlen(version) == 0) {
#ifdef DEBUG
                        fprintf(stderr, "WARNING: Cannot find build version number\n");
#endif
                        sprintf(version, "0");
                }

                fclose(fs);
        }

        return set_header(&(t_ref->record->headers[TM_SYSTEM_BUILD]),
                          TM_SYSTEM_BUILD_STR, version, &(t_ref->record->header_size));

}

/**
 * Sets the header reporting the machine_id. Note, the machine_id is really
 * managed by the telemetrics daemon (telemprobd), and populated by the daemon
 * as it delivers the records. Consequently, the header is simply set to a
 * dummy value of 0xFFFFFFF.
 *
 * @param t_ref Telemetry Record reference obtained from tm_create_record.
 *
 * @return 0 if successful, or a negative errno-style value if not.
 *
 */
static int set_machine_id_header(struct telem_ref *t_ref)
{
        uint64_t new_id = 0;
        int rc = 0;
        char *buf = NULL;
        int status = 0;

        new_id = 0xFFFFFFFF;

        rc = asprintf(&buf, "%zX", new_id);

        if (rc < 0) {
                return -ENOMEM;
        } else {
                status = set_header(&(t_ref->record->headers[TM_MACHINE_ID]),
                                    TM_MACHINE_ID_STR, buf, &(t_ref->record->header_size));

                free(buf);
        }

        return status;
}

/**
 * Sets the header reporting the timestamp for the creation of the telemetry
 * record.
 *
 * @param t_ref Telemetry Record reference obtained from tm_create_record.
 *
 * @return 0 if successful, or a negative errno-style value if not.
 *
 */
static int set_timestamp_header(struct telem_ref *t_ref)
{

        time_t *t = NULL;
        char *buf;
        int rc = 0;
        int status = 0;

        rc = asprintf(&buf, "%zd", time(t));

        if (rc < 0) {
                return -ENOMEM;
        } else {
                status = set_header(&(t_ref->record->headers[TM_TIMESTAMP]),
                                    TM_TIMESTAMP_STR, buf, &(t_ref->record->header_size));
                free(buf);
        }

        return status;
}

/**
 * Sets cpu model for telemetry record. The information from cpu is extracted
 * from /proc/cpuinfo, specifically "model name" attribute.
 *
 * @param t_ref Telemetry Record reference obtained from tm_create_record.
 *
 * @return 0 if successful, or a negative errno-style value if not.
 *
 */
static int set_cpu_model_header(struct telem_ref *t_ref)
{
        FILE *fs = NULL;
        char buf[SMALL_LINE_BUF] = { 0 };
        char *model_name = NULL;
        const char *attr_name = "model name";
        int status = 0;
        size_t attr_len = strlen(attr_name);
        size_t model_str_len = 0;

        fs = fopen("/proc/cpuinfo", "r");
        if (fs != NULL) {
                while (fgets(buf, SMALL_LINE_BUF, fs)) {
                        if (strncmp(attr_name, buf, attr_len) == 0) {
                                model_name = strchr(buf, ':');
                                break;
                        }
                }
                fclose(fs);
                if (model_name != NULL) {
                        model_str_len = strlen(model_name);
                        if (model_str_len > 2) {
                                model_name = (char *)model_name + 2 * sizeof(char);
                                model_name[model_str_len - 3] = '\0';
                        } else {
                                model_name = "blank";
                        }
                } else {
                        model_name = "blank";
                        fprintf(stderr, "NOTICE: Unable to find attribute:%s\n", attr_name);
                }

                status = set_header(
                        &(t_ref->record->headers[TM_CPU_MODEL]),
                        TM_CPU_MODEL_STR, model_name,
                        &(t_ref->record->header_size));

        } else {
#ifdef DEBUG
                fprintf(stderr, "NOTICE: Unable to open /proc/cpuinfo\n");
#endif
                status = -1;
        }

        return status;
}

/**
 * A healper function for set_host_type_header that reads the first line out of
 * a file and chomps the newline if necessary. If the file does not exist the
 * function returns "no_<key>_file". If the file exists but contains only
 * whitespace we return "blank". Otherwise the values read are returned in the
 * argument 'buf'. It is the caller's responsibility to free this memory later.
 *
 * @param source The file to be inspected.
 * @param key Shorthand notation for dmi value sought after. e.g.,
 * 'sv', 'pv', 'pvr'.
 * @param buf Buffer to store the read value.
 *
 * @return 0 if successful, or a negative errno-style value if not.
 *
 */
static int get_dmi_value(const char *source, const char *key, char **buf)
{
        char *line = NULL;
        FILE *dmifile;
        size_t size = 0;
        size_t new_size;
        char *old_value;
        int ret = 0;

        *buf = (char *)malloc(sizeof(char) * SMALL_LINE_BUF);

        if (*buf == NULL) {
#ifdef DEBUG
                fprintf(stderr, "CRIT: Out of memory\n");
#endif
                return -ENOMEM;
        }

        bzero(*buf, SMALL_LINE_BUF);
        dmifile = fopen(source, "r");

        if (dmifile != NULL) {
                line = fgets(*buf, SMALL_LINE_BUF - 1, dmifile);
                fclose(dmifile);

                /* zap the newline... */
                if (line != NULL) {
                        size = strlen(*buf);
                        (*buf)[size - 1] = '\0';
                }

                new_size = strlen(*buf);

                /* The file existed but was empty... */
                if (new_size == 0) {
                        old_value = *buf;
                        if (asprintf(buf, "blank") < 0) {
#ifdef DEBUG
                                fprintf(stderr, "CRIT: Out of memory\n");
#endif
                                ret = -ENOMEM;
                        }
                        free(old_value);
                } else {
                        /* ...or it contained all spaces */
                        int j;
                        for (j = 0; j < new_size; ++j) {
                                if ((*buf)[j] == ' ') {
                                        continue;
                                } else {
                                        break;
                                }
                        }
                        if (j == new_size) {
                                old_value = *buf;
                                if (asprintf(buf, "blank") < 0) {
#ifdef DEBUG
                                        fprintf(stderr, "CRIT: Out of memory\n");
#endif
                                        ret = -ENOMEM;
                                }
                                free(old_value);
                        }
                }
        } else {
#ifdef DEBUG
                fprintf(stderr, "NOTICE: Dmi file %s does not exist\n", source);
#endif

                old_value = *buf;
                if (asprintf(buf, "no_%s_file", key) < 0) {
#ifdef DEBUG
                        fprintf(stderr, "CRIT: Out of memory\n");
#endif
                        ret = -ENOMEM;
                }
                free(old_value);
        }

        return ret;
}

/**
 * Sets the board name for telemetry record, this record is a combination of
 * board name and board vendor. This information is read from dmi filesystem
 * Board Name (board_name) and Board Vendor (board_vendor).
 *
 * @param t_ref Telemetry Record reference obtained from tm_create_record.
 *
 * @return 0 if successful, or a negative errno-style value if not.
 *
 */
static int set_board_name_header(struct telem_ref *t_ref)
{
        int status = 0;
        int rc;
        char *buf = NULL;
        char *bn = NULL;
        char *bv = NULL;

        rc  = get_dmi_value("/sys/class/dmi/id/board_name", "bn", &bn);
        if (rc < 0) {
                status = rc;
                goto cleanup;
        }

        rc  = get_dmi_value("/sys/class/dmi/id/board_vendor", "bv", &bv);
        if (rc < 0) {
                status = rc;
                goto cleanup;
        }

        rc = asprintf(&buf, "%s|%s", bn, bv);

        if (rc < 0) {
                status = -ENOMEM;
                goto cleanup;
        } else {
                status = set_header(
                        &(t_ref->record->headers[TM_BOARD_NAME]),
                        TM_BOARD_NAME_STR, buf,
                        &(t_ref->record->header_size));
                free(buf);
        }

cleanup:
        if (bn != NULL) {
                free(bn);
        }

        if (bv != NULL) {
                free(bv);
        }

        return status;
}

/**
 * Sets BIOS version header for telemetry record, this information is
 * read from dmi filesystem BIOS Version (bios_version).
 *
 * @param t_ref Telemetry Record reference obtained from tm_create_record.
 *
 * @return 0 if successful, or a negative errno-style value if not.
 *
 */
static int set_bios_version_header(struct telem_ref *t_ref)
{
        int status = 0;
        int rc = 0;
        char *bios_version  = NULL;

        rc  = get_dmi_value("/sys/class/dmi/id/bios_version", "bv", &bios_version);
        if (rc < 0) {
                status = rc;
        } else {
                status = set_header(
                        &(t_ref->record->headers[TM_BIOS_VERSION]),
                        TM_BIOS_VERSION_STR, bios_version,
                        &(t_ref->record->header_size));
                free(bios_version);
        }

        return status;
}

/**
 * Sets the event_id header, this id is an identifier that multiple
 * records can share. This means that one event can lead to multiple
 * records.
 *
 * @param t_ref Telemetry Record reference obtained from tm_create_record.
 *
 * @return 0 if successful, -1 on failure
 *
 */
static int set_event_id_header(struct telem_ref *t_ref)
{
        int rc = 0;
        char *buff = NULL;

        rc = get_random_id(&buff);

        if (rc == 0) {
                rc = set_header(
                        &(t_ref->record->headers[TM_EVENT_ID]),
                        TM_EVENT_ID_STR, buff,
                        &(t_ref->record->header_size));
        }
        free(buff);

        return rc;
}

/**
 * Sets the hosttype header, which is a tuple of three values looked for
 * in the dmi filesystem.  System Vendor (sys_vendor), Product Name
 * (product_name), and Product Version (product_version). The values are
 * separated by pipes.  e.g., <sv|pn|pvr>
 *
 * @param t_ref Telemetry Record reference obtained from tm_create_record.
 *
 * @return 0 if successful, or a negative errno-style value if not.
 *
 */
static int set_host_type_header(struct telem_ref *t_ref)
{
        char *buf = NULL;
        char *sv  = NULL;
        char *pn  = NULL;
        char *pvr = NULL;
        int rc = 0;
        int status = 0;

        rc  = get_dmi_value("/sys/class/dmi/id/sys_vendor", "sv", &sv);
        if (rc < 0) {
                status = rc;
                goto cleanup;
        }

        rc  = get_dmi_value("/sys/class/dmi/id/product_name", "pn", &pn);
        if (rc < 0) {
                status = rc;
                goto cleanup;
        }

        rc = get_dmi_value("/sys/class/dmi/id/product_version", "pvr", &pvr);
        if (rc < 0) {
                status = rc;
                goto cleanup;
        }

        rc = asprintf(&buf, "%s|%s|%s", sv, pn, pvr);

        if (rc < 0) {
                status = -ENOMEM;
                goto cleanup;
        } else {
                status = set_header(&(t_ref->record->headers[TM_HOST_TYPE]),
                                    TM_HOST_TYPE_STR, buf, &(t_ref->record->header_size));
                free(buf);
        }

cleanup:
        if (sv != NULL) {
                free(sv);
        }

        if (pn != NULL) {
                free(pn);
        }

        if (pvr != NULL) {
                free(pvr);
        }

        return status;
}

/**
 * Sets the kernel version header, as reported by uname. If we cannnot
 * access the results of the uname system call, the function will gracefully
 * fail and report "unknown".
 *
 * @param t_ref Telemetry Record reference obtained from tm_create_record.
 *
 * @return 0 if successful, or a negative errno-style value if not.
 *
 */
static int set_kernel_version_header(struct telem_ref *t_ref)
{
        struct utsname uts_buf;
        char *buf = NULL;
        int rc = 0;
        int status = 0;

        rc = uname(&uts_buf);

        if (rc < 0) {
                rc = asprintf(&buf, "unknown");
        } else {
                rc = asprintf(&buf, "%s", uts_buf.release);
        }

        if (rc < 0) {
                return -ENOMEM;
        } else {
                status = set_header(
                        &(t_ref->record->headers[TM_KERNEL_VERSION]),
                        TM_KERNEL_VERSION_STR, buf,
                        &(t_ref->record->header_size));
                free(buf);
        }

        return status;
}

/**
 * Sets the payload format header, which is a probe-specific version number
 * for the format of the payload. This is a hint to the back-end that parses records.
 *
 * @param t_ref Telemetry Record reference obtained from tm_create_record.
 * @param payload_version Version number provided by probe.
 *
 * @return 0 if successful, or a negative errno-style value if not.
 *
 */
static int set_payload_format_header(struct telem_ref *t_ref, uint32_t payload_version)
{
        char *buf = NULL;
        int rc;
        int status = 0;

        rc = asprintf(&buf, "%" PRIu32, payload_version);

        if (rc < 0) {
                return -ENOMEM;
        } else {
                status = set_header(
                        &(t_ref->record->headers[TM_PAYLOAD_VERSION]),
                        TM_PAYLOAD_VERSION_STR, buf,
                        &(t_ref->record->header_size));
                free(buf);
        }

        return status;
}

void tm_set_config_file(char *c_file)
{
        set_config_file(c_file);
}

/**
 * Helper function for tm_create_record().  Allocate all of the headers
 * for a new telemetrics record. The parameters are passed through from
 * tm_create_record to this function.
 *
 * @param t_ref Telemetry Record reference obtained from tm_create_record.
 * @param severity Severity field value. Accepted values are in the range 1-4,
 *     with 1 being the lowest severity, and 4 being the highest severity.
 * @param classification Classification field value. It should have the form
 *     DOMAIN/PROBENAME/REST: DOMAIN is the reverse domain to use as a namespace
 *     for the probe (e.g. org.clearlinux); PROBENAME is the name of the probe;
 *     and REST is an arbitrary value that the probe should use to classify the
 *     record.
 * @param payload_version Payload format version. The only supported value right
 *     now is 1, which indicates that the payload is a freely-formatted
 *     (unstructured) string. Values greater than 1 are reserved for future use.
 *
 *
 * @return 0 if successful, or a negative errno-style value if not.
 *
 */
int allocate_header(struct telem_ref *t_ref, uint32_t severity,
                    char *classification, uint32_t payload_version)
{

        int k, i = 0;
        int ret = 0;

        /* The order we create the headers matters */

        if ((ret = set_record_format_header(t_ref)) < 0) {
                goto free_and_fail;
        }

        i++;

        if ((ret = set_classification_header(t_ref, classification)) < 0) {
                goto free_and_fail;
        }

        i++;

        if ((ret = set_severity_header(t_ref, severity)) < 0) {
                goto free_and_fail;
        }

        i++;

        if ((ret = set_machine_id_header(t_ref)) < 0) {
                goto free_and_fail;
        }

        i++;

        if ((ret = set_timestamp_header(t_ref)) < 0) {
                goto free_and_fail;
        }

        i++;

        if ((ret = set_arch_header(t_ref)) < 0) {
                goto free_and_fail;
        }

        i++;

        if ((ret = set_host_type_header(t_ref)) < 0) {
                goto free_and_fail;
        }

        i++;

        if ((ret = set_system_build_header(t_ref)) < 0) {
                goto free_and_fail;
        }

        i++;

        if ((ret = set_kernel_version_header(t_ref)) < 0) {
                goto free_and_fail;
        }

        i++;

        if ((ret = set_payload_format_header(t_ref, payload_version)) < 0) {
                goto free_and_fail;
        }

        i++;

        if ((ret = set_system_name_header(t_ref)) < 0) {
                goto free_and_fail;
        }

        i++;

        if ((ret = set_board_name_header(t_ref)) < 0) {
                goto free_and_fail;
        }

        i++;

        if ((ret = set_cpu_model_header(t_ref)) < 0) {
                goto free_and_fail;
        }

        i++;

        if ((ret = set_bios_version_header(t_ref)) < 0) {
                goto free_and_fail;
        }

        i++;

        if ((ret = set_event_id_header(t_ref)) < 0) {
                goto free_and_fail;
        }

        i++; /* Not necessary, but including for future expansion */

        return ret;

free_and_fail:
        for (k = 0; k < i; k++) {
                free(t_ref->record->headers[k]);
        }
        return ret;

}

int tm_create_record(struct telem_ref **t_ref, uint32_t severity,
                     char *classification, uint32_t payload_version)
{
        int ret = 0;
        int k = 0;
        struct stat unused;

        k = stat(TM_OPT_OUT_FILE, &unused);
        if (k == 0) {
                // Bail early if opt-out is enabled
                return -ECONNREFUSED;
        }

        *t_ref = (struct telem_ref *)malloc(sizeof(struct telem_ref));
        if (*t_ref == NULL) {
#ifdef DEBUG
                fprintf(stderr, "CRIT: Out of memory\n");
#endif
                return -ENOMEM;
        }

        (*t_ref)->record = (struct telem_record *)malloc(sizeof(struct telem_record));
        if ((*t_ref)->record == NULL) {
#ifdef DEBUG
                fprintf(stderr, "CRIT: Out of memory\n");
#endif
                free(*t_ref);
                return -ENOMEM;
        }

        // Need to initialize header size, since it is only incremented elsewhere
        (*t_ref)->record->header_size = 0;

        /* tm_set_payload() may not be called (e.g. in our test suite); in this
         * case, initialize the payload member to NULL so that we can properly
         * detect a non-NULL value in tm_free_record().
         */
        (*t_ref)->record->payload = NULL;

        /* Set up the headers */
        if ((ret = allocate_header(*t_ref, severity, classification, payload_version)) < 0) {
                free((*t_ref)->record);
                free(*t_ref);
        }

        return ret;
}

/**
 * Validate's a payload is printable ascii, including whitespace.
 *
 * @param payload The payload to be verified.
 * @param len Length of the payload.
 *
 * @return 0 if successful, or -EINVAL.
 *
 */

int payload_is_ascii(char *payload, size_t len)
{
        int i = 0;
        int ret = 0;

        for (i = 0; i < len; i++) {
                if (!isprint(payload[i]) &&
                    !isspace(payload[i])) {
                        ret = -EINVAL;
                        break;
                }
        }

        return ret;
}

// TODO: Consider simply setting a pointer to point to data provide in payload instead of copying it?
int tm_set_payload(struct telem_ref *t_ref, char *payload)
{
        size_t payload_len;
        int ret = 0;
        int k = 0;
        struct stat unused;

        k = stat(TM_OPT_OUT_FILE, &unused);
        if (k == 0) {
                // Bail early if opt-out is enabled
                return -ECONNREFUSED;
        }

        payload_len = strlen((char *)payload);

        if (payload_len > MAX_PAYLOAD_LENGTH) {
                return -EINVAL;
        }

        if (payload_is_ascii(payload, payload_len) != 0) {
                return -EINVAL;
        }

        t_ref->record->payload = (char *)malloc(sizeof(char) * payload_len + 1);

        if (!t_ref->record->payload) {
#ifdef DEBUG
                fprintf(stderr, "CRIT: Out of memory\n");
#endif
                return -ENOMEM;
        }

        memset(t_ref->record->payload, 0, sizeof(char) * payload_len + 1);

        strncpy((char *)(t_ref->record->payload), (char *)payload, payload_len);

        t_ref->record->payload_size = payload_len;

        return ret;
}

/**
 * Checks for id to have length = 32 characters
 * and hexadecimal characters only.
 *
 * @param id A pointer to string to be checked
 *
 * @return 0 on success, 1 on failure
 *
 */
static int validate_event_id(char *id)
{
        int rc = 0;
        const char alphab[] = EVENT_ID_ALPHAB;

        // make sure is not null
        if (!id) {
                return 1;
        }

        // 32 - 32 = 0
        if ((rc = (int)(strlen(id) - EVENT_ID_LEN)) != 0) {
                return 1;
        }

        // verify alphabet
        // strspn checks for characters in alphab
        if (strspn(id, alphab) != EVENT_ID_LEN) {
                return 1;
        }

        return rc;
}

int tm_set_event_id(struct telem_ref *t_ref, char *event_id)
{
        int rc = -1;

        if (!validate_event_id(event_id)) {
                if (t_ref && t_ref->record && t_ref->record->headers) {
                        // free default id before overriding
                        free(t_ref->record->headers[TM_EVENT_ID]);
                        // set new event_id
                        if (asprintf(&(t_ref->record->headers[TM_EVENT_ID]),
                                     "%s: %s\n", TM_EVENT_ID_STR, event_id) > 0) {
                                rc = 0;
                        }
                }
        }

        return rc;
}

/**
 * Write nbytes from buf to fd. Used to send records to telemprobd.
 *
 * @param fd Socket fd obtained from tm_get_socket.
 * @param buf Data to be written to the socket.
 * @param nbytes Number of bytes to write out of buf.
 *
 * @return 0 if successful, or a negative errno-style value if not.
 *
 */
static int tm_write_socket(int fd, char *buf, size_t nbytes)
{
        size_t nbytes_out = 0;
        int k = 0;
        int ret = 0;

        while (nbytes_out != nbytes) {
                ssize_t b;
                b = write(fd, buf + nbytes_out, nbytes - nbytes_out);

                if (b == -1 && errno != EAGAIN) {
                        ret = -errno;
#ifdef DEBUG
                        fprintf(stderr, "ERR: Write to daemon socket with"
                                " system error: %s\n", strerror(errno));
#endif
                        return ret;
                } else if (b == -1 &&
                           (errno == EAGAIN || errno == EWOULDBLOCK)) {
                        if (++k > 100) {
                                ret = -errno;
                                return ret;
                        }
                } else {
                        nbytes_out += (size_t)b;
                        k = 0;
                }
        }

        return ret;
}

/**
 * Obtain a file descriptor for a unix domain socket.
 * Connect to the socket in a non-blocking fashion.
 *
 * @return A valid file descriptor, or a negative errno-style value on error.
 *
 */
static int tm_get_socket(void)
{
        int sfd = -1;
        int ret = 0;
        struct sockaddr_un addr;
        long sflags = 0;
        int res = 0;
        struct timeval tv;
        fd_set fdset;
        socklen_t lon = 0;
        int valopt = 0;

        sfd = socket(AF_UNIX, SOCK_STREAM, 0);

        if (sfd == -1) {
                ret = -errno;
#ifdef DEBUG
                fprintf(stderr, "ERR: Attempt to allocate socket fd failed:"
                        " %s\n", strerror(errno));
#endif
                return ret;
        }

        /* Set timeout for the socket first */
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        if (setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, &tv,
                       sizeof(tv)) < 0) {
                ret = -errno;
#ifdef DEBUG
                fprintf(stderr, "ERR: Failed to set socket timeout:"
                        " %s\n", strerror(errno));
#endif
                goto out1;
        }

        /* Construct server address, and make the connection */
        memset(&addr, 0, sizeof(struct sockaddr_un));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, socket_path_config(),
                sizeof(addr.sun_path) - 1);

        res = connect(sfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un));

        if (res < 0) {
                // Socket still connecting. Try to select it with a 1 second.
                // failure to select() is a sign of problems - bail out.
                if (errno == EINPROGRESS) {
                        do {
                                tv.tv_sec = 1;
                                tv.tv_usec = 0;
                                FD_ZERO(&fdset);
                                FD_SET(sfd, &fdset);
                                res = select(sfd + 1, NULL, &fdset, NULL, &tv);

                                if (res < 0 && errno != EINTR) {
                                        // connection error or resume-after-interrupt
                                        ret = -errno;
                                        goto out1;
                                } else if (res > 0) {
                                        // Socket was selected for a write
                                        lon = sizeof(int);
                                        if (getsockopt(sfd, SOL_SOCKET, SO_ERROR,
                                                       (void *)(&valopt), &lon) < 0) {
                                                ret = -errno;
                                                goto out1;
                                        }
                                        if (valopt) {
                                                // errors in socket
                                                ret = -valopt;
                                                goto out1;
                                        }
                                        break;
                                } else {
                                        // We timed out.
                                        ret = -ETIMEDOUT;
                                        goto out1;
                                }
                        } while (1);
                } else {
                        ret = -errno;
                        goto out1;
                }
        }

        // Set non-blocking after the connect() succeeded
        if ((sflags = fcntl(sfd, F_GETFL, NULL)) < 0) {
                ret = -errno;
#ifdef DEBUG
                fprintf(stderr, "ERR: Failed to get socket flags\n");
#endif
                goto out1;
        }

        sflags |= O_NONBLOCK;

        if (fcntl(sfd, F_SETFL, sflags) < 0) {
                ret = -errno;
#ifdef DEBUG
                fprintf(stderr, "ERR: Failed to set socket flags\n");
#endif
                goto out1;
        }

        return sfd;

out1:
        close(sfd);
        return ret;
}

int tm_send_record(struct telem_ref *t_ref)
{
        int i;
        size_t record_size = 0;
        size_t total_size = 0;
        int sfd;
        char *data = NULL;
        size_t offset = 0;
        int ret = 0;
        int k = 0;
        struct stat unused;

        k = stat(TM_OPT_OUT_FILE, &unused);
        if (k == 0) {
                // Bail early if opt-out is enabled
                return -ECONNREFUSED;
        }

        sfd = tm_get_socket();

        if (sfd < 0) {
#ifdef DEBUG
                fprintf(stderr, "ERR: Failed to get socket fd: %s\n",
                        strerror(-sfd));
#endif
                return sfd;
        }

        total_size = t_ref->record->header_size + t_ref->record->payload_size;
#ifdef DEBUG
        fprintf(stderr, "DEBUG: Header size : %zu\n", t_ref->record->header_size);
        fprintf(stderr, "DEBUG: Payload size : %zu\n", t_ref->record->payload_size);

        fprintf(stderr, "DEBUG: Total size : %zu\n", total_size);
#endif

        /*
         * Allocating buffer for what we intend to send.  Buffer layout is:
         * <uint32_t total_size><uint32_t header_size><headers + Payload>
         * <null-byte>
         * The additional char at the end ensures null termination
         */
        record_size = (2 * sizeof(uint32_t)) + total_size + 1;

        data = malloc(record_size);
        if (!data) {
#ifdef DEBUG
                fprintf(stderr, "CRIT: Out of memory\n");
#endif
                close(sfd);
                return -ENOMEM;
        }

        memset(data, 0, record_size);

        memcpy(data, &total_size, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        memcpy(data + offset, &t_ref->record->header_size, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        size_t len = 0;
        for (i = 0; i < NUM_HEADERS; i++) {
                len = strlen(t_ref->record->headers[i]);
                memcpy(data + offset, t_ref->record->headers[i], len);
                offset += len;
        }

        memcpy(data + offset, t_ref->record->payload, t_ref->record->payload_size);

#ifdef DEBUG
        fprintf(stderr, "DEBUG: Data to be sent :\n\n%s\n", data + 2 * sizeof(uint32_t));
#endif
        if ((ret = tm_write_socket(sfd, data, record_size)) == 0) {
#ifdef DEBUG
                fprintf(stderr, "INFO: Successfully sent record over the socket\n");
#endif
        } else {
#ifdef DEBUG
                fprintf(stderr, "ERR: Error while writing data to socket\n");
#endif
        }

        close(sfd);
        free(data);

        return ret;
}

void tm_free_record(struct telem_ref *t_ref)
{

        int k;

        if (t_ref == NULL) {
                return;
        }

        if (t_ref->record == NULL) {
                return;
        }

        for (k = 0; k < NUM_HEADERS; k++) {
                free(t_ref->record->headers[k]);
        }

        if (t_ref->record->payload) {
                free(t_ref->record->payload);
        }

        free(t_ref->record);
        free(t_ref);
}

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
