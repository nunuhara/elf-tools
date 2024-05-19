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

#ifndef ELF_TOOLS_CLI_H
#define ELF_TOOLS_CLI_H

#include "nulib/command.h"

#define CLI_ERROR(fmt, ...) sys_error(fmt "\n", ##__VA_ARGS__)
#define CLI_WARNING(fmt, ...) sys_warning("*WARNING*: " fmt "\n", ##__VA_ARGS__)

extern struct command cmd_elf;
extern struct command cmd_a6;
extern struct command cmd_a6_compile;
extern struct command cmd_a6_decompile;
extern struct command cmd_anim;
extern struct command cmd_anim_compile;
extern struct command cmd_anim_decompile;
extern struct command cmd_anim_render;
extern struct command cmd_arc;
extern struct command cmd_arc_extract;
extern struct command cmd_arc_list;
extern struct command cmd_arc_pack;
extern struct command cmd_ccd;
extern struct command cmd_ccd_unpack;
extern struct command cmd_cg;
extern struct command cmd_cg_convert;
extern struct command cmd_eve;
extern struct command cmd_eve_unpack;
extern struct command cmd_font;
extern struct command cmd_font_extract;
extern struct command cmd_lzss;
extern struct command cmd_lzss_compress;
extern struct command cmd_lzss_decompress;
extern struct command cmd_mes;
extern struct command cmd_mes_compile;
extern struct command cmd_mes_decompile;

enum game_id parse_game_id(const char *str);

#endif // ELF_TOOLS_CLI_H
