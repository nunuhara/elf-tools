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

#include "nulib.h"
#include "ai5/arc.h"
#include "ai5/game.h"

#include "cli.h"

enum {
	LOPT_GAME = 256,
};

int arc_list(int argc, char *argv[])
{
	while (1) {
		int c = command_getopt(argc, argv, &cmd_arc_list);
		if (c == -1)
			break;
		switch (c) {
		case 'g':
		case LOPT_GAME:
			ai5_set_game(optarg);
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		command_usage_error(&cmd_arc_list, "Wrong number of arguments.\n");

	struct archive *arc = archive_open(argv[0], 0);
	if (!arc)
		sys_error("Failed to open archive file \"%s\".\n", argv[0]);

	for (unsigned i = 0; i < vector_length(arc->files); i++) {
		printf("%u: %s\n", i, vector_A(arc->files, i).name);
	}

	archive_close(arc);
	return 0;
}

struct command cmd_arc_list = {
	.name = "list",
	.usage = "[options...] <input-file>",
	.description = "List the contents of an archive file",
	.parent = &cmd_arc,
	.fun = arc_list,
	.options = {
		{ "game", 'g', "Set the target game", required_argument, LOPT_GAME },
		{ 0 }
	}
};
