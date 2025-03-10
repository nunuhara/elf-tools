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
#include "ai5/lzss.h"

#include "cli.h"

enum {
	LOPT_OUTPUT = 256,
	LOPT_BITWISE,
};

static int cli_lzss_decompress(int argc, char *argv[])
{
	char *output_file = NULL;
	bool bitwise = false;

	while (1) {
		int c = command_getopt(argc, argv, &cmd_lzss_decompress);
		if (c == -1)
			break;

		switch (c) {
		case 'o':
		case LOPT_OUTPUT:
			output_file = optarg;
			break;
		case LOPT_BITWISE:
			bitwise = true;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		command_usage_error(&cmd_lzss_decompress, "Wrong number of arguments.\n");

	size_t data_size;
	uint8_t *data = file_read(argv[0], &data_size);
	if (!data)
		sys_error("Error reading input file \"%s\": %s", argv[0], strerror(errno));

	size_t out_size;
	uint8_t *out_data;
	if (bitwise)
		out_data = lzss_bw_decompress(data, data_size, &out_size);
	else
		out_data = lzss_decompress(data, data_size, &out_size);
	free(data);

	if (!file_write(output_file ? output_file : "out.dat", out_data, out_size))
		sys_error("Error writing output file \"%s\": %s",
				output_file ? output_file : "out.dat", strerror(errno));

	free(out_data);
	return 0;
}

struct command cmd_lzss_decompress = {
	.name = "decompress",
	.usage = "[options] <input-file>",
	.description = "Deompress a file",
	.parent = &cmd_lzss,
	.fun = cli_lzss_decompress,
	.options = {
		{ "output", 'o', "Set the output file path", required_argument, LOPT_OUTPUT },
		{ "bitwise", 0, "Use \"bitwise\" LZSS decoder", no_argument, LOPT_BITWISE },
		{ 0 }
	}
};
