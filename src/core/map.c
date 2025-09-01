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

#include "nulib.h"
#include "nulib/buffer.h"
#include "nulib/port.h"

#include "map.h"

void mpx_print(struct port *out, struct buffer *in)
{
	uint16_t nr_cols = buffer_read_u16(in);
	uint16_t nr_rows = buffer_read_u16(in);

	for (unsigned row = 0; row < nr_rows; row++) {
		for (unsigned col = 0; col < nr_cols; col++) {
			if (buffer_remaining(in) < 5) {
				WARNING("MPX file truncated at (%u,%u)?", col, row);
				return;
			}
			uint16_t bg_tileno = buffer_read_u16(in);
			uint16_t fg_tileno = buffer_read_u16(in);
			uint8_t flags = buffer_read_u8(in);
			const char *prefix = col ? "|" : "";
			if (bg_tileno == 0xffff && fg_tileno == 0xffff)
				port_printf(out, "%s----,----,%2x ", prefix, flags);
			else if (bg_tileno == 0xffff)
				port_printf(out, "%s----,%4u,%2x ", prefix, fg_tileno, flags);
			else if (fg_tileno == 0xffff)
				port_printf(out, "%s%4u,----,%2x ", prefix, bg_tileno, flags);
			else
				port_printf(out, "%s%4u,%4u,%2x ", prefix, bg_tileno, fg_tileno, flags);
		}
		port_putc(out, '\n');
	}
	if (!buffer_end(in))
		WARNING("Junk at end of MPX file?");
}

void eve_print(struct port *out, uint8_t *data, size_t data_size)
{
	port_printf(out, "  ID,     X,     Y,    CX,    CY, UK\n");
	for (unsigned off = 0; off + 12 < data_size; off += 12) {
		port_printf(out, "%4u, %5u, %5u, %5u, %5u, %2u\n",
				le_get16(data, off),
				le_get16(data, off+2),
				le_get16(data, off+4),
				le_get16(data, off+6),
				le_get16(data, off+8),
				data[off+10]);
	}
}
