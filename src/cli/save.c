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
#include "nulib/file.h"
#include "nulib/little_endian.h"
#include "ai5/game.h"

#include "cli.h"

static unsigned nr_flags = 0;
static unsigned save_size = 0;
static unsigned flags_off = 128;
static unsigned sysvar16_off = 0;
static bool flags_packed = false;

enum {
	LOPT_GAME = 256,
};

static struct command cmd_save_get_flag;
static struct command cmd_save_info;

static void save_set_game(const char *game)
{
	ai5_set_game(game);
	switch (ai5_parse_game_id(game)) {
	case GAME_AI_SHIMAI:
	case GAME_ISAKU:
	case GAME_SHANGRLIA:
		nr_flags = 2048;
		save_size = 4096;
		break;
	case GAME_DOUKYUUSEI:
	case GAME_KAKYUUSEI:
	case GAME_YUNO:
		nr_flags = 4096;
		save_size = 8192;
		break;
	case GAME_SHUUSAKU:
		nr_flags = 2500;
		save_size = 3736;
		flags_off = 0x470;
		sysvar16_off = 0xe98;
		flags_packed = true;
		break;
	default:
		break;
	}
}

static void guess_nr_flags(size_t size)
{
	if (size == 4096)
		nr_flags = 2048;
	else if (size == 8192)
		nr_flags = 4096;
	else
		nr_flags = size - flags_off;
}

static void guess_sysvar16_off(void)
{
	sysvar16_off = flags_off + nr_flags + 4 + 26 * 2;
}

static uint8_t *read_save_file(const char *path, size_t *size_out)
{
	uint8_t *save = file_read(path, size_out);
	if (!save)
		CLI_ERROR("Failed to read save file \"%s\": %s", path, strerror(errno));
	if (save_size && *size_out != save_size)
		CLI_ERROR("Unexpected size of save file: %u (expected %u)",
				(unsigned)*size_out, save_size);
	return save;
}

static int save_get_flag(int argc, char *argv[])
{
	while (1) {
		int c = command_getopt(argc, argv, &cmd_save_get_flag);
		if (c == -1)
			break;
		switch (c) {
		case 'g':
		case LOPT_GAME:
			save_set_game(optarg);
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		command_usage_error(&cmd_save_get_flag, "Wrong number of arguments.\n");

	size_t size;
	uint8_t *mem16 = read_save_file(argv[1], &size);
	if (size <= flags_off)
		CLI_ERROR("Invalid save file");

	// determine valid range of flag numbers
	if (!nr_flags)
		guess_nr_flags(size);
	nr_flags = min(nr_flags, size - flags_off);

	// parse flag number
	char *endptr;
	long flag_no = strtol(argv[0], &endptr, 0);
	if (*endptr != '\0' || flag_no < 0)
		CLI_ERROR("Invalid flag number: %s", argv[0]);

	// read and print flag
	if (flags_packed) {
		if (flag_no >= nr_flags * 2)
			CLI_ERROR("Invalid flag number: %s", argv[0]);
		uint8_t flag = mem16[flags_off + flag_no/2];
		if (flag_no % 2)
			flag &= 0xf;
		else
			flag >>= 4;
		NOTICE("flag[%ld] = %u", flag_no, (unsigned)flag);
	} else {
		if (flag_no >= nr_flags)
			CLI_ERROR("Invalid flag number: %s", argv[0]);
		uint8_t flag = mem16[flags_off + flag_no];
		NOTICE("flag[%ld] = %u", flag_no, (unsigned)flag);
	}
	free(mem16);
	return 0;
}

static struct command cmd_save_get_flag = {
	.name = "get-flag",
	.usage = "<flag-number> <save-file>",
	.description = "Get the value of a flag",
	.parent = &cmd_save,
	.fun = save_get_flag,
	.options = {
		{ "game", 'g', "Set the target game", required_argument, LOPT_GAME },
		{ 0 }
	}
};

static int save_get_sysvar16(int argc, char *argv[])
{
	while (1) {
		int c = command_getopt(argc, argv, &cmd_save_get_flag);
		if (c == -1)
			break;
		switch (c) {
		case 'g':
		case LOPT_GAME:
			save_set_game(optarg);
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		command_usage_error(&cmd_save_get_flag, "Wrong number of arguments.\n");

	size_t size;
	uint8_t *mem16 = read_save_file(argv[1], &size);
	if (size <= 128)
		CLI_ERROR("Invalid save file");

	// determine offset of sysvar16
	if (!nr_flags)
		guess_nr_flags(size);
	if (!sysvar16_off)
		guess_sysvar16_off();

	// parse flag number
	char *endptr;
	long var_no = strtol(argv[0], &endptr, 0);
	if (*endptr != '\0' || var_no < 0 || sysvar16_off + var_no + 2 > size)
		CLI_ERROR("Invalid flag number: %s", argv[0]);

	// read and print flag
	uint16_t var = le_get16(mem16, sysvar16_off + var_no * 2);
	NOTICE("System.var16[%ld] = %u", var_no, (unsigned)var);

	free(mem16);
	return 0;

}

static struct command cmd_save_get_sysvar16 = {
	.name = "get-sysvar16",
	.usage = "<var-number> <save-file>",
	.description = "Get the value of a system word (or heap word)",
	.parent = &cmd_save,
	.fun = save_get_sysvar16,
	.options = {
		{ "game", 'g', "Set the target game", required_argument, LOPT_GAME },
		{ 0 }
	}
};

static int save_info(int argc, char *argv[])
{
	while (1) {
		int c = command_getopt(argc, argv, &cmd_save_info);
		if (c == -1)
			break;
		switch (c) {
		case 'g':
		case LOPT_GAME:
			save_set_game(optarg);
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		command_usage_error(&cmd_save_get_flag, "Wrong number of arguments.\n");

	size_t size;
	uint8_t *mem16 = read_save_file(argv[0], &size);

	if (ai5_target_game == GAME_DOUKYUUSEI) {
		NOTICE("Affection Level");
		NOTICE("---------------");
		NOTICE("   Miho: %u", (unsigned)mem16[128 + 102]);
		NOTICE(" Hiromi: %u", (unsigned)mem16[128 + 103]);
		NOTICE(" Satomi: %u", (unsigned)mem16[128 + 105]);
		NOTICE("    Ako: %u", (unsigned)mem16[128 + 106]);
		NOTICE("  Kaori: %u", (unsigned)mem16[128 + 107]);
		NOTICE("   Mako: %u", (unsigned)mem16[128 + 108]);
		NOTICE("Natsuko: %u", (unsigned)mem16[128 + 109]);
		NOTICE("Yoshiko: %u", (unsigned)mem16[128 + 110]);
		NOTICE("    Mai: %u", (unsigned)mem16[128 + 111]);
		NOTICE(" Kurumi: %u", (unsigned)mem16[128 + 112]);
		NOTICE("   Misa: %u", (unsigned)mem16[128 + 113]);
		NOTICE("Chiharu: %u", (unsigned)mem16[128 + 114]);
		NOTICE("  Reiko: %u", (unsigned)mem16[128 + 115]);
		NOTICE("  Yayoi: %u", (unsigned)mem16[128 + 116]);
	} else if (ai5_target_game == GAME_KAKYUUSEI) {
#define SYSVAR16(n) ((unsigned)le_get16(mem16, 4280 + n*2))
		NOTICE("Affection Level");
		NOTICE("---------------");
		NOTICE("  Reiko: %u", SYSVAR16(200));
		NOTICE("   Miko: %u", SYSVAR16(201));
		NOTICE(" Mizuho: %u", SYSVAR16(202));
		NOTICE(" Mayumi: %u", SYSVAR16(203));
		NOTICE(" Ryouko: %u", SYSVAR16(204));
		NOTICE("   Tina: %u", SYSVAR16(205));
		NOTICE("   Nana: %u", SYSVAR16(206));
		NOTICE(" Miyuki: %u", SYSVAR16(207));
		NOTICE("     Ai: %u", SYSVAR16(208));
		NOTICE("Shizuka: %u", SYSVAR16(209));
		NOTICE(" Mahoko: %u", SYSVAR16(210));
		NOTICE("Minatsu: %u", SYSVAR16(211));
		NOTICE("   Maki: %u", SYSVAR16(212));
#undef SYSVAR16
	} else {
		NOTICE("Save info not supported for this game.");
	}

	free(mem16);
	return 0;
}

static struct command cmd_save_info = {
	.name = "info",
	.usage = "--game <game> <save-file>",
	.description = "Print game-specific info",
	.parent = &cmd_save,
	.fun = save_info,
	.options = {
		{ "game", 'g', "Set the target game", required_argument, LOPT_GAME },
		{ 0 }
	}
};

struct command cmd_save = {
	.name = "save",
	.usage = "<command> ...",
	.description = "Tools for reading/writing save files",
	.parent = &cmd_elf,
	.commands = {
		&cmd_save_get_flag,
		&cmd_save_info,
		&cmd_save_get_sysvar16,
		//&cmd_save_get_reg,
		//&cmd_save_get_reg32,
		NULL
	}
};
