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
	LOPT_BLOCKS,
	LOPT_TREE,
	LOPT_TEXT,
	LOPT_NAME,
};

enum decompile_mode {
	DECOMPILE_NORMAL,
	DECOMPILE_FLAT,
	DECOMPILE_BLOCKS,
	DECOMPILE_TREE,
	DECOMPILE_TEXT,
};

int cli_mes_decompile(int argc, char *argv[])
{
	char *output_file = NULL;
	enum decompile_mode mode = DECOMPILE_NORMAL;
	int name_function = -1;

	while (1) {
		int c = command_getopt(argc, argv, &cmd_mes_decompile);
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
			mode = DECOMPILE_FLAT;
			break;
		case LOPT_BLOCKS:
			mode = DECOMPILE_BLOCKS;
			break;
		case LOPT_TREE:
			mode = DECOMPILE_TREE;
			break;
		case 't':
		case LOPT_TEXT:
			mode = DECOMPILE_TEXT;
			break;
		case LOPT_NAME:
			name_function = atoi(optarg);
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1) {
		command_usage_error(&cmd_mes_decompile, "Wrong number of arguments.\n");
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

	// read mes file
	size_t mes_size;
	uint8_t *mes = file_read(argv[0], &mes_size);
	if (!mes) {
		sys_error("Failed to read file \"%s\".\n", argv[0]);
	}

	// parse/decompile mes file
	if (mode == DECOMPILE_FLAT) {
		mes_statement_list statements = vector_initializer;
		if (!mes_parse_statements(mes, mes_size, &statements))
			sys_error("Failed to parse .mes file \"%s\".\n", argv[0]);
		mes_flat_statement_list_print(statements, &out);
		mes_statement_list_free(statements);
	} else if (mode == DECOMPILE_BLOCKS) {
		mes_block_list toplevel = vector_initializer;
		if (!mes_decompile_debug(mes, mes_size, &toplevel))
			sys_error("Failed to decompile .mes file \"%s\".\n", argv[0]);
		mes_block_list_print(toplevel, &out);
		mes_block_list_free(toplevel);
	} else if (mode == DECOMPILE_TREE) {
		mes_block_list toplevel = vector_initializer;
		if (!mes_decompile_debug(mes, mes_size, &toplevel))
			sys_error("Failed to decompile .mes file \"%s\".\n", argv[0]);
		mes_block_tree_print(toplevel, &out);
		mes_block_list_free(toplevel);
	} else if (mode == DECOMPILE_TEXT) {
		mes_statement_list statements = vector_initializer;
		if (!mes_parse_statements(mes, mes_size, &statements))
			sys_error("Failed to parse .mes file \"%s\".\n", argv[0]);
		mes_text_print(statements, &out, name_function);
		mes_statement_list_free(statements);
	} else {
		mes_ast_block toplevel = vector_initializer;
		if (!mes_decompile(mes, mes_size, &toplevel))
			sys_error("Failed to decompile .mes file \"%s\".\n", argv[0]);
		mes_ast_block_print(toplevel, name_function, &out);
		mes_ast_block_free(toplevel);
	}

	free(mes);
	port_close(&out);
	return 0;
}

struct command cmd_mes_decompile = {
	.name = "decompile",
	.usage = "[options...] <input-file>",
	.description = "Decompile a .mes file",
	.parent = &cmd_mes,
	.fun = cli_mes_decompile,
	.options = {
		{ "output", 'o', "Set the output file path", required_argument, LOPT_OUTPUT },
		{ "game", 'g', "Set the target game", required_argument, LOPT_GAME },
		{ "text", 't', "Extract text only", no_argument, LOPT_TEXT },
		{ "flat", 0, "Decompile to a flat list of statements", no_argument, LOPT_FLAT },
		{ "blocks", 0, "Display (labelled) blocks", no_argument, LOPT_BLOCKS },
		{ "tree", 0, "Display block tree", no_argument, LOPT_TREE },
		{ "name-function", 0, "Specify the name function number", required_argument, LOPT_NAME },
		{ 0 }
	}
};
