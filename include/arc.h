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

#ifndef ELF_TOOLS_ARC_H
#define ELF_TOOLS_ARC_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "nulib.h"
#include "nulib/hashtable.h"
#include "nulib/string.h"
#include "nulib/vector.h"

declare_hashtable_string_type(arcindex, int);

enum {
	ARCHIVE_MMAP = 1
};

struct archive {
	hashtable_t(arcindex) index;
	vector_t(struct archive_data) files;
	bool mapped;
	union {
		FILE *fp;
		struct {
			uint8_t *data;
			size_t size;
		} map;
	};
};

struct archive_data {
	uint32_t offset;
	uint32_t raw_size; // size of file in archive
	uint32_t size;     // size of data in `data` (uncompressed)
	string name;
	uint8_t *data;
	unsigned int ref : 16;      // reference count
	unsigned int mapped : 1;    // true if `data` is a pointer into mmapped region
	unsigned int reserved : 15; // reserved for future flags
	struct archive *archive;
};

/*
 * Close an archive.
 */
void archive_close(struct archive *arc)
	attr_nonnull;

/*
 * Open an archive.
 */
struct archive *archive_open(const char *path, unsigned flags)
	attr_dealloc(archive_close, 1)
	attr_nonnull;

/*
 * Release a reference to an entry. If the reference count becomes zero, the
 * loaded data is free'd.
 */
void archive_data_release(struct archive_data *data)
	attr_nonnull;

/*
 * Get an entry by name. The entry data is loaded and the caller owns a
 * reference to the entry when this function returns.
 */
struct archive_data *archive_get(struct archive *arc, const char *name)
	attr_nonnull;


struct archive_data *archive_get_by_index(struct archive *arc, unsigned i)
	attr_nonnull;

/*
 * Load data for an entry (if it is not already loaded). The caller owns a
 * reference to the entry when this function returns.
 *
 * This function is useful when iterating over the file list with
 * `archive_foreach`.
 */
bool archive_data_load(struct archive_data *data)
	attr_warn_unused_result
	attr_nonnull;

/*
 * Iterate over the list of files in an archive. The caller does NOT own a
 * reference to the entries it iterates over, and data is NOT loaded.
 *
 * The caller must use `archive_data_load` to create a reference and load
 * the file data.
 */
#define archive_foreach(var, arc) vector_foreach_p(var, (arc)->files)

#endif
