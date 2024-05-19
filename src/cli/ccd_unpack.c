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
#include "ai5/ccd.h"

#include "cli.h"

enum {
	LOPT_OUTPUT = 256,
};

static int cli_ccd_unpack(int argc, char *argv[])
{
	string output_file = NULL;
	while (1) {
		int c = command_getopt(argc, argv, &cmd_ccd_unpack);
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
		command_usage_error(&cmd_ccd_unpack, "Wrong number of arguments.\n");

	size_t data_size;
	uint8_t *data = file_read(argv[0], &data_size);
	if (!data)
		CLI_ERROR("Error reading file \"%s\": %s", argv[0], strerror(errno));

	struct ccd *ccd = ccd_parse(data, data_size);
	if (!ccd)
		CLI_ERROR("Failed to parse CCD file: \"%s\"", argv[0]);

	struct port out;
	if (output_file) {
		if (!port_file_open(&out, output_file))
			CLI_ERROR("Failed to open output file \"%s\": %s",
					output_file, strerror(errno));
	} else {
		port_file_init(&out, stdout);
	}

	ccd_print(&out, ccd);

	port_close(&out);
	ccd_free(ccd);
	free(data);
	string_free(output_file);
	return 0;
}

struct command cmd_ccd_unpack = {
	.name = "unpack",
	.usage = "[options] <input-file>",
	.description = "Unpack a .ccd file",
	.parent = &cmd_ccd,
	.fun = cli_ccd_unpack,
	.options = {
		{ "output", 'o', "Set the output file path", required_argument, LOPT_OUTPUT },
		{ 0 }
	}
};
