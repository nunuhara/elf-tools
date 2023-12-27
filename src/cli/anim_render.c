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
#include "ai5/anim.h"
#include "ai5/cg.h"
#include "ai5/game.h"

#include "anim.h"
#include "cli.h"
#include "file.h"

enum {
	LOPT_OUTPUT = 256,
	LOPT_GAME,
	LOPT_BG,
	LOPT_MAX_FRAMES,
};

#define DEFAULT_MAX_FRAMES 500

static int cli_anim_render(int argc, char *argv[])
{
	string output_file = NULL;
	string bg_file = NULL;
	int max_frames = DEFAULT_MAX_FRAMES;

	while (1) {
		int c = command_getopt(argc, argv, &cmd_anim_render);
		if (c == -1)
			break;

		switch (c) {
		case 'o':
		case LOPT_OUTPUT:
			output_file = string_new(optarg);
			break;
		case LOPT_GAME:
			ai5_set_game(optarg);
			break;
		case LOPT_BG:
			bg_file = string_new(optarg);
			break;
		case 'f':
		case LOPT_MAX_FRAMES:
			max_frames = atoi(optarg);
			if (max_frames < 1) {
				sys_warning("Invalid value for max frames: %s", optarg);
				max_frames = DEFAULT_MAX_FRAMES;
			}
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		command_usage_error(&cmd_anim_render, "Wrong number of arguments.\n");

	if (!output_file) {
		output_file = file_replace_extension(path_basename(argv[0]), "GIF");
	}

	// load animation and CGs
	struct anim *anim = file_anim_load(argv[0]);
	struct cg *cg = file_cg_load(argv[1]);
	struct cg *bg = bg_file ? file_cg_load(bg_file) : NULL;

	// render gif
	size_t gif_size;
	uint8_t *gif = anim_render_gif(anim, cg, bg, max_frames, &gif_size);
	if (gif && !file_write(output_file, gif, gif_size))
		sys_error("Failed to write output file \"%s\": %s", output_file,
				strerror(errno));

	string_free(output_file);
	string_free(bg_file);
	free(gif);
	anim_free(anim);
	cg_free(cg);
	cg_free(bg);
	return 0;
}

struct command cmd_anim_render = {
	.name = "render",
	.usage = "[options] <s4-file> <g8-file>",
	.description = "Render an animation file",
	.parent = &cmd_anim,
	.fun = cli_anim_render,
	.options = {
		{ "output", 'o', "Set the output file path", required_argument, LOPT_OUTPUT },
		{ "bg", 0, "Set the background CG", required_argument, LOPT_BG },
		{ "game", 0, "Specify the target game", required_argument, LOPT_GAME },
		{ "max-frames", 'f', "Set the maximum number of frames to render",
		  required_argument, LOPT_MAX_FRAMES },
		{ 0 }
	}
};
