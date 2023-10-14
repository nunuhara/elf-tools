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
#include "nulib/string.h"

#include "cli.h"
#include "cg.h"

enum {
	LOPT_OUTPUT = 256,
};

static int cli_cg_convert(int argc, char *argv[])
{
	string output_file = NULL;

	while (1) {
		int c = command_getopt(argc, argv, &cmd_cg_convert);
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
		command_usage_error(&cmd_cg_convert, "Wrong number of arguments.\n");

	if (!output_file)
		output_file = file_replace_extension(argv[0], "png");

	// determine output CG type
	enum cg_type out_type = cg_type_from_name(output_file);
	if (out_type < 0)
		sys_error("Unable to determine CG type for output \"%s\".\n", output_file);

	// determine input CG type
	enum cg_type in_type = cg_type_from_name(argv[0]);
	if (in_type < 0)
		sys_error("Unable to determine CG type for input \"%s\".\n", argv[0]);

	// read input CG
	size_t data_size;
	uint8_t *data = file_read(argv[0], &data_size);
	if (!data)
		sys_error("Error reading input file \"%s\": %s", argv[0], strerror(errno));

	// decode input CG
	struct cg *cg = cg_load(data, data_size, in_type);
	free(data);
	if (!cg)
		sys_error("Failed to decode CG.\n");

	// open output file
	FILE *f = file_open_utf8(output_file, "wb");
	if (!f)
		sys_error("Failed to open output file \"%s\": %s.\n", output_file, strerror(errno));

	// write output CG
	if (!cg_write(cg, f, out_type))
		sys_error("Failed to write CG to output file.\n");

	cg_free(cg);
	fclose(f);
	string_free(output_file);
	return 0;
}

struct command cmd_cg_convert = {
	.name = "convert",
	.usage = "[options] <input-file>",
	.description = "Convert an image file to another format",
	.parent = &cmd_cg,
	.fun = cli_cg_convert,
	.options = {
		{ "output", 'o', "Set the output file path", required_argument, LOPT_OUTPUT },
		{ 0 }
	}
};
