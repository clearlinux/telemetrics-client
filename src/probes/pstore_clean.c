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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/mount.h>
#include <dirent.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>

#include "log.h"

/* Location where the pstore files are copied */
const char *DEST = PSTOREDIR;

/* Location where the pstore file system will be mounted */
const char *MOUNT = "/dev/pstore";

const size_t BUFFER_SIZE = 4096;

int copy_file(char *src_file, char *dest_file)
{
        int src_fd = -1, dest_fd = -1;
        char buffer[BUFFER_SIZE];
        ssize_t bytes_read, bytes_written;
        int ret = -1;

        src_fd = open(src_file, O_RDONLY);
        if (src_fd == -1) {
                telem_log(LOG_ERR, "Failed to open file %s:%s\n", src_file,
                          strerror(errno));
                goto end;
        }

        dest_fd = open(dest_file, O_WRONLY | O_CREAT, 0644);
        if (dest_fd == -1) {
                telem_log(LOG_ERR, "Failed to create file %s:%s\n", dest_file,
                          strerror(errno));
                goto end;
        }

        do {
                memset(buffer, 0, BUFFER_SIZE);
                bytes_read = read(src_fd, buffer, BUFFER_SIZE);
                if (bytes_read == -1) {
                        telem_log(LOG_ERR, "Failed to read file %s:%s\n",
                                  src_file, strerror(errno));
                        goto end;
                }

                bytes_written = write(dest_fd, buffer, (size_t)bytes_read);
                if (bytes_written == -1) {
                        telem_log(LOG_ERR, "Failed to write file %s:%s\n",
                                  dest_file, strerror(errno));
                        goto end;
                }
        } while (bytes_read > 0 && bytes_written > 0);

        if (chmod(dest_file, 0444) < 0) {
                telem_log(LOG_ERR, "Failed changing permissions for file %s: %s\n",
                          dest_file, strerror(errno));
        }

        ret = 0;
end:
        if (src_fd >= 0) {
                close(src_fd);
        }
        if (dest_fd >= 0) {
                close(dest_fd);
        }
        return ret;

}

/*
 * The pstore is mounted at location /dev/pstore. The files from the pstore are then copied to a
 * location defined by DEST. The files from pstore are then deleted to make room for future crash
 * logs
 */
int main(int argc, char **argv)
{
        int ret;
        DIR *mnt_dir, *dest_dir;
        struct dirent *entry;
        char *src_file, *dest_file;
        int exit_code = EXIT_FAILURE;

        /*
         * Clean the DEST dir first in case it wasnt cleaned up by the pstore-probe.
         * This should only happen when the telemetry is disabled
         */
        dest_dir = opendir(DEST);
        if (!dest_dir) {
                /* This shouldnt happen. In any case log, and continue */
                telem_log(LOG_ERR, "Failed to open directory %s : %s\n", DEST, strerror(errno));
        } else {
                while ((entry = readdir(dest_dir)) != NULL) {
                        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) {
                                continue;
                        }
                        if (asprintf(&dest_file, "%s/%s", DEST, entry->d_name) == -1) {
                                exit(EXIT_FAILURE);
                        }
                        if (remove(dest_file) == -1) {
                                telem_log(LOG_ERR, "Failed to remove file %s : %s\n", dest_file, strerror(errno));
                        }
                        free(dest_file);
                }
                closedir(dest_dir);
        }

        if (mkdir(MOUNT, 0755) < 0) {
                //If there is an error other than an existing directory
                if (errno != EEXIST) {
                        telem_perror("Error creating directory /dev/pstore");
                        //bail out?
                        exit(EXIT_FAILURE);
                }
        }

        ret = mount("pstore", MOUNT, "pstore", 0, NULL);
        if (ret == -1) {
                telem_perror("Failed to mount pstore directory");
                goto error;
        }

        mnt_dir = opendir(MOUNT);
        if (!mnt_dir) {
                telem_perror("Error opening pstore mount directory");
                goto error;
        }

        while ((entry = readdir(mnt_dir)) != NULL) {
                if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) {
                        continue;
                }

                if (asprintf(&src_file, "%s/%s", MOUNT, entry->d_name) == -1) {
                        goto mem_error;
                }
                if (asprintf(&dest_file, "%s/%s", DEST, entry->d_name) == -1) {
                        goto mem_error;
                }

                if (copy_file(src_file, dest_file) == -1) {
                        telem_log(LOG_ERR, "Failed to copy %s file in pstore\n", entry->d_name);
                }

                unlink(src_file);
                free(src_file);
                free(dest_file);
        }
        exit_code = 0;

mem_error:
        closedir(mnt_dir);

error:
        umount(MOUNT);
        rmdir(MOUNT);
        return exit_code;
}

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
