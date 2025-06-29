/* Copyright (C) 2025 Nunuhara Cabbage <nunuhara@haniwa.technology>
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
#include "nulib/little_endian.h"
#include "nulib/port.h"
#include "nulib/string.h"
#include "ai5/cg.h"
#include "ai5/game.h"

#include "cli.h"
#include "file.h"
#include "mdd.h"

enum {
	LOPT_OUTPUT = 256,
};

static int cli_mdd_render(int argc, char *argv[])
{
	string output_file = NULL;
	while (1) {
		int c = command_getopt(argc, argv, &cmd_mdd_render);
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
		command_usage_error(&cmd_mdd_render, "Wrong number of arguments.\n");

	if (!output_file)
		output_file = file_replace_extension(path_basename(argv[0]), "GIF");

	size_t size;
	uint8_t *data = file_read(argv[0], &size);
	uint8_t *gif = mdd_render(data, size, &size);
	free(data);

	if (!file_write(output_file, gif, size))
		sys_error("Failed to write output file \"%s\": %s", output_file, strerror(errno));

	string_free(output_file);
	free(gif);
	return 0;
}

struct command cmd_mdd_render = {
	.name = "render",
	.usage = "[options] <mdd-file>",
	.description = "Render a .mdd animation",
	.parent = &cmd_mdd,
	.fun = cli_mdd_render,
	.options = {
		{ "output", 'o', "Set the output file path", required_argument, LOPT_OUTPUT },
		{ 0 }
	}
};
