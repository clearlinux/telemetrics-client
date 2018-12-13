/*
 * This program is part of the Clear Linux Project
 *
 * Copyright 2019 Intel Corporation
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <fcntl.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <getopt.h>

#include "log.h"
#include "telemetry.h"

#define PYTHON_TELEMETRY_DIR "/var/lib/telemetry/python"

/* We expect something like: 1.python-exception.3.N2Qy24 */
static bool validate_file_name(const char *filename, uint32_t *severity,
                               char** class, uint32_t *version)
{
        unsigned int ver, sev;
        char cls[PATH_MAX] = { 0 };
        char rnd[PATH_MAX] = { 0 };
        char *filename_base = basename(filename);
        char *str_class = NULL;

        if (!filename_base) {
                telem_perror("basename failed");
                return false;
        }

        if (sscanf(filename_base,"%u.%[^]0-9.].%u.%s", &ver, cls, &sev, rnd) == 4) {
                if (asprintf(&str_class, "org.clearlinux/python/%s", cls) < 29) {
                        telem_log(LOG_ERR, "%s: invalid telemetry classification\n",
                                  str_class);
                        return false;
                }
                *class = str_class;
                *version = ver;
                *severity = sev;
                return true;
        }

        return false;
}

static char *read_file_into_buffer(const char *filename)
{
        FILE *fp = NULL;
        char *contents = NULL;
        long sz;
        size_t size, bytes_read;

        fp = fopen(filename, "r");
        if (fp == NULL) {
                telem_log(LOG_ERR, "Failed to open Python telemetry file %s: %s\n",
                          filename, strerror(errno));
                return NULL;
        }

        if (fseek(fp, 0, SEEK_END) == -1) {
                telem_log(LOG_ERR, "Failed to seek file %s: %s\n", filename,
                          strerror(errno));
                goto error_end;
        }

        sz = ftell(fp);
        if (sz == -1L) {
                telem_perror("ftell failed");
                goto error_end;
        }

        if (sz == 0L) {
                telem_perror("File is empty\n");
                goto error_end;
        }

        if (fseek(fp, 0, SEEK_SET) < 0) {
                telem_log(LOG_ERR, "fseek failed for file %s:%s\n", filename,
                          strerror(errno));
                goto error_end;
        }

        size = (size_t)sz;
        contents = calloc(sizeof(char), size);
        if (!contents) {
                telem_log(LOG_ERR, "Call to calloc failed\n");
                goto error_end;
        }

        bytes_read = fread(contents, sizeof(char), size, fp);

        if (bytes_read < size) {
                telem_log(LOG_ERR, "Error while reading file %s:%s\n", filename,
                          strerror(errno));
                goto error_end;
        }

        fclose(fp);
        return contents;

error_end:
        fclose(fp);
        if (contents) {
                free(contents);
        }
        return NULL;
}

static void send_data(char *contents, uint32_t severity, char *class,
                      uint32_t version)
{
        struct telem_ref *handle = NULL;
        int ret;

        if ((ret = tm_create_record(&handle, severity, class, version)) < 0) {
                telem_log(LOG_ERR, "Failed to create record: %s\n",
                          strerror(-ret));
                return;
        }

        if ((ret = tm_set_payload(handle, contents)) < 0) {
                telem_log(LOG_ERR, "Failed to set payload: %s\n",
                          strerror(-ret));
                tm_free_record(handle);
                return;
        }

        if ((ret = tm_send_record(handle)) < 0) {
                telem_log(LOG_ERR, "Failed to send record: %s\n",
                          strerror(-ret));
        }

        tm_free_record(handle);
}

/* Send a valid file to the backend. */
static void deliver_payload(const char *filename)
{
        uint32_t severity;
        uint32_t version;
        char *class = NULL;
        char *contents = NULL;

        if (validate_file_name(filename, &severity, &class, &version)) {
                if ((contents = read_file_into_buffer(filename)) != NULL) {
                        send_data(contents, severity, class, version);
                        free(contents);
                }

                free(class);
        }

        /* Keep PYTHON_TELEMETRY_DIR empty. Delete the file. */
        if (unlink(filename) != 0) {
                telem_log(LOG_ERR, "Failed to unlink %s: %s\n", filename,
                          strerror(errno));
        }
}

static void drop_privs(void)
{
        uid_t euid;

        euid = geteuid();
        if (euid != 0) {
                telem_log(LOG_DEBUG, "Not root; skipping privilege drop\n");
                return;
        }

        struct passwd *pw;

        pw = getpwnam("telemetry");
        if (!pw) {
                telem_log(LOG_ERR, "telemetry user not found\n");
                exit(EXIT_FAILURE);
        }

        // The order is important here:
        // change supplemental groups, our gid, and then our uid
        if (initgroups(pw->pw_name, pw->pw_gid) != 0) {
                telem_perror("Failed to set supplemental group list");
                exit(EXIT_FAILURE);
        }
        if (setgid(pw->pw_gid) != 0) {
                telem_perror("Failed to set GID");
                exit(EXIT_FAILURE);
        }
        if (setuid(pw->pw_uid) != 0) {
                telem_perror("Failed to set UID");
                exit(EXIT_FAILURE);
        }

        assert(getuid() == pw->pw_uid);
        assert(geteuid() == pw->pw_uid);
        assert(getgid() == pw->pw_gid);
        assert(getegid() == pw->pw_gid);
}

static void handle_inotify_event(const struct inotify_event *event)
{
        if (!event) {
                telem_perror("Null event received");
                return;
        }

        if (event->len == 0) {
                telem_perror("inotify event received with no file.");
                return;
        }

        if (event->mask & IN_MOVED_TO) {
                if (event->mask & IN_ISDIR) {
                        return;
                }

                #ifdef DEBUG
                telem_log(LOG_DEBUG, "New file moved to the monitored location: %s\n", event->name);
                #endif
                deliver_payload(event->name);
        }
}

static void wait_for_payload(void)
{
        int inotify_fd;
        char buf[4096];
        char *ptr = NULL;
        ssize_t len;
        const struct inotify_event *event;

        inotify_fd = inotify_init();
        if (inotify_fd == -1) {
                telem_log(LOG_ERR, "Failed to create inotify instance: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
        }

        if (inotify_add_watch(inotify_fd, PYTHON_TELEMETRY_DIR, IN_MOVED_TO) == -1) {
                telem_perror("Error adding watch to the inotify instance");
        }

        while (true) {
                len = read(inotify_fd, buf, sizeof buf);
                if (len <= 0) {
                        telem_perror("error reading event");
                        continue;
                }

                if (len < sizeof(struct inotify_event)) {
                        telem_perror("Incomplete event received");
                        continue;
                }

                /* Loop over all events in the buffer */
                for (ptr = buf; ptr < buf + len;
                     ptr += sizeof(struct inotify_event) + event->len) {

                        event = (const struct inotify_event *)ptr;
                        /* handle the event */
                        handle_inotify_event(event);
                }
        }
}

static void print_usage(char *prog)
{
        printf("%s: Usage\n", prog);
        printf("  -f,  --config_file    Specify a configuration file other than default\n");
        printf("  -h,  --help           Display this help message\n");
        printf("  -V,  --version        Print the program version\n");
}

int main(int argc, char **argv)
{
        DIR *dir;
        struct dirent *entry;
        int c;
        int opt_index = 0;

        struct option opts[] = {
                { "config_file", 1, NULL, 'f' },
                { "help", 0, NULL, 'h' },
                { "version", 0, NULL, 'V' },
                { NULL, 0, NULL, 0 }
        };

        drop_privs();

        while ((c = getopt_long(argc, argv, "f:hV", opts, &opt_index)) != -1) {
                switch (c) {
                        case 'f':
                                if (tm_set_config_file(optarg) != 0) {
                                    telem_log(LOG_ERR, "Configuration file"
                                                  " path not valid\n");
                                    exit(EXIT_FAILURE);
                                }
                                break;
                        case 'h':
                                print_usage(argv[0]);
                                exit(EXIT_SUCCESS);
                        case 'V':
                                printf(PACKAGE_VERSION "\n");
                                exit(EXIT_SUCCESS);
                        case '?':
                                exit(EXIT_FAILURE);
                }
        }

        if (chdir(PYTHON_TELEMETRY_DIR)) {
                perror(PYTHON_TELEMETRY_DIR);
                exit(EXIT_FAILURE);
        }

        /* Process all existing files */
        dir = opendir(PYTHON_TELEMETRY_DIR);
        if (!dir) {
                perror(PYTHON_TELEMETRY_DIR);
                exit(EXIT_FAILURE);
        }

        while (true) {
                entry = readdir(dir);
                if (!entry) {
                        break;
                }
                if (entry->d_name[0] == '.') {
                        continue;
                }
                deliver_payload(entry->d_name);
        }

        closedir(dir);

        /* Wait for any new python exception files to appear */
        wait_for_payload();
        exit(EXIT_SUCCESS);
}
