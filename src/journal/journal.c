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

#define ID_LEN 32
#define BOOTID_LEN 36
#define BOOTID_FILE "/proc/sys/kernel/random/boot_id"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "util.h"
#include "common.h"
#include "journal.h"


/**
 * Packs journal entry values into one line string separated
 * by '\036' RS.
 *
 * @param entry A pointer to a JournalEntry struct with values
 *              to serialize.
 * @param buff A pointer to be initialized and set with the
 *             serialized values.
 *
 * @return 0 on success, -1 on failure.
 */
static int serialize_journal_entry(struct JournalEntry *entry, char **buff)
{
        int rc = 0;

        if (asprintf(buff, "%s\036%lld\036%s\036%s\036%s", entry->record_id, (long long int) entry->timestamp,
                     entry->classification, entry->event_id, entry->boot_id) < 0) {
#ifdef DEBUG
                fprintf(stderr, "Error: unable to serialize data in buff\n");
#endif
                rc = -1;
        }

        return rc;
}

/**
 *  Frees journal entry struct members and journal entry pointer.
 *
 *  @param the pointer to the entry structure to free.
 */
static void free_journal_entry(struct JournalEntry *entry)
{
        if (entry) {
                free(entry->classification);
                free(entry->record_id);
                free(entry->event_id);
                free(entry->boot_id);
                free(entry);
        }
}

/**
 * Sets last character of a string to '\0' if the character
 * is '\n'.
 *
 * @param l a pointer to string.
 *
 */
static void strip_new_line(char *l)
{
        size_t len;

        len = strlen(l);
        if (*(l+len-1) == '\n') {
                *(l+len-1) = '\0';
        }
}

/**
 * Unpacks a record that was stored in file and initializes a
 * JournalEntry struct with parsed values.
 *
 * @param line A pointer to the data to unpack.
 * @param entry A pointer to be initialized and set with values
 *        parsed from parameter line.
 *
 * @return 0 on success, -1 on failure.
 */
static int deserialize_journal_entry(char *line, struct JournalEntry **entry)
{
        int rc = -1;
        size_t len = 0;
        size_t offset = 0;
        struct JournalEntry *e = NULL;

        e = malloc(sizeof(struct JournalEntry));
        if (!e) {
                return -1;
        }

        for (int i = 0; i < 5; i++) {
                char *out = NULL;
                len = strcspn(line+offset, "\036");
                out = strndup((line+offset), len);
                offset += len + 1;
                if (out) {
                        switch (i) {
                                case 0:
                                     e->record_id = out;
                                     break;
                                case 1:
                                     e->timestamp = atoll(out);
                                     free(out);
                                     break;
                                case 2:
                                     e->classification = out;
                                     break;
                                case 3:
                                     e->event_id = out;
                                     break;
                                case 4:
                                     strip_new_line(out);
                                     e->boot_id = out;
                                     rc = 0;
                                     break;
                        }
                } else {
                        rc = -1;
                        break;
                }
        }

        if (rc == 0) {
                *entry = e;
        } else {
                free_journal_entry(e);
        }

        return rc;
}

/**
 * Counts the number of records in journal.
 *
 * @param telem_journal Pointer to a telemetry journal
 *        to get record count.
 *
 * @return record count
 *
 */
static int get_record_count(struct TelemJournal *telem_journal)
{
        int count = 0;
        size_t len;
        ssize_t read;
        char *line = NULL;

        rewind(telem_journal->fptr);
        // read until the end line by line
        while((read = getline(&line, &len, telem_journal->fptr)) != -1) {
                count++;
        }
        free(line);

        return count;
}

/**
 * Serializes JournalEntry and saves it to file.
 *
 * @param fptr A FILE pointer.
 * @param entry A pointer to data.
 *
 * @return 0 on succes, !=0 on failure
 */
static int save_entry(FILE *fptr, struct JournalEntry *entry)
{
        int rc = -1;
        char *serialized_data = NULL;

        if ((rc = serialize_journal_entry(entry, &serialized_data)) == 0) {
#ifdef DEBUG
                printf("Saving: %s\n", serialized_data);
#endif
                fprintf(fptr, "%s\n", serialized_data);
                fflush(fptr);
                free(serialized_data);
        }

        return rc;
}

/**
 * Reads the boot unique identifier from BOOTID_FILE
 *
 * @param buff pointer to be initialized and set with value
 *             read from boot_id file.
 *
 * @return 0 on sucess, -1 on failure
 */
static int read_boot_id(char **buff)
{
        int rc = -1;
        FILE *fs = NULL;

        *buff = (char*)malloc((size_t)BOOTID_LEN + 1);
        if (!buff) {
                return rc;
        }

        fs = fopen(BOOTID_FILE, "r");
        if (!fs) {
                return rc;
        }

        if (fgets(*buff, BOOTID_LEN + 1, fs)){
                *(char *)(*buff+BOOTID_LEN)='\0';
                rc = 0;
        }
        fclose(fs);

        return rc;
}

/**
 * Moves the file position indicator n lines.
 *
 * @param n Number of lines to skip.
 * @param fptr A file pointer.
 *
 * @returns 0 on sucess, 1 if end was reached before
 *          line n
 */
static int skip_n_lines(int n, FILE *fptr)
{
        int rc = 0;
        size_t len = 0;
        char *line = NULL;

        fseek(fptr, 0, 0);
        for(int i = 0; i < n; i++) {
                if (getline(&line, &len, fptr) == -1) {
                        rc = 1;
                        break;
                }
        }
        free(line);

        return rc;
}

/**
 * Copies contents of fptr to a known location.
 *
 * @param fptr A file pointer from source file.
 *
 * @returns 0 on success, 1 on failure
 */
static int copy_to_tmp(FILE *fptr)
{
        size_t len;
        char *line = NULL;
        FILE *fptr_tmp = NULL;

        fptr_tmp = fopen(JOURNAL_TMP, "w");
        if (!fptr_tmp) {
                return 1;
        }

        // Copy lines to new file
        while (getline(&line, &len, fptr) > -1) {
                fprintf(fptr_tmp, "%s", line);
        }

        fclose(fptr_tmp);
        free(line);

        return 0;
}

/* Exported function */
TelemJournal *open_journal(const char *journal_file)
{
        FILE *fptr = NULL;
        char *boot_id = NULL;
        struct TelemJournal *telem_journal;

        // Use default location if journal_file parameter is NULL
        fptr = (journal_file == NULL) ? fopen(JOURNAL_PATH, "a+") :
               fopen(journal_file, "a+");

        if (fptr == NULL) {
#ifdef DEBUG
                printf("Unable to open journal on disk\n");
#endif
                return NULL;
        }

        if (read_boot_id(&boot_id) != 0) {
#ifdef DEBUG
                printf("Failure reading boot_id\n");
#endif
                return NULL;
        }

        telem_journal = malloc(sizeof(struct TelemJournal));

        telem_journal->fptr = fptr;
        telem_journal->journal_file = \
                (journal_file == NULL) ? strdup(JOURNAL_PATH) : strdup(journal_file);
        telem_journal->boot_id = boot_id;
        telem_journal->record_count = get_record_count(telem_journal);
        telem_journal->record_count_limit = RECORD_LIMIT;

#ifdef DEBUG
        printf("Records in db: %d\n", telem_journal->record_count);
#endif

        return telem_journal;
}

/* Exported function */
void close_journal(TelemJournal *telem_journal)
{
	if (telem_journal) {
                free(telem_journal->boot_id);
                free(telem_journal->journal_file);
                fclose(telem_journal->fptr);
                free(telem_journal);
        }
}

/* Exported function */
int print_journal(TelemJournal *telem_journal, char *classification,
                   char *record_id, char *event_id, char *boot_id)
{
        int n = 0;
        int count = 0;
        char str_time[80] = {0};
        char *line = NULL;
        size_t len = 0;
        FILE *journal_fileptr = NULL;
        struct tm ts;
        struct JournalEntry *entry = NULL;

        if (telem_journal == NULL) {
                return -1;
        }

        journal_fileptr = fopen(telem_journal->journal_file, "r");
        if (!journal_fileptr) {
                return -1;
        }

        // Advance file pointer to line n
        n = telem_journal->record_count - telem_journal->record_count_limit;
        if (n > 0) {
                skip_n_lines(n, journal_fileptr);
        }

        while (getline(&line, &len, journal_fileptr) != -1) {
                deserialize_journal_entry(line, &entry);
                if (entry) {
                        /* filter entry out if one is provided */
                        if (record_id != NULL && strncmp(entry->record_id, record_id, ID_LEN) != 0) {
                                continue;
                        }
                        if (classification != NULL && strncmp(entry->classification, classification, strlen(entry->classification)) != 0) {
                                continue;
                        }
                        if (boot_id != NULL && strncmp(entry->boot_id, boot_id, BOOTID_LEN) != 0) {
                                continue;
                        }
                        if (event_id != NULL && strncmp(entry->event_id, event_id, strlen(entry->event_id)) != 0) {
                                continue;
                        }
                        /* end filters section */
                        ts = *localtime(&entry->timestamp);
                        if (strftime(str_time, sizeof(str_time), "%a %Y-%m-%d %H:%M:%S %Z", &ts) == 0) {
                                continue;
                        }

                        fprintf(stdout, "%-30s %s %s %s %s\n", entry->classification, str_time, entry->record_id, entry->event_id, entry->boot_id);
                        count++;
                }
                free_journal_entry(entry);
        }
        free(line);
        fclose(journal_fileptr);

        return count;
}

/**
 * Validate classification
 * TODO: move to util or common and use it in telemetry.c
 *
 * @param classification A pointer with the value to validate.
 *
 * @returns 0 on success, 1 on failure
 */
static int validate_classification(char *classification)
{
        size_t i, j;
        int slashes = 0;
        int x = 0;

        if (classification == NULL) {
                return 1;
        }

        j = strlen(classification);

        if (j > MAX_CLASS_LENGTH) {
                return 1;
        }

        for (i = 0, x = 0; i <= (j - 1); i++, x++) {
                if (classification[i] == '/') {
                        slashes++;
                        x = 0;
                } else {
                        if (x > MAX_SUBCAT_LENGTH) {
                                return 1;
                        }
                }
        }

        if (slashes != 2) {
#ifdef DEBUG
                fprintf(stderr, "ERR: Classification string should have two /s.\n");
#endif
                return 1;
        }

        return 0;
}


/**
 * Validation for event id.
 *
 * @param event_id A pointer to id value.
 *
 * @return 0 on success, 1 on failure
 */
static int validate_event_id(char *event_id)
{
        const char alphab[] = EVENT_ID_ALPHAB;

        if (event_id == NULL) {
                return 1;
        }

        if (strlen(event_id) != EVENT_ID_LEN) {
                return 1;
        }

        if (strspn(event_id, alphab) != EVENT_ID_LEN) {
                return 1;
        }

        return 0;
}

/* Exported function */
int new_journal_entry(TelemJournal *telem_journal, char *classification,
                      time_t timestamp, char *event_id)
{
       int rc = 1;
       char *boot_id = NULL;
       char *record_id = NULL;
       struct JournalEntry *entry = NULL;

       if (telem_journal == NULL) {
#ifdef DEBUG
               fprintf(stderr, "Error: telem_journal was not initialized\n");
#endif
               return rc;
       }

       if (validate_classification(classification) != 0) {
               return rc;
       }

       if (validate_event_id(event_id) != 0) {
               return rc;
       }

       entry = malloc(sizeof(struct JournalEntry));
       entry->classification = NULL;
       entry->record_id = NULL;
       entry->event_id = NULL;
       entry->boot_id = NULL;

       if (get_random_id(&record_id) != 0) {
#ifdef DEBUG
               fprintf(stderr, "Error: unable to generate random id\n");
#endif
               goto quit;
       }

       if (read_boot_id(&boot_id) != 0) {
#ifdef DEBUG
               fprintf(stderr, "Error: unable to read boot_id\n");
#endif
               goto quit;
       }

       if ((entry->classification = strdup(classification)) == NULL) {
               goto quit;
       }
       if ((entry->event_id = strdup(event_id)) == NULL) {
               goto quit;
       }

       entry->record_id = record_id;
       entry->boot_id = boot_id;
       entry->timestamp = timestamp;

       if ((rc = save_entry(telem_journal->fptr, entry)) == 0) {
               telem_journal->record_count = telem_journal->record_count + 1;
#ifdef DEBUG
               fprintf(stdout, "%d records in journal\n", telem_journal->record_count);
#endif
       }

quit:
       free_journal_entry(entry);

       return rc;
}

/* Exported function */
int prune_journal(struct TelemJournal *telem_journal)
{

    int rc = 1;
    int count = 0;
    int deviation = DEVIATION;

    if ( telem_journal->record_count > (deviation + telem_journal->record_count_limit)) {
        count = telem_journal->record_count - telem_journal->record_count_limit;

        // jump to line# count
        if ( skip_n_lines(count, telem_journal->fptr) != 0) {
#ifdef DEBUG
            fprintf(stderr, "Error: skipping %d journal lines\n", count);
#endif
            return rc;
        }
        // create new file with rest of file
        if ( copy_to_tmp(telem_journal->fptr) != 0) {
#ifdef DEBUG
            fprintf(stderr, "Error: copying partial journal to temp journal file\n");
#endif
            return rc;
        }
        // close file handler
        if ( fclose(telem_journal->fptr) != 0) {
#ifdef DEBUG
            fprintf(stderr, "Error: clossing journal file handler\n");
#endif
            return rc;
        }
        // overwrite file
        if ( rename(JOURNAL_TMP, JOURNAL_PATH) != 0) {
#ifdef DEBUG
            fprintf(stderr, "Error: while overriding journal file\n");
#endif
            return rc;
        }
        // reopen file handler
        telem_journal->fptr = fopen(JOURNAL_PATH, "a+");
        if ( !telem_journal->fptr ) {
#ifdef DEBUG
            fprintf(stderr, "Error: re-opening journal file\n");
#endif
            return rc;
        }
        // update record count
        telem_journal->record_count = (count >= telem_journal->record_count_limit) ?
                                      telem_journal->record_count_limit :
                                      telem_journal->record_count - count;
#ifdef DEBUG
        fprintf(stdout, "record_count: %d\n", telem_journal->record_count);
#endif
        rc = 0;
    }

    return rc;
}

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
