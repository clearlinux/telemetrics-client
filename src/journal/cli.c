/**
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
 *
 * */

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>

#include "journal.h"

void print_usage(void)
{
        printf(" Usage\n");
        printf("  -r,  --record_id       Print record to stdout\n");
        printf("  -e,  --event_id        List records with specific event_id\n");
        printf("  -c,  --classification  List records with specific classification\n");
        printf("  -b,  --boot_id         List records with specific boot_id\n");
        printf("  -V,  --verbose         Verbose output\n");
        printf("  -h,  --help            Display this help message\n");
}


int main(int argc, char **argv){

        int rc = 0;
        int count = 0;
        int verbose_output = 0;
        char *boot_id = NULL;
        char *record_id = NULL;
        char *event_id = NULL;
        char *classification = NULL;
        struct TelemJournal *telem_journal = NULL;

        /** opts */
        int c;
        int opt_index = 0;
        struct option opts[] = {
               { "record_id", 1, NULL, 'r' },
               { "event_id", 1, NULL, 'e' },
               { "classification", 1, NULL, 'c' },
               { "boot_id", 1, NULL, 'b' },
               { "verbose", 0, NULL, 'V' },
               { "help", 0, NULL, 'h' },
               { NULL, 0, NULL, 0}
        };

        while((c = getopt_long(argc, argv, "r:e:c:Vh", opts, &opt_index)) != -1) {
               switch(c) {
                       case 'r':
                               record_id = strdup((char*)optarg);
                               printf("record_id: %s\n", record_id);
                               break;
                       case 'e':
                               event_id = strdup((char*)optarg);
                               printf("event_id: %s\n", event_id);
                               break;
                       case 'c':
                               classification = strdup((char*)optarg);
                               printf("classification: %s\n", classification);
                               break;
                       case 'b':
                               boot_id = strdup((char*)optarg);
                               printf("boot_id: %s\n", boot_id);
                               break;
                       case 'V':
                               verbose_output = 1;
                               break;
                       case 'h':
                               print_usage();
                               exit(EXIT_SUCCESS);
               }
        }

        if ((telem_journal = open_journal(NULL))){
                if (verbose_output) {
                        fprintf(stdout, "%-30s %-27s %-32s %-32s %-36s\n", "Classification", "Time stamp",
                                        "Record ID", "Event ID", "Boot ID");
                }
                count = print_journal(telem_journal, classification, record_id, event_id, boot_id);
                if (verbose_output) {
                        fprintf(stdout, "Total records: %d\n", count);
                }
                close_journal(telem_journal);
        } else {
                fprintf(stderr, "Unable to open journal\n");
        }

        free(classification);
        free(record_id);
        free(event_id);
        free(boot_id);

        return rc;
}
