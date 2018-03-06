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

#define _GNU_SOURCE

/* default record limit */
#define RECORD_LIMIT 100
#define DEVIATION 50

#include <time.h>
#include <stdio.h>

/* Journal entry type */
typedef struct JournalEntry {
        time_t timestamp;
        char *classification;
        char *record_id;
        char *event_id;
        char *boot_id;
} JournalEntry;

/* Telemetry journal type */
typedef struct TelemJournal {
        FILE *fptr;
        char *journal_file;
        char *boot_id;
        int record_count;
        int record_count_limit;
} TelemJournal;

/**
 * Telemetry journal initialization.
 *
 * @param journal_file A pointer to a string containing
 *        the full path to file used as journal storage.
 *        If journal_file is set to NULL a default value
 *        will be used.
 *
 * @returns a telemetry journal structure in success or
 *          NULL in case of failure.
 */
TelemJournal *open_journal(const char *journal_file);

/**
 * Closes journal file and deallocates memory that was
 * previously initialized by open_journal call.
 *
 * @param telem_journal A pointer to struct that has a
 *         reference to journal file pointer and other
 *         members.
 */
void close_journal(TelemJournal *telem_journal);

/**
 * Prints journal contents to stdout. Use function parameters
 * to filter journal entries to print.
 *
 * @param telem_journal A pointer to struct initialized
 *        by open_journal call.
 * @param classification A pointer to string used as
 *        classification filter.
 * @param record_id A pointer to string used as record_id filter.
 * @param event_id A pointer to string used as event_id filter.
 * @param boot_id A pointer to string used as boor_id filter.
 *
 * @return the number of lines printed to stdout on success, -1
 *         on failure.
 */
int print_journal(TelemJournal *telem_journal, char *classification,
                  char *record_id, char *event_id, char *boot_id);

/**
 * Creates a new entry in journal.
 *
 * @param telem_journal A pointer to telemetry journal.
 * @param classification A pointer to a classification value.
 * @param timestamp Record creation time stamp value.
 * @param event_id A pointer to a unique id value for record
 *        event.
 *
 * @return 0 on success 1 on failure.
 */
int new_journal_entry(TelemJournal *telem_journal, char *classification,
                      time_t timestamp, char *event_id);

/**
 * Checks number of lines in log and prunes the oldest records
 * if journal grows more than telem_journal->record_count_limit.
 *
 * @param telem_journal A pointer to telemetry journal struct
 *        returned by open_journal call.
 *
 * @return 0 on success, 1 on failure
 */
int prune_journal(TelemJournal *telem_journal);



/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
