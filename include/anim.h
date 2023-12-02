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

#ifndef ELF_TOOLS_ANIM_H
#define ELF_TOOLS_ANIM_H

#include <stddef.h>
#include <stdint.h>

struct s4;
struct cg;

struct s4 *s4_parse_script(const char *path);
uint8_t *s4_pack(struct s4 *in, size_t *size_out);
uint8_t *s4_render_gif(struct s4 *anim, struct cg *src, struct cg *dst,
		unsigned max_frames, size_t *size_out);

#endif // ELF_TOOLS_ANIM_H
