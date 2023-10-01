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
	ARCHIVE_MMAP = 1, // map archive in memory
	ARCHIVE_RAW  = 2, // skip decompression when loading files
};

struct arc_metadata {
	off_t arc_size;
	uint32_t nr_files;
	unsigned name_length;
	uint32_t offset_key;
	uint32_t size_key;
	uint8_t name_key;
};

struct archive {
	hashtable_t(arcindex) index;
	vector_t(struct archive_data) files;
	struct arc_metadata meta;
	unsigned flags;
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
 * Get the index of an entry by name.
 */
int archive_get_index(struct archive *arc, const char *name)
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

enum arc_manifest_type {
	ARC_MF_INVALID,
	ARC_MF_ARCPACK,
};

struct arc_arcpack_manifest {
	string input_arc;
	vector_t(string) input_files;
};

struct arc_manifest {
	enum arc_manifest_type type;
	string output_path;
	union {
		struct arc_arcpack_manifest arcpack;
	};
};

struct arc_manifest *arc_manifest_parse(const char *path);
void arc_manifest_free(struct arc_manifest *mf);

#endif
