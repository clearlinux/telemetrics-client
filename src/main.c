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

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <assert.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <getopt.h>
#include <sys/queue.h>
#include <time.h>
#include <unistd.h>
#include <sys/signalfd.h>
#include <signal.h>
#include <malloc.h>

#include "config.h"
#include "common.h"
#ifdef  HAVE_SYSTEMD_SD_DAEMON_H

/* Certain static analysis tools do not understand GCC's __INCLUDE_LEVEL__
 * macro; the conditional definition below is used to fix the build with
 * systemd's _sd-common.h, included by the public systemd headers, which rely on
 * the macro being defined and having an accurate value.
 */
#ifndef __INCLUDE_LEVEL__
# define __INCLUDE_LEVEL__ 2
#endif

#include <systemd/sd-daemon.h>
#endif
#include "log.h"
#include "telemdaemon.h"
#include "configuration.h"
#include "retention.h"
#include "spool.h"

/*
 *  Using a function pointer for unit testing to isolate the call to actual post function.
 *  The call to post the record to the server is stubbed out in the unit tests
 *  using pointer to a fake function.
 */
bool (*post_record_ptr)(char *[], char *, bool) = post_record_http;

void print_usage(char *prog)
{
        printf("%s: Usage\n", prog);
        printf("  -f,  --config_file    Configuration file. This overides the other parameters\n");
        printf("  -h,  --help           Display this help message\n");
        printf("  -V,  --version        Print the program version\n");
}

int main(int argc, char **argv)
{
        struct sockaddr_un addr;
        int sockfd, fd, sigfd;
        int ret = 0;
        TelemDaemon daemon;
        nfds_t i;
        client *cl = NULL;
        client *current_client = NULL;
        int c;
        char *config_file = NULL;
        int opt_index = 0;
        nfds_t initial_nfds = 0;
        sigset_t mask;
        int pending_records;
        //bool interrupted = false;

        if (setenv("LC_ALL", "C", 1)) {
                perror("Cannot set LC_ALL to C");
        }

        struct option opts[] = {
                { "config_file", 1, NULL, 'f' },
                { "help", 0, NULL, 'h' },
                { "version", 0, NULL, 'V' },
                { NULL, 0, NULL, 0 }
        };

        while ((c = getopt_long(argc, argv, "f:hV", opts, &opt_index)) != -1) {
                switch (c) {
                        case 'f':
                                config_file = optarg;
                                struct stat buf;
                                ret = stat(config_file, &buf);

                                /* check if the file exists and is a regular file */
                                if (ret == -1 || !S_ISREG(buf.st_mode)) {
                                        telem_log(LOG_ERR, "Configuration file"
                                                  " path not valid\n");
                                        exit(EXIT_FAILURE);
                                }
                                set_config_file(config_file);
                                break;
                        case 'h':
                                print_usage(argv[0]);
                                exit(EXIT_SUCCESS);
                        case 'V':
                                printf(PACKAGE_VERSION "\n");
                                exit(EXIT_SUCCESS);
                        case '?':
                                /* get_opt will print an error message */
                                print_usage(argv[0]);
                                exit(EXIT_FAILURE);
                }
        }
        initialize_daemon(&daemon);

        /* Register record retention delete action as a callback to prune entry */
        if (daemon.record_journal) {
                daemon.record_journal->prune_entry_callback = &delete_record_by_id;
        }

        sigemptyset(&mask);

        if (sigaddset(&mask, SIGHUP) != 0) {
                telem_perror("Error adding signal SIGHUP to mask");
                exit(EXIT_FAILURE);
        }

        if (sigaddset(&mask, SIGINT) != 0) {
                telem_perror("Error adding signal SIGINT to mask");
                exit(EXIT_FAILURE);
        }

        if (sigaddset(&mask, SIGTERM) != 0) {
                telem_perror("Error adding signal SIGTERM to mask");
                exit(EXIT_FAILURE);
        }

        if (sigaddset(&mask, SIGPIPE) != 0) {
                telem_perror("Error adding signal SIGPIPE to mask");
                exit(EXIT_FAILURE);
        }

        if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
                telem_perror("Error changing signal mask with SIG_BLOCK");
                exit(EXIT_FAILURE);
        }

        sigfd = signalfd(-1, &mask, 0);
        if (sigfd == -1) {
                telem_perror("Error creating the signalfd");
                exit(EXIT_FAILURE);
        }
        add_pollfd(&daemon, sigfd, POLLIN);

#ifdef HAVE_SYSTEMD_SD_DAEMON_H
        ret = sd_listen_fds(0);
        telem_log(LOG_INFO, "Number of file descriptors from systemd: %d\n",
                  ret);

        if (ret >= 1) {
                fd = SD_LISTEN_FDS_START + 0;

                /* Check if the socket is of correct type */
                if (sd_is_socket_unix(fd, SOCK_STREAM, 1, socket_path_config(), 0)) {
                        telem_log(LOG_INFO, "Socket of type AF_UNIX passed by systemd\n");
                        add_pollfd(&daemon, fd, POLLIN | POLLPRI);
                } else if (sd_is_socket(fd, AF_UNSPEC, 0, -1)) {
                        telem_log(LOG_INFO, "Socket of type SOCKET passed by systemd\n");
                        add_pollfd(&daemon, fd, POLLIN | POLLPRI);
                } else {
                        telem_log(LOG_ERR, "File descriptor other than socket passed by systemd\n");
                        exit(EXIT_FAILURE);
                }

                sockfd = fd;
        } else if (ret < 0) {
                telem_log(LOG_ERR, "sd_listen_fds() failed: %s\n", strerror(-ret));
                exit(EXIT_FAILURE);
        } else
#endif
        {
                sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
                if (sockfd < 0) {
                        telem_perror("Socket creation failed");
                        exit(EXIT_FAILURE);
                }

                memset(&addr, 0, sizeof(addr));
                addr.sun_family = AF_UNIX;
                strncpy(addr.sun_path, socket_path_config(), sizeof(addr.sun_path) - 1);
                addr.sun_path[sizeof(addr.sun_path) - 1] = 0;

                ret = unlink(addr.sun_path);
                if (ret == -1 && errno != ENOENT) {
                        telem_perror("Failed to unlink socket");
                        exit(EXIT_FAILURE);
                }

                if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
                        telem_perror("Failed to bind socket to address");
                        exit(EXIT_FAILURE);
                }

                if (chmod(addr.sun_path, 0666) == -1) {
                        telem_perror("Failed to change socket permissions");
                        exit(EXIT_FAILURE);
                }

                if (listen(sockfd, SOMAXCONN) == -1) {
                        telem_perror("Failed to mark socket as passive");
                        exit(EXIT_FAILURE);
                }

                /*Add listener fd to array of pollfds */
                add_pollfd(&daemon, sockfd, POLLIN | POLLPRI);
        }

        telem_log(LOG_INFO, "Listening on socket...\n");

        /* Calculate the spool directory size */
        if (is_spool_valid()) {
                daemon.is_spool_valid = true;
                daemon.current_spool_size = get_spool_dir_size();
        }

        int spool_process_time = spool_process_time_config();

        pending_records = spool_records_loop(&(daemon.current_spool_size));
        time_t last_spool_run_time = time(NULL);
        time_t last_daemon_start_time = time(NULL);

        bool daemon_recycling_enabled = daemon_recycling_enabled_config();

        ret = update_machine_id();
        if (ret == -1) {
                telem_log(LOG_ERR, "Unable to update machine id\n");
        }

        /* Read the static machine id file if it exists.*/
        daemon.machine_id_override = read_machine_id_override();

        time_t last_refresh_time = time(NULL);

        /* Save initial count for non connection fds */
        initial_nfds = daemon.nfds;

        /* Loop to accept clients */
        while (1) {
                static int requeue = 0;
                int delay = spool_process_time;

                malloc_trim(0);
                if (initial_nfds == daemon.nfds) {
                        ret = prune_journal(daemon.record_journal, JOURNAL_TMPDIR);
                        if (ret != 0) {
                                telem_log(LOG_WARNING, "Unable to prune journal\n");
                        }
                }

                if ((pending_records > 0) && (requeue < 8)) {
                        requeue++;
                        delay = requeue * requeue;
                } else {
                        requeue = 0;
                }

                ret = poll(daemon.pollfds, daemon.nfds, delay * 1000);
                if (ret == -1) {
                        telem_perror("Failed to poll daemon file descriptors");
                        break;
                } else if (ret != 0) {
                        for (i = 0; i < daemon.nfds; i++) {

                                /* Check if a signal was received */
                                if (daemon.pollfds[0].revents != 0) {
                                        struct signalfd_siginfo fdsi;
                                        ssize_t s;

                                        s = read(sigfd, &fdsi, sizeof(struct signalfd_siginfo));
                                        if (s != sizeof(struct signalfd_siginfo)) {
                                                telem_perror("Error while reading from the signal"
                                                             "file descriptor");
                                                exit(EXIT_FAILURE);
                                        }

                                        if (fdsi.ssi_signo == SIGTERM || fdsi.ssi_signo == SIGINT) {
                                                telem_log(LOG_INFO, "Received either a \
                                                                     SIGINT/SIGTERM signal\n");
                                                goto clean_exit;
                                        }

                                        if (fdsi.ssi_signo == SIGHUP) {
                                                telem_log(LOG_INFO, "Received a SIGHUP signal\n");
                                                /* reload configuration file */
                                                reload_config();

                                                if (is_spool_valid()) {
                                                        daemon.is_spool_valid = true;
                                                        //calc_spool_dir_size();
                                                        daemon.current_spool_size = get_spool_dir_size();
                                                }
                                        }
                                }

                                if (daemon.pollfds[i].revents == 0) {
                                        continue;
                                }

                                /* Accept connection if data arrives on listening socket */
                                if (daemon.pollfds[i].fd == sockfd) {
                                        if ((fd = accept(daemon.pollfds[i].fd, NULL, NULL)) == -1) {
                                                telem_perror("Failed to accept socket");
                                                //exit(EXIT_FAILURE);
                                                break;
                                        }
                                        telem_log(LOG_INFO, "New client %d connected\n", fd);

                                        /* set socket to non-blocking */
                                        if (fcntl(fd, F_SETFL, O_NONBLOCK)) {
                                                telem_perror("Failed to set socket as nonblocking");
                                                close(fd);
                                                break;
                                        }

                                        struct timeval timeout;
                                        timeout.tv_sec = 10;
                                        timeout.tv_usec = 0;

                                        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                                                       sizeof(timeout)) < 0) {
                                                telem_perror("Failed to set socket timeout");
                                        }

                                        /* Add the fd to the client list */
                                        if (!add_client(&(daemon.client_head), fd)) {
                                                telem_log(LOG_ERR, "Unable to add the client to list\n");
                                                exit(EXIT_FAILURE);
                                        }

                                        /* Add fd to the poll array */
                                        add_pollfd(&daemon, fd, POLLIN | POLLPRI);
                                } else if (i != 0) {
                                        /* Lookup client and handle data on client */
                                        LIST_FOREACH(cl, &(daemon.client_head), client_ptrs) {
                                                if (cl->fd == daemon.pollfds[i].fd) {
                                                        current_client = cl;
                                                        telem_log(LOG_INFO, "Client found: %d\n",
                                                                  current_client->fd);
                                                }
                                        }
                                        assert(current_client);
                                        handle_client(&daemon, i, current_client);
                                }
                        }
                } else {
                        time_t now = time(NULL);
                        /* If spool is empty and time to recycle the daemon has elapsed*/
                        if (daemon_recycling_enabled && (daemon.current_spool_size == 0) &&
                            difftime(now, last_daemon_start_time) >= TM_DAEMON_EXIT_TIME) {
                                /* Exit */
                                telem_log(LOG_INFO, "Daemon exiting for recycling\n");
                                goto clean_exit;
                        }
                }

                time_t now = time(NULL);
                if (difftime(now, last_spool_run_time) >= spool_process_time) {
                        pending_records = spool_records_loop(&(daemon.current_spool_size));
                        last_spool_run_time = time(NULL);
                }

                if (difftime(now, last_refresh_time) >= TM_REFRESH_RATE) {
                        int ret = update_machine_id();
                        if (ret == -1) {
                                telem_log(LOG_ERR, "Unable to update machine id\n");
                        }
                        last_refresh_time = time(NULL);
                }
        }

clean_exit:
        close_journal(daemon.record_journal);

        /* Free memory before exiting */
        while ((cl = LIST_FIRST(&(daemon.client_head))) != NULL) {
                remove_client(&(daemon.client_head), cl);
        }
        free(daemon.pollfds);

        if (LIST_EMPTY(&(daemon.client_head))) {
                telem_log(LOG_INFO, "Client list cleared\n");
        }

        return 0;
}

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
