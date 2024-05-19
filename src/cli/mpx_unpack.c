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

#include "cli.h"

enum {
	LOPT_OUTPUT = 256,
};

static void mpx_print(struct port *out, struct buffer *in)
{
	uint16_t nr_cols = buffer_read_u16(in);
	uint16_t nr_rows = buffer_read_u16(in);

	for (unsigned row = 0; row < nr_rows; row++) {
		for (unsigned col = 0; col < nr_cols; col++) {
			if (buffer_remaining(in) < 5) {
				WARNING("MPX file truncated at (%u,%u)?", col, row);
				return;
			}
			uint16_t bg_tileno = buffer_read_u16(in);
			uint16_t fg_tileno = buffer_read_u16(in);
			uint8_t flags = buffer_read_u8(in);
			const char *prefix = col ? "|" : "";
			if (bg_tileno == 0xffff && fg_tileno == 0xffff)
				port_printf(out, "%s----,----,%2x ", prefix, flags);
			else if (bg_tileno == 0xffff)
				port_printf(out, "%s----,%4u,%2x ", prefix, fg_tileno, flags);
			else if (fg_tileno == 0xffff)
				port_printf(out, "%s%4u,----,%2x ", prefix, bg_tileno, flags);
			else
				port_printf(out, "%s%4u,%4u,%2x ", prefix, bg_tileno, fg_tileno, flags);
		}
		port_putc(out, '\n');
	}
	if (!buffer_end(in))
		WARNING("Junk at end of MPX file?");
}

static int cli_mpx_unpack(int argc, char *argv[])
{
	string output_file = NULL;

	while (1) {
		int c = command_getopt(argc, argv, &cmd_mpx_unpack);
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
		command_usage_error(&cmd_mpx_unpack, "Wrong number of arguments.\n");

	if (!output_file)
		output_file = string_new("out.smpx");

	size_t data_size;
	uint8_t *data = file_read(argv[0], &data_size);
	if (!data)
		CLI_ERROR("Error reading file \"%s\": %s\n", argv[0], strerror(errno));

	struct port out;
	if (!port_file_open(&out, output_file))
		CLI_ERROR("Failed to open output file \"%s\": %s",
				output_file, strerror(errno));

	struct buffer in;
	buffer_init(&in, data, data_size);
	mpx_print(&out, &in);

	port_close(&out);
	free(data);
	string_free(output_file);
	return 0;
}

struct command cmd_mpx_unpack = {
	.name = "unpack",
	.usage = "[options] <input-file>",
	.description = "Unpack a .mpx file",
	.parent = &cmd_mpx,
	.fun = cli_mpx_unpack,
	.options = {
		{ "output", 'o', "Set the output file path", required_argument, LOPT_OUTPUT },
		{ 0 }
	}
};
