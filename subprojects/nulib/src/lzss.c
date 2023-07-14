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

#include "nulib.h"
#include "nulib/buffer.h"

#define FRAME_SIZE 0x1000
#define FRAME_INIT_POS 0xfee

uint8_t *lzss_decompress(uint8_t *input, size_t input_size, size_t *output_size)
{
	uint8_t frame[FRAME_SIZE] = {0};
	unsigned frame_pos = FRAME_INIT_POS;
	unsigned frame_mask = FRAME_SIZE - 1;

	struct buffer in;
	buffer_init(&in, input, input_size);

	struct buffer out;
	buffer_init(&out, NULL, 0);

	while (!buffer_end(&in)) {
		uint8_t ctl = buffer_read_u8(&in);
		for (unsigned bit = 1; bit != 0x100; bit <<= 1) {
			if (ctl & bit) {
				if (buffer_remaining(&in) < 1)
					break;
				uint8_t b = buffer_read_u8(&in);;
				frame[frame_pos++ & frame_mask] = b;
				buffer_write_u8(&out, b);
			} else {
				if (buffer_remaining(&in) < 2)
					break;
				uint8_t lo = buffer_read_u8(&in);
				uint8_t hi = buffer_read_u8(&in);
				uint16_t offset = ((hi & 0xf0) << 4) | lo;
				for (int count = 3 + (hi & 0xf); count != 0; --count) {
					uint8_t v = frame[offset++ & frame_mask];
					frame[frame_pos++ & frame_mask] = v;
					buffer_write_u8(&out, v);
				}
			}
		}
	}
	*output_size = out.index;
	return out.buf;
}
