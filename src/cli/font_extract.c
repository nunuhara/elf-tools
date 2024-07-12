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
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "nulib.h"
#include "nulib/file.h"
#include "nulib/port.h"
#include "nulib/string.h"
#include "ai5/cg.h"

#include "cli.h"

// FIXME: probably varies by game?
static unsigned char_w = 28;
static unsigned char_h = 28;

static void extract_char(uint8_t *out, uint8_t *fnt, uint8_t *msk)
{
	const unsigned stride = char_w * 4 * 16;

	for (int row = 0; row < char_h; row++) {
		uint8_t *fnt_src = fnt + char_w * row;
		uint8_t *msk_src = msk + char_w * row;
		uint8_t *dst = out + row * stride;
		for (int col = 0; col < char_w; col++, fnt_src++, msk_src++, dst += 4) {
			dst[0] = *fnt_src;
			dst[1] = *fnt_src;
			dst[2] = *fnt_src;
			dst[3] = min(*msk_src, 15) * 16;
		}
	}
}

static void extract_font(const char *output_path, uint8_t *fnt, size_t fnt_size,
		uint8_t *msk, size_t msk_size)
{
	const unsigned char_size = char_w * char_h;
	if (fnt_size % char_size != 0)
		sys_error("FNT size is not multiple of glyph size\n");
	if (msk_size % char_size != 0)
		sys_error("MSK size is not multiple of glyph size\n");
	if (fnt_size != msk_size)
		sys_error("FNT size and MSK size differ\n");

	FILE *f = file_open_utf8(output_path, "wb");
	if (!f)
		sys_error("file_open_utf8: %s\n", strerror(errno));

	const unsigned nr_chars = fnt_size / char_size;
	const unsigned nr_rows = ((nr_chars + 15) & ~15) / 16;

	struct cg *cg = cg_alloc();
	cg->metrics.w = char_w * 16;
	cg->metrics.h = char_h * nr_rows;
	cg->metrics.bpp = 32;
	cg->pixels = xcalloc(cg->metrics.w * cg->metrics.h, 4);

	for (unsigned row = 0; row < nr_rows; row++) {
		const unsigned nr_cols = row == nr_rows - 1 ? nr_chars % 16 : 16;
		for (unsigned col = 0; col < nr_cols; col++) {
			unsigned char_i = row * 16 + col;
			unsigned src_off = char_i * char_size;
			unsigned dst_off = (row * char_w * 16 * char_h + col * char_w) * 4;
			extract_char(cg->pixels + dst_off, fnt + src_off, msk + src_off);
		}
	}

	cg_write(cg, f, CG_TYPE_PNG);
	fclose(f);
	cg_free(cg);
}

enum {
	LOPT_OUTPUT = 256,
	LOPT_SIZE,
};

static int font_extract(int argc, char *argv[])
{
	const char *output_path = "out.png";
	while (1) {
		int c = command_getopt(argc, argv, &cmd_font_extract);
		if (c == -1)
			break;
		switch (c) {
		case 'o':
		case LOPT_OUTPUT:
			output_path = optarg;
			break;
		case 's':
		case LOPT_SIZE: {
			int size = atoi(optarg);
			if (size < 8 || size > 64)
				sys_error("Invalid character size: %s", optarg);
			char_w = size;
			char_h = size;
		}
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		command_usage_error(&cmd_font_extract, "Wrong number of arguments.\n");

	size_t fnt_size, msk_size;
	uint8_t *fnt, *msk;
	if (!(fnt = file_read(argv[0], &fnt_size)))
		sys_error("Failed to open font file \"%s\".\n", argv[0]);
	if (!(msk = file_read(argv[1], &msk_size)))
		sys_error("Failed to open mask file \"%s\".\n", argv[1]);

	extract_font(output_path, fnt, fnt_size, msk, msk_size);

	free(fnt);
	free(msk);
	return 0;
}

struct command cmd_font_extract = {
	.name = "extract",
	.usage = "[options...] <fnt-file> <msk-file>",
	.description = "Extract a font file",
	.parent = &cmd_font,
	.fun = font_extract,
	.options = {
		{ "output", 'o', "Set the output path", required_argument, LOPT_OUTPUT },
		{ "size", 's', "Set the character size", required_argument, LOPT_SIZE },
		{ 0 }
	}
};
