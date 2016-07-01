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

/** @file telemetry.h
 * Public API header file for telemetrics-client
 *
 * @mainpage
 *
 * @section overview Overview
 *
 * This package provides the front end component of a complete telemetrics
 * solution for Linux-based operating systems. Specifically, the front end
 * component includes:
 *
 * - telemetrics probes that collect specific types of data from the operating
 *   system.
 *
 * - a library, libtelemetry, that telemetrics probes use to create telemetrics
 *   records and send them to the daemon for further processing.
 *
 * - a daemon, telemd, that prepares the records to send to a telemetrics server
 *   (not included in this source tree), or spools the records on disk in case
 *   it's unable to successfully deliver them.
 *
 * @section public_api Public API
 *
 * API documentation is included in @link telemetry.h @endlink.
 *
 * @copyright Copyright 2015 Intel Corporation, under terms of the GNU Lesser
 * General Public License 2.1, or (at your option) any later version.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

struct telem_ref {
        struct telem_record *record;
};

/**
 * Set the configuration file name to use
 *
 * @param c_file Absolute file name of the configuration file. If the path name
 * provided is not an absolute path, or the path given cannot be successfuly
 * stat'd, then the effective configuration file is left unchanged.
 */
void tm_set_config_file(char *c_file);

/**
 * Create a new telemetrics record
 *
 * @param t_ref A pointer to a telem_ref struct pointer declared by the caller.
 *     The struct is initialized if the function returns success.
 * @param severity Severity field value. Accepted values are in the range 1-4,
 *     with 1 being the lowest severity, and 4 being the highest severity.
 *     Values provided outside of this range are clamped to 1 or 4.
 * @param classification Classification field value. It should have the form
 *     DOMAIN/PROBENAME/REST: DOMAIN is the reverse domain to use as a namespace
 *     for the probe (e.g. org.clearlinux); PROBENAME is the name of the probe;
 *     and REST is an arbitrary value that the probe should use to classify the
 *     record. The maximum length for the classification string is 122 bytes.
 *     Each sub-category may be no longer than 40 bytes long. Two / delimiters
 *     are required.
 * @param payload_version Payload format version. The only supported value right
 *     now is 1, which indicates that the payload is a freely-formatted
 *     (unstructured) string. Values greater than 1 are reserved for future use.
 *
 * @return 0 on success, or a negative errno-style value on error
 */
int tm_create_record(struct telem_ref **t_ref, uint32_t severity,
                     char *classification, uint32_t payload_version);

/**
 * Set the payload field of a telemetrics record
 *
 * @param t_ref The handle returned by tm_create_record()
 * @param payload The payload to set
 *
 * @return 0 on success, or a negative errno-style value on error
 */
int tm_set_payload(struct telem_ref *t_ref, char *payload);

/**
 * Send a record to the telemetrics daemon for delivery
 *
 * @param t_ref The handle returned by tm_create_record()
 *
 * @return 0 on success, or a negative errno-style value on error
 */
int tm_send_record(struct telem_ref *t_ref);

/**
 * Release the memory allocated to a telemetrics record.
 *
 * @param t_ref A handle created by a call to
 *     tm_create_record(). It is the caller's responsibility
 *     to not reuse this pointer without first
 *     resetting it with another call to tm_create_record()
 *
 */
void tm_free_record(struct telem_ref *t_ref);

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
