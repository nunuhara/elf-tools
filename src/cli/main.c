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

#include "nulib.h"
#include "nulib/file.h"
#include "ai5/game.h"

#include "cli.h"
#include "mes.h"

enum ai5_game_id target_game = GAME_ISAKU;

struct command cmd_a6 = {
	.name = "a6",
	.usage = "<command> ...",
	.description = "Tools for compiling and decompiling .a6 files",
	.parent = &cmd_elf,
	.commands = {
		//&cmd_a6_compile,
		&cmd_a6_decompile,
		NULL
	}
};

struct command cmd_anim = {
	.name = "anim",
	.usage = "<command> ...",
	.description = "Tools for compiling and decompiling animation files",
	.parent = &cmd_elf,
	.commands = {
		&cmd_anim_compile,
		&cmd_anim_decompile,
		&cmd_anim_render,
		NULL
	}
};

struct command cmd_arc = {
	.name = "arc",
	.usage = "<command> ...",
	.description = "Tools for packing and unpacking Elf/Silky's archive files",
	.parent = &cmd_elf,
	.commands = {
		&cmd_arc_extract,
		&cmd_arc_list,
		&cmd_arc_pack,
		NULL
	}
};

struct command cmd_ccd = {
	.name = "ccd",
	.usage = "<command> ...",
	.description = "Tools for packing and unpacking .ccd files",
	.parent = &cmd_elf,
	.commands = {
		//&cmd_ccd_pack,
		&cmd_ccd_unpack,
		NULL
	}
};

struct command cmd_cg = {
	.name = "cg",
	.usage = "<command> ...",
	.description = "Tools for converting images",
	.parent = &cmd_elf,
	.commands = {
		&cmd_cg_convert,
		NULL
	}
};

struct command cmd_eve = {
	.name = "eve",
	.usage = "<command> ...",
	.description = "Tools for packing and unpacking .eve files",
	.parent = &cmd_elf,
	.commands = {
		//&cmd_eve_pack,
		&cmd_eve_unpack,
		NULL
	}
};

struct command cmd_font = {
	.name = "font",
	.usage = "<command> ...",
	.description = "Tools for extracting and editing fonts",
	.parent = &cmd_elf,
	.commands = {
		&cmd_font_extract,
		//&cmd_font_pack,
		NULL
	}
};

struct command cmd_lzss = {
	.name = "lzss",
	.usage = "<command> ...",
	.description = "Tools for compressing and decompressing files with LZSS",
	.parent = &cmd_elf,
	.commands = {
		&cmd_lzss_compress,
		&cmd_lzss_decompress,
		NULL
	}
};

struct command cmd_mdd = {
	.name = "mdd",
	.usage = "<command> ...",
	.description = "Tools for .mdd files",
	.parent = &cmd_elf,
	.commands = {
		&cmd_mdd_render,
		NULL
	}
};

struct command cmd_mes = {
	.name = "mes",
	.usage = "<command> ...",
	.description = "Tools for compiling and decompiling .mes files",
	.parent = &cmd_elf,
	.commands = {
		&cmd_mes_compile,
		&cmd_mes_decompile,
		NULL
	}
};

struct command cmd_mp3 = {
	.name = "mp3",
	.usage = "<command> ...",
	.description = "Tools for packing and unpacking .mp3 files",
	.parent = &cmd_elf,
	.commands = {
		&cmd_mp3_extract,
		&cmd_mp3_render,
		NULL
	}
};

struct command cmd_mpx = {
	.name = "mpx",
	.usage = "<command> ...",
	.description = "Tools for packing and unpacking .mpx files",
	.parent = &cmd_elf,
	.commands = {
		//&cmd_mpx_pack,
		&cmd_mpx_unpack,
		NULL
	}
};

struct command cmd_elf = {
	.name = "elf",
	.usage = "<command> ...",
	.description = "Toolkit for extracting and editing Elf/Silky's file formats",
	.parent = NULL,
	.commands = {
		&cmd_a6,
		&cmd_anim,
		&cmd_arc,
		&cmd_ccd,
		&cmd_cg,
		&cmd_eve,
		&cmd_font,
		&cmd_lzss,
		&cmd_mdd,
		&cmd_mes,
		&cmd_mp3,
		&cmd_mpx,
		&cmd_save,
		NULL
	}
};

struct game_name {
	const char *name;
	enum ai5_game_id id;
	const char *description;
};

int main(int argc, char *argv[])
{
	if (argc < 2) {
		command_print_usage(&cmd_elf);
		exit(0);
	}
	return command_execute(&cmd_elf, argc, argv);
}
