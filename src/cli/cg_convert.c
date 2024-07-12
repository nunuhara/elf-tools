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
#include "nulib/little_endian.h"
#include "nulib/string.h"
#include "ai5/cg.h"

#include "cli.h"
#include "file.h"

enum {
	LOPT_OUTPUT = 256,
	LOPT_RAW_BMP,
	LOPT_BMP_WIDTH,
	LOPT_PALETTE,
	LOPT_MPX,
};

static uint8_t *load_palette(const char *pal_path)
{
	size_t pal_size;
	uint8_t *pal = file_read(pal_path, &pal_size);
	if (!pal)
		CLI_ERROR("Unable to read PAL file \"%s\": %s", pal_path, strerror(errno));

	if (pal_size != 256 * 2)
		CLI_ERROR("Unexpected PAL file size: %u", (unsigned)pal_size);

	// convert BGR555 palette to BGRx
	uint8_t *out = xcalloc(256, 4);
	for (unsigned i = 0; i < 256; i++) {
		uint16_t c = le_get16(pal, i * 2);
		out[i * 4 + 0] = (c & 0x001f) << 3;
		out[i * 4 + 1] = (c & 0x03e0) >> 2;
		out[i * 4 + 2] = (c & 0x7c00) >> 7;
		out[i * 4 + 3] = 0xff;
	}

	free(pal);
	return out;
}

static struct cg *load_raw_bitmap(const char *bmp_path, const char *pal_path, int w)
{
	// load bitmap
	size_t bmp_size;
	uint8_t *bmp = file_read(bmp_path, &bmp_size);
	if (!bmp)
		CLI_ERROR("Unable to read BMP file \"%s\": %s", bmp_path, strerror(errno));

	struct cg *cg = cg_alloc();

	// try to find palette if not specified
	if (!pal_path) {
		string tmp = file_replace_extension(bmp_path, "PAL");
		char *path = path_get_icase(tmp);
		if (!path)
			CLI_ERROR("Couldn't locate palette for raw bitmap");
		cg->palette = load_palette(path);
		free(path);
		string_free(tmp);
	} else {
		NOTICE("loading palette \"%s\"", pal_path);
		cg->palette = load_palette(pal_path);
	}

	// determine dimensions of bitmap
	if (w > 0) {
		if ((unsigned)w > bmp_size)
			CLI_ERROR("Width is smaller than bitmap size (%u)",
					(unsigned)bmp_size);
		if (bmp_size % w)
			CLI_WARNING("Bitmap size (%u) is not a multiple of the given width",
					(unsigned)bmp_size);
		cg->metrics.w = w;
		cg->metrics.h = bmp_size / w;
	} else if (bmp_size == 1280 * 960) {
		// maps
		cg->metrics.w = 1280;
		cg->metrics.h = 960;
	} else if (bmp_size == 640 * 480) {
		// allstars character sprite sheet
		cg->metrics.w = 640;
		cg->metrics.h = 480;
	} else if (bmp_size == 640 * 96) {
		// doukyuusei character sprite sheets
		cg->metrics.w = 640;
		cg->metrics.h = 96;
	} else {
		CLI_ERROR("Unexpected BMP file size: %u", (unsigned)bmp_size);
	}
	cg->metrics.bpp = 8;
	cg->metrics.has_alpha = false;

	// reverse y-direction of bitmap
	cg->pixels = xcalloc(1, bmp_size);
	for (unsigned row = 0; row < cg->metrics.h; row++) {
		uint8_t *dst = cg->pixels + row * cg->metrics.w;
		uint8_t *src = bmp + (cg->metrics.h - (row + 1)) * cg->metrics.w;
		memcpy(dst, src, cg->metrics.w);
	}

	free(bmp);
	return cg;
}

static struct cg *alloc_indexed_cg_with_palette(unsigned w, unsigned h, uint8_t *pal)
{
	struct cg *cg = cg_alloc_indexed(w, h);
	memcpy(cg->palette, pal, 256 * 4);
	return cg;
}

static void write_blank_tile(uint8_t *dst, unsigned stride)
{
	for (unsigned row = 0; row < 16; row++) {
		memset(dst + row * stride, 0, 16);
	}
}

static void copy_tile(struct cg *tileset, struct cg *dst, unsigned tile_x, unsigned tile_y,
		unsigned tileno)
{
	unsigned x = tile_x * 16;
	unsigned y = tile_y * 16;
	unsigned dst_off = y * dst->metrics.w + x;
	if (dst_off + 15 * dst->metrics.w + 16> dst->metrics.w * dst->metrics.h)
		CLI_ERROR("Invalid tile coordinate: %u,%u", tile_x, tile_y);

	if (tileno == 0xffff) {
		write_blank_tile(dst->pixels + dst_off, dst->metrics.w);
		return;
	}

	unsigned tileset_w = tileset->metrics.w / 16;
	unsigned tileset_x = (tileno % tileset_w) * 16;
	unsigned tileset_y = (tileno / tileset_w) * 16;
	unsigned tileset_off = tileset_y * tileset->metrics.w + tileset_x;
	if (tileset_off + 15 * tileset->metrics.w + 16 > tileset->metrics.w * tileset->metrics.h) {
		CLI_WARNING("WARNING: Invalid tileset index: %u", tileno);
		write_blank_tile(dst->pixels + dst_off, dst->metrics.w);
		return;
	}

	for (unsigned row = 0; row < 16; row++) {
		memcpy(dst->pixels + dst_off, tileset->pixels + tileset_off, 16);
		dst_off += dst->metrics.w;
		tileset_off += tileset->metrics.w;
	}
}

static void apply_mpx(const char *mpx_path, struct cg *bmp, struct cg **bg_out, struct cg **fg_out)
{
	size_t mpx_size;
	uint8_t *mpx = file_read(mpx_path, &mpx_size);
	if (!mpx)
		CLI_ERROR("Unable to read MPX file \"%s\": %s", mpx_path, strerror(errno));
	if (mpx_size < 5)
		CLI_ERROR("MPX file size too small: %u", (unsigned)mpx_size);

	unsigned nr_cols = le_get16(mpx, 0);
	unsigned nr_rows = le_get16(mpx, 2);
	if (mpx_size < 4 + nr_cols * nr_rows * 5)
		CLI_ERROR("MPX file size too small for given dimensions (%ux%u): %u",
				nr_cols, nr_rows, (unsigned)mpx_size);

	struct cg *bg = alloc_indexed_cg_with_palette(nr_cols * 16, nr_rows * 16, bmp->palette);
	struct cg *fg = alloc_indexed_cg_with_palette(nr_cols * 16, nr_rows * 16, bmp->palette);

	unsigned off = 4;
	for (unsigned row = 0; row < nr_rows; row++) {
		for (unsigned col = 0; col < nr_cols; col++, off += 5) {
			copy_tile(bmp, bg, col, row, le_get16(mpx, off + 0));
			copy_tile(bmp, fg, col, row, le_get16(mpx, off + 2));
		}
	}

	free(mpx);
	*bg_out = bg;
	*fg_out = fg;
}

static void write_image(const char *path, struct cg *cg)
{
	// determine output image format
	enum cg_type out_type = cg_type_from_name(path);
	if (out_type < 0)
		CLI_ERROR("Unable to determine CG type for output \"%s\".\n", path);

	// open output file
	FILE *f = file_open_utf8(path, "wb");
	if (!f)
		CLI_ERROR("Failed to open output file \"%s\": %s.\n", path, strerror(errno));

	// write output image
	if (!cg_write(cg, f, out_type))
		CLI_ERROR("Failed to write output image.\n");

	fclose(f);
}

static void write_image_with_extension(struct cg *cg, const char *path, const char *ext)
{
	string ext_path = file_replace_extension(path, ext);
	write_image(ext_path, cg);
	string_free(ext_path);
}

static int cli_cg_convert(int argc, char *argv[])
{
	string output_file = NULL;
	bool raw_bitmap = false;
	const char *pal_file = NULL;
	const char *mpx_file = NULL;
	int bmp_width = -1;

	while (1) {
		int c = command_getopt(argc, argv, &cmd_cg_convert);
		if (c == -1)
			break;

		switch (c) {
		case 'o':
		case LOPT_OUTPUT:
			output_file = string_new(optarg);
			break;
		case 'r':
		case LOPT_RAW_BMP:
			raw_bitmap = true;
			break;
		case LOPT_BMP_WIDTH:
			if ((bmp_width = atoi(optarg)) <= 0)
				CLI_ERROR("Invalid value for bitmap width: %s.\n", optarg);
			break;
		case 'p':
		case LOPT_PALETTE:
			pal_file = optarg;
			break;
		case LOPT_MPX:
			mpx_file = optarg;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		command_usage_error(&cmd_cg_convert, "Wrong number of arguments.\n");
	if (mpx_file && !raw_bitmap)
		command_usage_error(&cmd_cg_convert, "--mpx can only be used with --raw-bmp.\n");

	if (!output_file)
		output_file = file_replace_extension(argv[0], "png");

	struct cg *cg;
	if (raw_bitmap)
		cg = load_raw_bitmap(argv[0], pal_file, bmp_width);
	else
		cg = file_cg_load(argv[0]);

	if (mpx_file) {
		struct cg *bg, *fg;
		apply_mpx(mpx_file, cg, &bg, &fg);
		write_image_with_extension(bg, output_file, "bg.png");
		write_image_with_extension(fg, output_file, "fg.png");
		cg_free(bg);
		cg_free(fg);
	} else {
		write_image(output_file, cg);
	}

	cg_free(cg);
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
		{ "raw-bmp", 'r', "Interpret the source as a raw bitmap", no_argument, LOPT_RAW_BMP },
		{ "bmp-width", 0, "Specify the CG width (for raw bitmap)", required_argument, LOPT_BMP_WIDTH },
		{ "palette", 'p', "Specify a palette file (for raw bitmap)", required_argument, LOPT_PALETTE },
		{ "mpx", 0, "Specify a mpx tilemap (for raw bitmap)", required_argument, LOPT_MPX },
		{ 0 }
	}
};
