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

enum {
	LOPT_OUTPUT = 256,
};

/*
 * XXX: KABE[n].DAT is a bunch of variously-sized 4-bit indexed bitmaps
 *      concatenated together. The sizes and offsets are baked into the
 *      engine.
 */
static struct kabe_entry {
	unsigned w;      // width of CG
	unsigned h;      // height of CG
	unsigned offset; // offset into KABE[n].DAT
} kabe_entry[] = {
	// KABE1.DAT, KABE2.DAT
	[0]  = { 88,  480, 0x0 },
	[1]  = { 84,  440, 0x5280 },
	[2]  = { 60,  312, 0x9ab0 },
	[3]  = { 88,  432, 0xbf40 },
	[4]  = { 144, 432, 0x10980 },
	[5]  = { 84,  312, 0x18300 },
	[6]  = { 48,  312, 0x1b630 },
	[7]  = { 20,  312, 0x1d370 },
	[8]  = { 28,  480, 0x1dfa0 },
	[9]  = { 112, 480, 0x1f9e0 },
	[10] = { 92,  368, 0x262e0 },
	[11] = { 28,  480, 0x2a500 },
	[12] = { 264, 480, 0x2bf40 },
	[13] = { 112, 368, 0x3b6c0 },
	[14] = { 28,  368, 0x40740 },
	[15] = { 40,  368, 0x41b60 },
	[16] = { 92,  208, 0x43820 },
	[17] = { 116, 480, 0x45d80 },
	[18] = { 84,  412, 0x4ca40 },
	[19] = { 32,  268, 0x50dd8 },
	[20] = { 80,  412, 0x51e98 },
	[21] = { 40,  412, 0x55ef8 },
	[22] = { 84,  412, 0x57f28 },
	[23] = { 84,  260, 0x5c2c0 },
	[24] = { 36,  260, 0x5ed68 },
	[25] = { 412, 480, 0x5ffb0 },
	[26] = { 228, 408, 0x781f0 },
	[27] = { 92,  296, 0x837a0 },
	[28] = { 144, 420, 0x86cd0 },
	[29] = { 228, 480, 0x8e2f0 },
	[30] = { 268, 288, 0x9b8b0 },
	[31] = { 156, 256, 0xa4f70 },
	[32] = { 44,  232, 0xa9d70 },
	[33] = { 112, 224, 0xab160 },
	[34] = { 36,  224, 0xae260 },
	[35] = { 48,  240, 0xaf220 },
	[36] = { 180, 288, 0xb08a0 },
	[37] = { 320, 104, 0xb6de0 },
	[38] = { 320, 168, 0xbaee0 },
	[39] = { 320, 104, 0xc17e0 },
	[40] = { 320, 168, 0xc58e0 },
	[41] = { 320, 104, 0xcc1e0 },
	[42] = { 320, 168, 0xd02e0 },
	[43] = { 640, 104, 0xd6be0 },
	[44] = { 640, 168, 0xdede0 },
	// KABE3.DAT
	[45] = { 88,  480, 0x0 },
	[46] = { 84,  440, 0x5280 },
	[47] = { 60,  312, 0x9ab0 },
	[48] = { 88,  432, 0xbf40 },
	[49] = { 144, 432, 0x10980 },
	[50] = { 84,  312, 0x18300 },
	[51] = { 48,  312, 0x1b630 },
	[52] = { 20,  312, 0x1d370 },
	[53] = { 28,  480, 0x1dfa0 },
	[54] = { 112, 480, 0x1f9e0 },
	[55] = { 92,  368, 0x262e0 },
	[56] = { 28,  480, 0x2a500 },
	[57] = { 264, 480, 0x2bf40 },
	[58] = { 112, 368, 0x3b6c0 },
	[59] = { 28,  368, 0x40740 },
	[60] = { 40,  368, 0x41b60 },
	[61] = { 92,  208, 0x43820 },
	[62] = { 116, 480, 0x45d80 },
	[63] = { 84,  412, 0x4ca40 },
	[64] = { 32,  268, 0x50dd8 },
	[65] = { 80,  412, 0x51e98 },
	[66] = { 40,  412, 0x55ef8 },
	[67] = { 84,  412, 0x57f28 },
	[68] = { 84,  260, 0x5c2c0 },
	[69] = { 36,  260, 0x5ed68 },
	[70] = { 412, 480, 0x5ffb0 },
	[71] = { 228, 408, 0x781f0 },
	[72] = { 92,  296, 0x837a0 },
	[73] = { 144, 420, 0x86cd0 },
	[74] = { 228, 480, 0x8e2f0 },
	[75] = { 268, 288, 0x9b8b0 },
	[76] = { 156, 256, 0xa4f70 },
	[77] = { 44,  232, 0xa9d70 },
	[78] = { 112, 224, 0xab160 },
	[79] = { 36,  224, 0xae260 },
	[80] = { 48,  240, 0xaf220 },
	[81] = { 180, 288, 0xb08a0 },
};

static void bgr555_to_rgba(uint8_t *dst, uint16_t c)
{
	dst[0] = (c & 0x7c00) >> 7;
	dst[1] = (c & 0x03e0) >> 2;
	dst[2] = (c & 0x001f) << 3;
	dst[3] = 0xff;
}

static struct cg *kabe_entry_to_cg(int i, uint8_t *kabe, uint16_t *pal)
{
	struct kabe_entry *e = &kabe_entry[i];
	struct cg *cg = cg_alloc_direct(e->w, e->h);


	uint8_t *src = kabe + e->offset;
	for (unsigned row = 0; row < e->h; row++) {
		uint8_t *dst = cg->pixels + row * (e->w * 4);
		for (unsigned col = 0; col < e->w / 2; col++, src++, dst += 8) {
			bgr555_to_rgba(dst+0, pal[*src >> 4]);
			bgr555_to_rgba(dst+4, pal[*src & 0xf]);
		}
	}

	return cg;
}

static void extract_kabe_entry(int i, uint8_t *kabe, size_t kabe_size, uint16_t *pal,
		const char *output_dir)
{
	unsigned data_size = (kabe_entry[i].w * kabe_entry[i].h) / 2;
	if (kabe_entry[i].offset + data_size > kabe_size) {
		NOTICE("Skipping CG %d: invalid offset/size", i);
		return;
	}
	struct cg *cg = kabe_entry_to_cg(i, kabe, pal);
	if (!cg)
		ERROR("Failed to decode CG %d", i);

	char name[64];
	sprintf(name, "KABE%02d.PNG", i);
	char *path = path_join(output_dir, name);

	FILE *f = file_open_utf8(path, "wb");
	if (!f)
		ERROR("Failed to open output file: %s", path);

	cg_write(cg, f, CG_TYPE_PNG);
	fclose(f);
	cg_free(cg);
	NOTICE("%s", path);
	free(path);
}

static void extract_kabe(const char *dat_name, const char *pal_name, const char *output_dir)
{
	size_t pal_size;
	uint16_t *pal = file_read(pal_name, &pal_size);
	if (!pal)
		ERROR("Failed to read %s: %s", pal_name, strerror(errno));

	size_t kabe_size;
	uint8_t *kabe = file_read(dat_name, &kabe_size);
	if (!kabe)
		ERROR("Failed to read %s: %s", dat_name, strerror(errno));

	for (int i = 0; i < ARRAY_SIZE(kabe_entry); i++) {
		extract_kabe_entry(i, kabe, kabe_size, pal, output_dir);
	}

	free(pal);
	free(kabe);
}

static int cli_mp3_extract(int argc, char *argv[])
{
	char *output_dir = NULL;

	while (1) {
		int c = command_getopt(argc, argv, &cmd_mp3_extract);
		if (c == -1)
			break;

		switch (c) {
		case 'o':
		case LOPT_OUTPUT:
			output_dir = optarg;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		command_usage_error(&cmd_mp3_extract, "Wrong number of arguments.\n");

	if (output_dir) {
		if (mkdir_p(output_dir) < 0)
			ERROR("Failed to create directory %s: %s", output_dir, strerror(errno));
	} else {
		output_dir = string_new("");
	}

	extract_kabe(argv[0], argv[1], output_dir);

	return 0;
}

struct command cmd_mp3_extract = {
	.name = "extract",
	.usage = "[options] <dat-file> <pal-file>",
	.description = "Extract one of Isaku's KABE[n].dat CG archives",
	.parent = &cmd_mp3,
	.fun = cli_mp3_extract,
	.options = {
		{ "output", 'o', "Set the output directory", required_argument, LOPT_OUTPUT },
		{ 0 },
	}
};
