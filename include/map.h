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

#ifndef ELF_TOOLS_MAP_H
#define ELF_TOOLS_MAP_H

#include <stdint.h>
#include <stddef.h>

struct port;
struct buffer;
struct cg;

// doukyuusei/kakyuusei
void mpx_print(struct port *out, struct buffer *in);
void eve_print(struct port *out, uint8_t *data, size_t data_size);

// isaku
struct cg *mp3_render(uint8_t *data, size_t data_size);

#endif // ELF_TOOLS_MAP_H
