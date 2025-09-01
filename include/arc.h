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
#include "ai5/arc.h"
#include "ai5/game.h"

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

enum archive_data_type {
	ARC_OTHER,
	ARC_MES,
	ARC_DATA,
	ARC_AUDIO,
};

struct arc_extract_options {
	bool raw;
	bool mes_text;
	bool mes_flat;
	int mes_name_fun;
};
#define ARC_EXTRACT_DEFAULT (struct arc_extract_options) { \
	.raw = false, \
	.mes_text = false, \
	.mes_flat = false, \
	.mes_name_fun = -1, \
}

enum archive_data_type arc_data_type(const char *path);
bool arc_is_compressed(const char *path, enum ai5_game_id game_id);
void arc_extract_one(struct archive *arc, const char *name, const char *output_file,
		struct arc_extract_options *opt);
void arc_extract_all(struct archive *arc, const char *_output_dir,
		struct arc_extract_options *opt);

#endif
