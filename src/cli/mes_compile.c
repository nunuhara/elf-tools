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

#include "nulib.h"
#include "nulib/file.h"
#include "nulib/port.h"

#include "cli.h"
#include "mes.h"

enum {
	LOPT_OUTPUT = 256,
	LOPT_GAME,
	LOPT_FLAT,
};

int cli_mes_compile(int argc, char *argv[])
{
	char *output_file = NULL;
	bool flat = false;

	while (1) {
		int c = command_getopt(argc, argv, &cmd_mes_compile);
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
		case LOPT_FLAT:
			flat = true;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1) {
		command_usage_error(&cmd_mes_compile, "Wrong number of arguments.\n");
	}

	if (!flat) {
		sys_error("Only flat mode is currently supported\n");
	}

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

	mes_statement_list mes = mes_flat_parse(argv[0]);

	size_t size;
	uint8_t *packed = mes_pack(mes, &size);

	if (!file_write(output_file ? output_file : "out.mes", packed, size)) {
		WARNING("file_write failed");
	}

	free(packed);
	mes_statement_list_free(mes);

	port_close(&out);
	return 0;
}

struct command cmd_mes_compile = {
	.name = "compile",
	.usage = "[options] <input-file>",
	.description = "Compile a .mes file",
	.parent = &cmd_mes,
	.fun = cli_mes_compile,
	.options = {
		{ "output", 'o', "Set the output file path", required_argument, LOPT_OUTPUT },
		{ "game", 'g', "Set the target game", required_argument, LOPT_GAME },
		{ "flat", 0, "Compile a flat list of statements", no_argument, LOPT_FLAT },
		{ 0 }
	}
};
