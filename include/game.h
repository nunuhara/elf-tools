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

#ifndef ELF_TOOLS_GAME_H_
#define ELF_TOOLS_GAME_H_

enum game_id {
	GAME_YUKINOJOU,    // 2001-08-31 release
	GAME_ELF_CLASSICS, // 2000-12-22 release
	GAME_BEYOND,       // 2000-07-19 release
	GAME_AI_SHIMAI,    // 2000-05-26 release (windows 98/2k version)
	GAME_KOIHIME,      // 1999-12-24 release
	GAME_DOUKYUUSEI,   // 1999-08-27 release (windows edition)
	GAME_ISAKU,        // 1999-02-26 release (renewal version)
};

extern enum game_id target_game;

#endif // ELF_TOOLS_GAME_H_
