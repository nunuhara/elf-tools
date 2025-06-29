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

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "nulib.h"
#include "nulib/file.h"
#include "nulib/little_endian.h"
#include "nulib/port.h"
#include "nulib/string.h"
#include "ai5/cg.h"
#include "ai5/game.h"

#include "msf_gif.h"

static uint8_t *decode_offset(uint8_t *dst, int stride, uint8_t b)
{
	static int same_line_offsets[8] = {
		-1, -2, -4, -6, -8, -12, -16, -20
	};
	static int prev_line_offsets[16] = {
		-20, -16, -12, -8, -6, -4, -2, -1, 0, 1, 2, 4, 6, 8, 12, 16
	};

	int x_off;
	int y_off;
	if (b & 0x70) {
		x_off = prev_line_offsets[b & 0xf];
		y_off = -((b >> 4) & 7);
	} else {
		x_off = same_line_offsets[b & 0x7];
		y_off = 0;
	}

	return dst + stride * y_off + x_off * 4;
}

struct movie {
	unsigned nr_frames;
	unsigned w, h;
	uint8_t *frame[100];
	uint8_t *palette;
};

static struct cg *render_frame(struct movie *mov, unsigned frame)
{
	struct cg *cg = cg_alloc_direct(mov->w, mov->h);
	uint8_t *src = mov->frame[frame];
	for (int row = 0; row < mov->h; row++) {
		uint8_t *dst = cg->pixels + row * mov->w * 4;
		for (int col = 0; col < mov->w;) {
			uint8_t b = *src++;
			if (b & 0x80) {
				uint8_t *copy_src = decode_offset(dst, mov->w * 4, b);
				unsigned len = *src++ + 2;
				for (unsigned i = 0; i < len; i++, col++) {
					assert(col < mov->w);
					memcpy(dst, copy_src, 4);
					copy_src += 4;
					dst += 4;
				}
			} else {
				// literal bytes
				for (unsigned i = 0; i < b; i++, col++) {
					assert(col < mov->w);
					uint8_t c = *src++;
					dst[0] = mov->palette[(c-10)*3+0];
					dst[1] = mov->palette[(c-10)*3+1];
					dst[2] = mov->palette[(c-10)*3+2];
					dst[3] = 255;
					dst += 4;
				}
			}
		}
	}

	return cg;
}

uint8_t *mdd_render(uint8_t *data, size_t size, size_t *size_out)
{
	struct movie mov;
	mov.nr_frames = le_get32(data, 0);
	mov.w = le_get16(data, 4);
	mov.h = le_get16(data, 6);

	uint8_t *frame_data = data + 8 + mov.nr_frames * 4 + 708;
	for (unsigned i = 0; i < mov.nr_frames; i++) {
		mov.frame[i] = frame_data + le_get32(data, 8 + i * 4);
	}

	mov.palette = data + 8 + mov.nr_frames * 4;

	MsfGifState gif_state = {};
	msf_gif_begin(&gif_state, mov.w, mov.h);
	for (unsigned frame = 0; frame < mov.nr_frames; frame++) {
		struct cg *cg = render_frame(&mov, frame);
		if (!msf_gif_frame(&gif_state, cg->pixels, 8, 16, mov.w * 4))
			ERROR("msf_gif_frame");
		cg_free(cg);
	}
	MsfGifResult gif = msf_gif_end(&gif_state);
	*size_out = gif.dataSize;
	return gif.data;
}
