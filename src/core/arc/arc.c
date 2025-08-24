/* Copyright (C) 2025 Nunuhara Cabbage <nunuhara@haniwa.technology>
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

#include <stdbool.h>
#include <string.h>
#include <strings.h>

#include "ai5/game.h"

enum archive_data_type {
	ARC_OTHER,
	ARC_MES,
	ARC_DATA,
	ARC_AUDIO,
};

static bool suffix_equal(const char *str, const char *suffix)
{
	size_t str_len = strlen(str);
	size_t suffix_len = strlen(suffix);
	if (str_len < suffix_len)
		return false;

	const char *start = str + (str_len - suffix_len);
	return !strcasecmp(start, suffix);
}

static enum archive_data_type arc_data_type(const char *path)
{
	if (suffix_equal(path, "mes.arc") || suffix_equal(path, "message.arc"))
		return ARC_MES;
	if (suffix_equal(path, "data.arc"))
		return ARC_DATA;
	if (suffix_equal(path, ".awd") || suffix_equal(path, ".awf"))
		return ARC_AUDIO;
	return ARC_OTHER;
}

bool arc_is_compressed(const char *path, enum ai5_game_id game_id)
{
	switch (game_id) {
	case GAME_ALLSTARS:
	case GAME_KAWARAZAKIKE:
		return false;
	default:
		break;
	}

	enum archive_data_type t = arc_data_type(path);
	if (t == ARC_AUDIO)
		return false;
	if (ai5_target_game == GAME_KAKYUUSEI)
		return t == ARC_MES;

	return t == ARC_MES || t == ARC_DATA;
}
