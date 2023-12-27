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
#include "ai5/anim.h"
#include "ai5/game.h"

#include "anim.h"
#include "cli.h"

enum {
	LOPT_OUTPUT = 256,
	LOPT_GAME,
};

static int cli_anim_compile(int argc, char *argv[])
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
		case LOPT_GAME:
			ai5_set_game(optarg);
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		command_usage_error(&cmd_anim_decompile, "Wrong number of arguments.\n");

	if (!output_file)
		output_file = file_replace_extension(path_basename(argv[0]), "S4");

	// parse input file
	struct anim *anim = anim_parse_script(argv[0]);
	if (!anim)
		sys_error("Failed to parse animation script file: %s\n", argv[0]);

	size_t size;
	uint8_t *data = anim_pack(anim, &size);
	if (!data)
		sys_error("Failed to serialize output file\n");

	if (!file_write(output_file, data, size))
		sys_error("Failed to write output file \"%s\": %s\n", output_file, strerror(errno));

	string_free(output_file);
	anim_free(anim);
	free(data);
	return 0;
}

struct command cmd_anim_compile = {
	.name = "compile",
	.usage = "[options] <input-file>",
	.description = "Compile an animation script file",
	.parent = &cmd_anim,
	.fun = cli_anim_compile,
	.options = {
		{ "output", 'o', "Set the output file path", required_argument, LOPT_OUTPUT },
		{ "game", 0, "Specify the target game", required_argument, LOPT_GAME },
		{ 0 }
	}
};
