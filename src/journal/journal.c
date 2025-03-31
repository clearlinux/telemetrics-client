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
#define BOOTID_LEN 37 // Includes the \n character at the end
#define BOOTID_FILE "/proc/sys/kernel/random/boot_id"
#define MID_BUFF 1024

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#include "log.h"
#include "util.h"
#include "common.h"
#include "journal.h"

/*
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

        if (entry == NULL) {
                return -1;
        }

        if (asprintf(buff, "%s\036%lld\036%s\036%s\036%s", entry->record_id, (long long int)entry->timestamp,
                     entry->classification, entry->event_id, entry->boot_id) < 0) {
                telem_log(LOG_ERR, "Error: Unable to serialize data in buff\n");
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
        size_t llen = 0;
        size_t offset = 0;
        struct JournalEntry *e = NULL;

        /* Assume an error... */
        *entry = NULL;

        if (line == NULL || !strlen(line)) {
                return -1;
        }

        e = calloc(1, sizeof(struct JournalEntry));
        if (!e) {
                return -1;
        }

        llen = strlen(line);

        for (int i = 0; i < 5; i++) {
                char *out = NULL;
                if (offset > llen) {
                        rc = -1;
                        break;
                }
                len = strcspn(line + offset, "\036\n");
                out = strndup((line + offset), len);
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
        while ((read = getline(&line, &len, telem_journal->fptr)) != -1) {
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

        if (fptr == NULL || entry == NULL) {
                return rc;
        }

        if ((rc = serialize_journal_entry(entry, &serialized_data)) == 0) {
                telem_debug("DEBUG: Saving: %s\n", serialized_data);
                fprintf(fptr, "%s\n", serialized_data);
                fflush(fptr);
                free(serialized_data);
        }

        return rc;
}

/**
 * Reads the boot unique identifier from BOOTID_FILE
 *
 * @param buff pointer to a BOOTID_LEN allocated
 *
 * @return 0 on success, -1 on failure
 */
static int read_boot_id(char buff[])
{
        int rc = -1;
        FILE *fs = NULL;

        fs = fopen(BOOTID_FILE, "r");
        if (!fs) {
                telem_log(LOG_ERR, "Error: Unable to open %s for reading: %d\n", BOOTID_FILE, errno);
                return rc;
        }

        if (fgets(buff, BOOTID_LEN, fs)) {
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
 * @param found_record A pointer to a function to be called
 *                     every time a valid record is found, if
 *                     pointer is not set to null.
 *
 * @returns 0 on success, on failure -1 if n > existing
 *            lines in file or errno if failure happens
 *            during file operation.
 */
static int skip_n_lines(int n, FILE *fptr, int (*found_record)(char *))
{
        int rc = 0;
        size_t len = 0;
        char *line = NULL;
        struct JournalEntry *entry = NULL;

        if (fptr == NULL) {
                telem_log(LOG_ERR, "fptr argument to skip_n_lines is invalid\n");
                // File descriptor in bad state
                return EBADFD;
        }

        if (fseek(fptr, 0, 0) != 0) {
                return errno;
        }

        for (int i = 0; i < n; i++) {
                if (getline(&line, &len, fptr) == -1) {
                        rc = -1;
                        break;
                }
                if (found_record != NULL) {
                        if (!strlen(line))
                            continue;
                        deserialize_journal_entry(line, &entry);
                        if (entry) {
                                found_record(entry->record_id);
                                free_journal_entry(entry);
                                entry = NULL;
                        }
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
 * @returns 0 on success, errno on failure
 */
static int copy_to_tmp(FILE *fptr, char *tmp_path)
{
        int rc = 0;
        size_t len;
        char *line = NULL;
        FILE *fptr_tmp = NULL;

        if (fptr == NULL) {
                return -1;
        }

        fptr_tmp = fopen(tmp_path, "w");
        if (!fptr_tmp) {
                return errno;
        }

        // Copy lines to new file
        while (getline(&line, &len, fptr) > -1) {
                if (fprintf(fptr_tmp, "%s", line) < 0) {
                        rc = errno;
                        break;
                }
        }

        if (fclose(fptr_tmp) != 0) {
                rc = errno;
                telem_perror("Error");
        }
        free(line);

        return rc;
}

/* Exported function */
TelemJournal *open_journal(const char *journal_file)
{
        FILE *fptr = NULL;
        char boot_id[BOOTID_LEN] = { '\0' };
        struct TelemJournal *telem_journal;

        // Use default location if journal_file parameter is NULL
        if (journal_file == NULL) {
                fptr = fopen(JOURNAL_PATH, "a+");
        } else {
                fptr = fopen(journal_file, "a+");
        }
        if (fptr == NULL) {
                telem_perror("Error while opening journal file");
                return NULL;
        }

        if (read_boot_id(boot_id) != 0) {
                telem_perror("Error while reading boot_id");
                fclose(fptr);
                return NULL;
        }

        telem_journal = malloc(sizeof(struct TelemJournal));
        if (!telem_journal) {
                telem_log(LOG_CRIT, "CRIT: Unable to allocate memory\n");
                fclose(fptr);
                return NULL;
        }

        telem_journal->fptr = fptr;
        telem_journal->journal_file = \
                (journal_file == NULL) ? strdup(JOURNAL_PATH) : strdup(journal_file);
        /* boot_id includes \n at the end, strip RC during duplication */
        telem_journal->boot_id = strndup(boot_id, BOOTID_LEN - 1);
        telem_journal->record_count = get_record_count(telem_journal);
        telem_journal->record_count_limit = RECORD_LIMIT;
        telem_journal->latest_record_id = NULL;
        telem_journal->prune_entry_callback = NULL;

        telem_debug("Records in db: %d\n", telem_journal->record_count);

        return telem_journal;
}

/* Exported function */
void close_journal(TelemJournal *telem_journal)
{
        if (telem_journal) {
                free(telem_journal->boot_id);
                free(telem_journal->journal_file);
                free(telem_journal->latest_record_id);
                fclose(telem_journal->fptr);
                free(telem_journal);
        }
}

/**
 * Checks if class is a prefix i.e. t/\* or t/t/\*
 *
 * @param class A pointer to string with classification
 *              value to check.
 * @return 1 if class is a prefix, 0 otherwise
 */
static int is_class_prefix(char *class)
{
        size_t class_len = strlen(class);
        return (strcmp((char *)(class + class_len - 2), "/*") == 0) ? 1 : 0;
}

/**
 * Print records content
 *
 * @param record_id Unique record identifier
 *
 */
static void print_record(char *record_id)
{
        int rc = 0;
        size_t read = 0;
        char buff[MID_BUFF] = { '\0' };
        char *filepath = NULL;
        FILE *recordfp = NULL;

        rc = asprintf(&filepath, "%s/%s", RECORD_RETENTION_DIR, record_id);
        if (rc == -1) {
                // just bail out, there are worse problems
                return;
        }

        recordfp = fopen(filepath, "r");
        if (!recordfp) {
                telem_log(LOG_INFO, "Could not open record %s: %s\n", record_id, strerror(errno));
                free(filepath);
                return;
        }

        while ((read = fread(buff, 1, sizeof(buff), recordfp)) > 0) {
                fwrite(buff, 1, read, stdout);
        }

        free(filepath);
        fclose(recordfp);
}

/* Exported function */
int print_journal(TelemJournal *telem_journal, char *classification,
                  char *record_id, char *event_id, char *boot_id,
                  bool include_record)
{
        int n = 0;
        int rc = 0;
        int count = 0;
        char str_time[80] = { '\0' };
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
                rc = skip_n_lines(n, journal_fileptr, NULL);
                if (rc == -1) {
                        return rc;
                } else if (rc != 0) {
                        telem_log(LOG_ERR, "An error occurred while advancing journal file: %s\n", strerror(rc));
                }
        }

        while (getline(&line, &len, journal_fileptr) != -1) {
                if (!strlen(line))
                    continue;
                deserialize_journal_entry(line, &entry);
                if (entry) {
                        /* filter entry out if one is provided */
                        if (record_id != NULL && strcmp(entry->record_id, record_id) != 0) {
                                goto skip_print;
                        }
                        if (boot_id != NULL && strcmp(entry->boot_id, boot_id) != 0) {
                                goto skip_print;
                        }
                        if (event_id != NULL && strcmp(entry->event_id, event_id) != 0) {
                                goto skip_print;
                        }
                        // In the case of class checking prefixes is an option
                        if (classification != NULL) {
                                // Check prefixes when classification ends in /*, otherwise use strcomp
                                if (is_class_prefix(classification)) {
                                        if (strncmp(entry->classification, classification, strlen(classification) - 1) != 0) {
                                                goto skip_print;
                                        }
                                } else if (strcmp(entry->classification, classification) != 0) {
                                        goto skip_print;
                                }
                        }
                        /* end filters section */
                        ts = *localtime(&entry->timestamp);
                        if (strftime(str_time, sizeof(str_time), "%a %Y-%m-%d %H:%M:%S %Z", &ts) == 0) {
                                goto skip_print;
                        }
                        /* print record metadata */
                        fprintf(stdout, "%-30s %s %s %s %s\n", entry->classification, str_time, entry->record_id, entry->event_id, entry->boot_id);
                        /* print record content */
                        if (include_record) {
                                print_record(entry->record_id);
                        }
                        count++;
skip_print:
                        free_journal_entry(entry);
                }
        }
        free(line);
        fclose(journal_fileptr);

        return count;
}

/**
 * Validation for event id. Thic check makes sure
 * that an event_id is a 32 hexadecimal chars long.
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
        char boot_id[BOOTID_LEN] = { '\0' };
        char *record_id = NULL;
        struct JournalEntry *entry = NULL;

        if (telem_journal == NULL) {
                telem_log(LOG_ERR, "telem_journal was not initialized\n");
                return rc;
        }

        if (validate_classification(classification) != 0) {
                return rc;
        }

        if (validate_event_id(event_id) != 0) {
                return rc;
        }

        entry = malloc(sizeof(struct JournalEntry));
        if (!entry) {
                telem_log(LOG_CRIT, "CRIT: unable to allocate memory\n");
                return rc;
        }
        entry->classification = NULL;
        entry->record_id = NULL;
        entry->event_id = NULL;
        entry->boot_id = NULL;

        if (get_random_id(&record_id) != 0) {
                telem_log(LOG_ERR, "Erorr: Unable to generate random id\n");
                goto quit;
        }

        if (read_boot_id(boot_id) != 0) {
                telem_log(LOG_ERR, "Error: Unable to read boot_id\n");
                goto quit;
        }

        if ((entry->classification = strdup(classification)) == NULL) {
                goto quit;
        }
        if ((entry->event_id = strdup(event_id)) == NULL) {
                goto quit;
        }

        entry->record_id = record_id;
        /* boot_id includes \n at the end, strip RC during duplication */
        entry->boot_id = strndup(boot_id, BOOTID_LEN - 1);
        entry->timestamp = timestamp;

        if ((rc = save_entry(telem_journal->fptr, entry)) == 0) {
                telem_journal->record_count = telem_journal->record_count + 1;
                telem_debug("DEBUG: %d records in journal\n", telem_journal->record_count);
        }

        free(telem_journal->latest_record_id);
        telem_journal->latest_record_id = strdup(entry->record_id);

quit:
        free_journal_entry(entry);

        return rc;
}

/* Exported function */
int prune_journal(struct TelemJournal *telem_journal, char *tmp_dir)
{

        int rc = 1;
        int count = 0;
        int deviation = DEVIATION;
        char *tmp_file_path = NULL;

        if (telem_journal == NULL) {
                return rc;
        }

        if (telem_journal->record_count > (deviation + telem_journal->record_count_limit)) {
                count = telem_journal->record_count - telem_journal->record_count_limit;

                // jump to line# count
                if ((rc = skip_n_lines(count, telem_journal->fptr, telem_journal->prune_entry_callback)) != 0) {
                        telem_log(LOG_ERR, "Error skipping %d journal lines\n", count);
                        // if rc == -1 (n > #lines in file, change to errno style)
                        return (rc == -1) ? EADDRNOTAVAIL : rc;
                }
                // Use default if not tmp_dir is specified
                if (tmp_dir == NULL) {
                        if ((asprintf(&tmp_file_path, "%s/.journal", JOURNAL_TMPDIR) == -1)) {
                                return ENOMEM;
                        }
                } else {
                        if ((asprintf(&tmp_file_path, "%s/.journal", tmp_dir) == -1)) {
                                return ENOMEM;
                        }
                }
                // create new file with rest of file
                if ((rc = copy_to_tmp(telem_journal->fptr, tmp_file_path)) != 0) {
                        telem_log(LOG_ERR, "Error copying partial journal to temp journal file\n");
                        goto quit;
                }
                // close file handler
                if ((rc = fclose(telem_journal->fptr)) != 0) {
                        telem_log(LOG_ERR, "Error closing journal file handler\n");
                        goto quit;
                }
                // overwrite file
                if ((rc = rename(tmp_file_path, telem_journal->journal_file)) != 0) {
                        telem_log(LOG_ERR, "Error while overwriting journal file\n");
                        goto quit;
                }
                // reopen file handler
                telem_journal->fptr = fopen(telem_journal->journal_file, "a+");
                if (!telem_journal->fptr) {
                        telem_log(LOG_ERR, "Error re-opening journal file\n");
                        rc = 1;
                        goto quit;
                }
                // update record count
                telem_journal->record_count = telem_journal->record_count - count;
                telem_debug("DEBUG: record_count: %d\n", telem_journal->record_count);
        }
        rc = 0;

quit:
        free(tmp_file_path);

        return rc;
}

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
