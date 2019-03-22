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

#include <stdbool.h>

/**
 * Save a telemetry record to disk
 *
 * @param path pointer to a directory to save record
 * @param headers pointer to array of headers and values
 * @param body record message content
 *
 */
void stage_record(char *path, char *headers[], char *body, char *cfg);

/**
 * Reads a telemetry record
 *
 * @param fullpath pointer to full path file name
 * @param headers pointer to array of headers and values
 * @param body record message content
 *
 * @return true if successful otherwise false
 */
bool read_record(char *fullpath, char *headers[], char **body, char **cfg);
