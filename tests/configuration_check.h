/*
 * This program is part of the Clear Linux Project
 *
 * Copyright 2020 Intel Corporation
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

/* Function used only for unit testing */
/* Frees configuration structure */
void free_config_struct(struct configuration *config);

/* Frees configuration file name string */
void free_config_file(void);
