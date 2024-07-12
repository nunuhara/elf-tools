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
#include "ai5/a6.h"
#include "ai5/cg.h"

#include "cli.h"

enum {
	LOPT_OUTPUT = 256,
	LOPT_IMAGE,
};

static void a6_dims(a6_array a, unsigned *w, unsigned *h)
{
	unsigned w_max = 0;
	unsigned h_max = 0;
	struct a6_entry *e;
	vector_foreach_p(e, a) {
		w_max = max(w_max, e->x_right + 1);
		h_max = max(h_max, e->y_bot + 1);
	}
	*w = w_max;
	*h = h_max;
}

static struct cg *a6_to_image(a6_array a)
{
	unsigned w, h;
	a6_dims(a, &w, &h);

	if (w == 0 || w > 1920)
		sys_error("Invalid width for image: %u\n", w);
	if (h == 0 || h > 1080)
		sys_error("Invalid height for image: %u\n", h);

	struct cg *cg = cg_alloc_indexed(w, h);
	cg->palette[4] = 255;
	cg->palette[5] = 255;
	cg->palette[6] = 255;

	struct a6_entry *e;
	vector_foreach_p(e, a) {
		if (e->x_left > e->x_right || e->y_top > e->y_bot)
			sys_error("Invalid rectangle: (%u,%u,%u,%u)", e->x_left, e->y_top,
					e->x_right, e->y_bot);
		unsigned x = e->x_left;
		unsigned y = e->y_top;
		unsigned w = (e->x_right - e->x_left) + 1;
		unsigned h = (e->y_bot - e->y_top) + 1;

		uint8_t *base = cg->pixels + y * cg->metrics.w + x;
		for (int row = 0; row < h; row++) {
			uint8_t *dst = base + row * cg->metrics.w;
			memset(dst, 1, w);
		}
	}

	return cg;
}

static int cli_a6_decompile(int argc, char *argv[])
{
	string output_file = NULL;
	bool image = false;

	while (1) {
		int c = command_getopt(argc, argv, &cmd_a6_decompile);
		if (c == -1)
			break;

		switch (c) {
		case 'o':
		case LOPT_OUTPUT:
			output_file = string_new(optarg);
			break;
		case LOPT_IMAGE:
			image = true;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		command_usage_error(&cmd_a6_decompile, "Wrong number of arguments.\n");

	size_t data_size;
	uint8_t *data = file_read(argv[0], &data_size);
	if (!data)
		sys_error("Error reading file \"%s\": %s\n", argv[0], strerror(errno));

	a6_array a6 = a6_parse(data, data_size);
	free(data);

	if (vector_length(a6) == 0)
		sys_error("A6 file is empty\n");

	if (image) {
		struct cg *cg = a6_to_image(a6);
		if (!cg)
			sys_error("Failed to encode image");
		if (!output_file)
			output_file = file_replace_extension(path_basename(argv[0]), "PNG");
		FILE *out = file_open_utf8(output_file, "wb");
		if (!out)
			sys_error("Error opening output file \"%s\": %s\n", output_file,
					strerror(errno));
		if (!cg_write(cg, out, CG_TYPE_PNG))
			sys_error("Error writing output file \"%s\"", output_file);
		cg_free(cg);
		string_free(output_file);
		fclose(out);
		a6_free(a6);
		return 0;
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

	NOTICE("%d entries", (int)vector_length(a6));
	a6_print(&out, a6);

	string_free(output_file);
	port_close(&out);
	a6_free(a6);
	return 0;
}

struct command cmd_a6_decompile = {
	.name = "decompile",
	.usage = "[options] <input-file>",
	.description = "Decompile a .a6 file",
	.parent = &cmd_a6,
	.fun = cli_a6_decompile,
	.options = {
		{ "output", 'o', "Set the output file path", required_argument, LOPT_OUTPUT },
		{ "image", 0, "Output a color-coded image", no_argument, LOPT_IMAGE },
		{ 0 }
	}
};
