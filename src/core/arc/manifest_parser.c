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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "nulib.h"
#include "nulib/file.h"
#include "nulib/string.h"
#include "nulib/vector.h"

#include "arc.h"

#pragma GCC diagnostic ignored "-Wunused-function"

extern int arc_mf_lex();
extern int arc_mf_lineno;
extern FILE *arc_mf_in;

int arc_mf_parse(void);

static struct arc_manifest *arc_mf_output;

void arc_mf_error(const char *s)
{
	sys_error("ERROR: At line %d: %s\n", arc_mf_lineno, s);
}

struct arc_manifest *arc_manifest_parse(const char *path)
{
	// open input file
	if (!strcmp(path, "-"))
		arc_mf_in = stdin;
	else
		arc_mf_in = file_open_utf8(path, "rb");
	if (!arc_mf_in)
		sys_error("Error opening input file: \"%s\": %s", path, strerror(errno));

	// parse
	arc_mf_parse();

	// close input file
	if (arc_mf_in != stdin) {
		fclose(arc_mf_in);
		arc_mf_in = stdin;
	}

	// return parsed manifest
	struct arc_manifest *r = arc_mf_output;
	arc_mf_output = NULL;
	return r;
}

static inline arc_string_list push_string(arc_string_list list, string str)
{
	vector_push(string, list, str);
	return list;
}

static inline arc_string_list make_string_list(string str)
{
	return push_string((arc_string_list)vector_initializer, str);
}

static inline arc_row_list push_row(arc_row_list rows, arc_string_list row)
{
	vector_push(arc_string_list, rows, row);
	return rows;
}

static inline arc_row_list make_row_list(arc_string_list row)
{
	return push_row((arc_row_list)vector_initializer, row);
}

static enum arc_manifest_type arc_parse_manifest_type(string str)
{
	if (!strcasecmp(str, "#ARCPACK"))
		return ARC_MF_ARCPACK;
	return ARC_MF_INVALID;
}

static void make_arcpack_manifest(struct arc_manifest *dst, arc_row_list rows,
		arc_string_list options)
{
	dst->type = ARC_MF_ARCPACK;

	// parse options
	string opt;
	vector_foreach(opt, options) {
		if (!strncmp(opt, "--mod-arc=", 10)) {
			dst->arcpack.input_arc = string_new(opt+10);
		} else {
			sys_error("Unrecognized ARCPACK options: %s\n", opt);
		}
		string_free(opt);
	}
	vector_destroy(options);

	// create file list
	arc_string_list *row;
	vector_foreach_p(row, rows) {
		if (vector_length(*row) != 1)
			sys_error("Wrong number of columns in ARCPACK manifest.\n");
		vector_push(string, dst->arcpack.input_files, vector_A(*row, 0));
		vector_destroy(*row);
	}
	vector_destroy(rows);
}

struct arc_manifest *arc_make_manifest(string magic, arc_string_list options,
		string output_path, arc_row_list rows)
{
	struct arc_manifest *mf = xcalloc(1, sizeof(struct arc_manifest));
	mf->output_path = output_path;

	switch (arc_parse_manifest_type(magic)) {
	case ARC_MF_ARCPACK:
		make_arcpack_manifest(mf, rows, options);
		break;
	case ARC_MF_INVALID:
	default:
		sys_error("Invalid manifest type: \"%s\".\n", magic);
	}

	string_free(magic);
	return mf;
}

static void arcpack_manifest_free(struct arc_arcpack_manifest *mf)
{
	string name;
	vector_foreach(name, mf->input_files) {
		string_free(name);
	}
	vector_destroy(mf->input_files);
	string_free(mf->input_arc);
}

void arc_manifest_free(struct arc_manifest *mf)
{
	switch (mf->type) {
	case ARC_MF_ARCPACK:
		arcpack_manifest_free(&mf->arcpack);
		break;
	case ARC_MF_INVALID:
	default:
		ERROR("Invalid manifest type");
	}
	string_free(mf->output_path);
	free(mf);
}
