/* Copyright (C) 2024 Nunuhara Cabbage <nunuhara@haniwa.technology>
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
#include "nulib/buffer.h"
#include "nulib/file.h"
#include "nulib/little_endian.h"
#include "nulib/port.h"
#include "nulib/string.h"
#include "ai5/cg.h"

#include "cli.h"
#include "map.h"

enum {
	LOPT_OUTPUT = 256,
};

static int cli_mp3_render(int argc, char *argv[])
{
	string output_file = NULL;

	while (1) {
		int c = command_getopt(argc, argv, &cmd_mp3_render);
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
		command_usage_error(&cmd_mp3_render, "Wrong number of arguments.\n");

	if (!output_file)
		output_file = string_new("out.png");

	size_t data_size;
	uint8_t *data = file_read(argv[0], &data_size);
	if (!data)
		CLI_ERROR("Error reading file \"%s\": %s", argv[0], strerror(errno));

	struct cg *cg = mp3_render(data, data_size);
	if (!cg)
		CLI_ERROR("Error rendering map");

	FILE *f = file_open_utf8(output_file, "wb");
	if (!f)
		CLI_ERROR("Error opening output file \"%s\": %s", output_file, strerror(errno));
	cg_write(cg, f, CG_TYPE_PNG);
	fclose(f);
	cg_free(cg);
	free(data);
	string_free(output_file);
	return 0;
}

struct command cmd_mp3_render = {
	.name = "render",
	.usage = "[options] <input-file>",
	.description = "Render a .mp3 file",
	.parent = &cmd_mp3,
	.fun = cli_mp3_render,
	.options = {
		{ "output", 'o', "Set the output file path", required_argument, LOPT_OUTPUT },
		{ 0 }
	}
};
