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

#include "cli.h"
#include "mes.h"
#include "game.h"

enum game_id target_game = GAME_ISAKU;

struct command cmd_arc = {
	.name = "arc",
	.usage = "<command> ...",
	.description = "Tools for packing and unpacking Elf/Silky's archive files",
	.parent = &cmd_elf,
	.commands = {
		&cmd_arc_extract,
		&cmd_arc_list,
		//&cmd_arc_pack,
		NULL
	}
};

struct command cmd_mes = {
	.name = "mes",
	.usage = "<command> ...",
	.description = "Tools for compiling and decompiling .mes files",
	.parent = &cmd_elf,
	.commands = {
		//&cmd_mes_compile,
		&cmd_mes_decompile,
		NULL
	}
};

struct command cmd_elf = {
	.name = "elf",
	.usage = "<command> ...",
	.description = "Toolkit for extracting and editing Elf/Silky's file formats",
	.parent = NULL,
	.commands = {
		&cmd_arc,
		&cmd_mes,
		NULL
	}
};

struct game_name {
	const char *name;
	enum game_id id;
	const char *description;
};

static struct game_name game_names[] = {
	{ "aishimai",   GAME_AI_SHIMAI,    "愛姉妹 ～二人の果実～" },
	{ "beyond",     GAME_BEYOND,       "ビ・ ヨンド ～黒大将に見られてる～" },
	{ "doukyuusei", GAME_DOUKYUUSEI,   "同級生 Windows版" },
	{ "isaku",      GAME_ISAKU,        "遺作 リニューアル" },
	{ "koihime",    GAME_KOIHIME,      "恋姫" },
	{ "yukinojou",  GAME_YUKINOJOU,    "あしたの雪之丞" },
	{ "yuno",       GAME_ELF_CLASSICS, "この世の果てで恋を唄う少女YU-NO (エルフclassics)" },
};

static enum game_id parse_game_id(const char *str)
{
	for (unsigned i = 0; i < ARRAY_SIZE(game_names); i++) {
		if (!strcmp(str, game_names[i].name))
			return game_names[i].id;
	}
	sys_warning("Unrecognized game name: %s\n", str);
	sys_warning("Valid names are:\n");
	for (unsigned i = 0; i < ARRAY_SIZE(game_names); i++) {
		sys_warning("    %-11s - %s\n", game_names[i].name, game_names[i].description);
	}
	sys_exit(EXIT_FAILURE);
}

void set_game(const char *name)
{
	mes_set_game((target_game = parse_game_id(name)));
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		command_print_usage(&cmd_elf);
		exit(0);
	}
	return command_execute(&cmd_elf, argc, argv);
}
