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
#include <string.h>
#include <errno.h>

#include "nulib.h"
#include "nulib/file.h"
#include "nulib/port.h"
#include "nulib/string.h"
#include "ai5/s4.h"

#include "cli.h"
#include "file.h"

enum {
	LOPT_OUTPUT = 256,
};

static int cli_anim_decompile(int argc, char *argv[])
{
	string output_file = NULL;

	while (1) {
		int c = command_getopt(argc, argv, &cmd_anim_decompile);
		if (c == -1)
			break;

		switch (c) {
		case 'o':
		case LOPT_OUTPUT:
			output_file = string_new(optarg);
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		command_usage_error(&cmd_anim_decompile, "Wrong number of arguments.\n");

	if (!output_file)
		output_file = file_replace_extension(path_basename(argv[0]), "SS4");

	struct s4 *s4 = file_s4_load(argv[0]);

	// open output file
	struct port out;
	if (output_file) {
		if (!port_file_open(&out, output_file)) {
			sys_error("Failed to open output file \"%s\": %s\n",
					output_file, strerror(errno));
		}
	} else {
		port_file_init(&out, stdout);
	}

	s4_print(&out, s4);

	string_free(output_file);
	port_close(&out);
	s4_free(s4);
	return 0;
}

struct command cmd_anim_decompile = {
	.name = "decompile",
	.usage = "[options] <input-file>",
	.description = "Decompile an animation file",
	.parent = &cmd_anim,
	.fun = cli_anim_decompile,
	.options = {
		{ "output", 'o', "Set the output file path", required_argument, LOPT_OUTPUT },
		{ 0 }
	}
};
