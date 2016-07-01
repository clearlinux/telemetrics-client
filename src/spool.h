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

#pragma once

/**
 * Run the spool record loop periodically
 */
void spool_records_loop(long *current_spool_size);

/**
 * Process the spooled record
 *
 * @param spool_dir Path of the spool directory
 * @param name File name of the spooled record
 * @param records_processed Number of records processed till now
 * @param records_sent Number of records sent to the backend
 */
void process_spooled_record(const char *spool_dir, char *name,
                            int *records_processed, int *records_sent,
                            long *current_spool_size);

/**
 * Send the spooled record to the backend
 *
 * @param record_path Path of the spooled record
 * @param post_succeeded bool indicating if the previous post was successful
 * @param sz Size of the file in bytes
 */
void transmit_spooled_record(char *record_path, bool *post_succeeded, long sz);

/**
 * Comparison function used for qsort
 *
 * @param entrya Pointer to the spool record file
 * @param entryb Pointer to the spool record file
 * @path path The path to the spool directory
 *
 * @return Returns the value of the comparison
 */
int spool_record_compare(const void *entrya, const void *entryb, void *path);

/**
 * Calculates the spool directory size.
 *
 * @return size of the spool directory on disk.
 *         may return a negative value in case of error.
 */
long get_spool_dir_size(void);

/**
 * Checks is the spool dir is valid and is writable
 */
bool is_spool_valid(void);

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
