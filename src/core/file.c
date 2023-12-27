/* Copyright (C) 2023 Nunuhara Cabbage <nunuhara@haniwa.technology>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://gnu.org/licenses/>.
 */

#include <string.h>
#include <errno.h>

#include "nulib.h"
#include "nulib/file.h"
#include "ai5/anim.h"
#include "ai5/cg.h"

struct anim *file_anim_load(const char *path)
{
	// read file
	size_t data_size;
	uint8_t *data = file_read(path, &data_size);
	if (!data)
		sys_error("Error reading file \"%s\": %s\n", path, strerror(errno));

	// deserialize
	struct anim *anim = anim_parse(data, data_size);
	if (!anim)
		sys_error("Failed to parse animation file \"%s\"\n", path);

	free(data);
	return anim;
}

struct cg *file_cg_load(const char *path)
{
	// determine CG type
	enum cg_type type = cg_type_from_name(path);
	if (type < 0)
		sys_error("Unable to determine CG type for \"%s\"\n", path);

	// read file
	size_t data_size;
	uint8_t *data = file_read(path, &data_size);
	if (!data)
		sys_error("Error reading file \"%s\": %s\n", path, strerror(errno));

	// decode CG
	struct cg *cg = cg_load(data, data_size, type);
	if (!cg)
		sys_error("Failed to decode CG \"%s\"\n", path);

	free(data);
	return cg;
}


