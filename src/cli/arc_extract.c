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
#include <limits.h>

#include "nulib.h"
#include "nulib/file.h"
#include "nulib/port.h"
#include "nulib/string.h"
#include "ai5/anim.h"
#include "ai5/arc.h"
#include "ai5/cg.h"
#include "ai5/game.h"

#include "arc.h"
#include "cli.h"
#include "mdd.h"
#include "mes.h"

enum {
	LOPT_OUTPUT = 256,
	LOPT_GAME,
	LOPT_NAME,
	LOPT_RAW,
	LOPT_DECOMPRESS,
	LOPT_NO_DECOMPRESS,
	LOPT_MES_FLAT,
	LOPT_MES_TEXT,
	LOPT_MES_NAME,
	LOPT_KEY,
	LOPT_STEREO,
};

int arc_extract(int argc, char *argv[])
{
	struct arc_extract_options opt = ARC_EXTRACT_DEFAULT;
	const char *output_file = NULL;
	const char *name = NULL;
	bool key = false;
	unsigned flags = 0;
	bool no_decompress = false;
	bool decompress = false;
	while (1) {
		int c = command_getopt(argc, argv, &cmd_arc_extract);
		if (c == -1)
			break;
		switch (c) {
		case 'o':
		case LOPT_OUTPUT:
			output_file = optarg;
			break;
		case 'g':
		case LOPT_GAME:
			ai5_set_game(optarg);
			break;
		case 'n':
		case LOPT_NAME:
			name = optarg;
			break;
		case LOPT_RAW:
			opt.raw = true;
			break;
		case LOPT_DECOMPRESS:
			decompress = true;
			break;
		case LOPT_NO_DECOMPRESS:
			no_decompress = true;
			break;
		case LOPT_MES_FLAT:
			opt.mes_flat = true;
			break;
		case LOPT_MES_TEXT:
			opt.mes_text = true;
			break;
		case LOPT_MES_NAME:
			opt.mes_name_fun = atoi(optarg);
			break;
		case LOPT_KEY:
			key = true;
			break;
		case LOPT_STEREO:
			flags |= ARCHIVE_STEREO;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		command_usage_error(&cmd_arc_extract, "Wrong number of arguments.\n");

	if (no_decompress || (!decompress && !arc_is_compressed(argv[0], ai5_target_game)))
		flags |= ARCHIVE_RAW;

	struct archive *arc = archive_open(argv[0], flags);
	if (!arc)
		sys_error("Failed to open archive file \"%s\".\n", argv[0]);

	if (key) {
		NOTICE("%08x%08x%02x%02x", arc->meta.offset_key, arc->meta.size_key,
				arc->meta.name_key, arc->meta.name_length);
	} else if (name) {
		arc_extract_one(arc, name, output_file, &opt);
	} else {
		arc_extract_all(arc, output_file, &opt);
	}

	archive_close(arc);
	return 0;
}

struct command cmd_arc_extract = {
	.name = "extract",
	.usage = "[options...] <input-file>",
	.description = "Extract an archive file",
	.parent = &cmd_arc,
	.fun = arc_extract,
	.options = {
		{ "output", 'o', "Set the output path", required_argument, LOPT_OUTPUT },
		{ "game", 'g', "Set the target game", required_argument, LOPT_GAME },
		{ "name", 'n', "Specify the file to extract", required_argument, LOPT_NAME },
		{ "raw", 0, "Do not convert (keep original file type)", no_argument, LOPT_RAW },
		{ "decompress", 0, "Decompress files", no_argument, LOPT_DECOMPRESS },
		{ "no-decompress", 0, "Do not decompress files", no_argument, LOPT_NO_DECOMPRESS },
		{ "mes-flat", 0, "Output flat mes files", no_argument, LOPT_MES_FLAT },
		{ "mes-text", 0, "Output text for mes files", no_argument, LOPT_MES_TEXT },
		{ "mes-name-function", 0, "Specify the name function number for mes files", no_argument, LOPT_MES_NAME },
		{ "key", 0, "Print the index encryption key (do not extract)", no_argument, LOPT_KEY },
		{ "stereo", 0, "Raw PCM data is stereo (AWD/AWF archives)", no_argument, LOPT_STEREO },
		{ 0 }
	}
};
