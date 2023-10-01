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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "nulib.h"
#include "nulib/file.h"
#include "nulib/port.h"
#include "nulib/string.h"

#include "cli.h"
#include "arc.h"
#include "mes.h"

static bool extract_file(struct archive_data *data, const char *output_file, bool raw)
{
	struct port out;
	if (output_file) {
		if (!port_file_open(&out, output_file)) {
			WARNING("port_file_open: %s", strerror(errno));
			return false;
		}
	} else {
		port_file_init(&out, stdout);
	}

	bool r = true;
	const char *ext = file_extension(data->name);
	if (!raw && !strcasecmp(ext, "mes")) {
		mes_ast_block toplevel = vector_initializer;
		if (!(mes_decompile(data->data, data->size, &toplevel))) {
			sys_warning("Failed to decompile .mes file \"%s\".\n", data->name);
		} else {
			mes_ast_block_print(toplevel, &out);
			mes_ast_block_free(toplevel);
		}
	} else {
		if (!port_write_bytes(&out, data->data, data->size)) {
			WARNING("port_write_bytes: %s", strerror(errno));
			r = false;
		}
	}

	port_close(&out);
	return r;
}

static void extract_one(struct archive *arc, const char *name, const char *output_file, bool raw)
{
	struct archive_data *data = archive_get(arc, name);
	if (!data)
		sys_error("Failed to read file \"%s\" from archive.\n", name);
	if (!extract_file(data, output_file, raw))
		sys_error("failed to write output file");

	archive_data_release(data);
}

// Add a trailing slash to a path
static char *output_dir_path(const char *path)
{
	if (!path || *path == '\0')
		return xstrdup("./"); // default is cwd

	size_t size = strlen(path);
	if (path[size-1] == '/')
		return xstrdup(path);

	char *s = xmalloc(size + 2);
	strcpy(s, path);
	s[size] = '/';
	s[size+1] = '\0';
	return s;
}

static void extract_all(struct archive *arc, const char *_output_dir, bool raw)
{
	char *output_dir = output_dir_path(_output_dir);
	if (mkdir_p(output_dir) < 0) {
		sys_error("Failed to create output directory: %s.\n", strerror(errno));
	}

	struct archive_data *data;
	archive_foreach(data, arc) {
		if (!archive_data_load(data)) {
			WARNING("Failed to read file \"%s\" from archive", data->name);
			continue;
		}
		char output_file[PATH_MAX];
		snprintf(output_file, PATH_MAX, "%s%s", output_dir, data->name);
		sys_message("%s... ", output_file);
		if (extract_file(data, output_file, raw)) {
			sys_message("OK\n");
		} else {
			WARNING("failed to extract file \"%s\"", data->name);
		}
		archive_data_release(data);
	}

	free(output_dir);
}

enum {
	LOPT_OUTPUT = 256,
	LOPT_GAME,
	LOPT_NAME,
	LOPT_RAW,
	LOPT_KEY,
};

int arc_extract(int argc, char *argv[])
{
	char *output_file = NULL;
	char *name = NULL;
	bool raw = false;
	bool key = false;
	while (1) {
		int c = command_getopt(argc, argv, &cmd_arc_extract);
		if (c == -1)
			break;
		switch (c) {
		case 'o':
		case LOPT_OUTPUT:
			output_file = optarg;
			break;
		case 'g':
		case LOPT_GAME:
			set_game(optarg);
			break;
		case 'n':
		case LOPT_NAME:
			name = optarg;
			break;
		case LOPT_RAW:
			raw = true;
			break;
		case LOPT_KEY:
			key = true;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		command_usage_error(&cmd_arc_extract, "Wrong number of arguments.\n");

	struct archive *arc = archive_open(argv[0], 0);
	if (!arc)
		sys_error("Failed to open archive file \"%s\".\n", argv[0]);

	if (key) {
		NOTICE("%08x%08x%02x%02x", arc->meta.offset_key, arc->meta.size_key,
				arc->meta.name_key, arc->meta.name_length);
	} else if (name) {
		extract_one(arc, name, output_file, raw);
	} else {
		extract_all(arc, output_file, raw);
	}

	archive_close(arc);
	return 0;
}

struct command cmd_arc_extract = {
	.name = "extract",
	.usage = "[options...] <input-file>",
	.description = "Extract an archive file",
	.parent = &cmd_arc,
	.fun = arc_extract,
	.options = {
		{ "output", 'o', "Set the output path", required_argument, LOPT_OUTPUT },
		{ "game", 'g', "Set the target game", required_argument, LOPT_GAME },
		{ "name", 'n', "Specify the file to extract", required_argument, LOPT_NAME },
		{ "raw", 0, "Do not convert (keep original file type)", no_argument, LOPT_RAW },
		{ "key", 0, "Print the index encryption key (do not extract)", no_argument, LOPT_KEY },
		{ 0 }
	}
};
