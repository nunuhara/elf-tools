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
#include "nulib/string.h"
#include "ai5/game.h"

#include "cli.h"
#include "mes.h"

enum {
	LOPT_OUTPUT = 256,
	LOPT_GAME,
	LOPT_FLAT,
	LOPT_TEXT,
	LOPT_BASE,
};

enum compile_mode {
	MODE_NORMAL,
	MODE_FLAT,
	MODE_TEXT,
};

int cli_mes_compile(int argc, char *argv[])
{
	char *input_mes = NULL;
	char *output_file = NULL;
	enum compile_mode mode = MODE_NORMAL;

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
			ai5_set_game(optarg);
			break;
		case LOPT_FLAT:
			mode = MODE_FLAT;
			break;
		case 't':
		case LOPT_TEXT:
			mode = MODE_TEXT;
		case LOPT_BASE:
			input_mes = optarg;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1) {
		command_usage_error(&cmd_mes_compile, "Wrong number of arguments.\n");
	}

	if (mode == MODE_NORMAL) {
		sys_error("Structured mes compilation not yet implemented.\n");
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

	mes_statement_list mes = vector_initializer;

	if (mode == MODE_FLAT) {
		mes = mes_flat_parse(argv[0]);
	} else if (mode == MODE_TEXT) {
		// read and parse input .MES file
		string mes_path = input_mes
			? string_new(input_mes)
			: file_replace_extension(argv[0], "MES.IN");
		size_t data_size;
		uint8_t *data = file_read(mes_path, &data_size);
		if (!data)
			sys_error("Reading input .MES file \"%s\": %s", mes_path, strerror(errno));
		if (!mes_parse_statements(data, data_size, &mes))
			sys_error("Parsing input .MES file \"%s\"", mes_path);
		string_free(mes_path);
		free(data);

		// read and parse input .TXT file
		FILE *text_file = file_open_utf8(argv[0], "rb");
		if (!text_file)
			sys_error("Opening input file \"%s\": %s", argv[0], strerror(errno));
		mes_text_sub_list subs;
		if (!mes_text_parse(text_file, &subs)) {
			sys_error("Parsing input file \"%s\"", argv[0]);
		}
		// substitute text into input .MES file
		mes = mes_substitute_text(mes, subs);
		mes_text_sub_list_free(subs);
	}

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
		{ "text", 't', "Replace text in a mes file", no_argument, LOPT_TEXT },
		{ "base", 0, "Base mes file (for text mode)", required_argument, LOPT_BASE },
		{ "flat", 0, "Compile a flat list of statements", no_argument, LOPT_FLAT },
		{ 0 }
	}
};
