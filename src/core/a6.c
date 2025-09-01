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

#include <string.h>

#include "nulib.h"
#include "ai5/a6.h"
#include "ai5/cg.h"
#include "a6.h"

static void a6_dims(a6_array a, unsigned *w, unsigned *h)
{
	unsigned w_max = 0;
	unsigned h_max = 0;
	struct a6_entry *e;
	vector_foreach_p(e, a) {
		w_max = max(w_max, e->x_right + 1);
		h_max = max(h_max, e->y_bot + 1);
	}
	*w = w_max;
	*h = h_max;
}

#define MAX_ID 8192

struct cg *a6_to_image(a6_array a)
{
	int last_color = 0;
	uint8_t id_to_color[MAX_ID] = {0};
	struct a6_entry *e;
	vector_foreach_p(e, a) {
		if (e->id >= MAX_ID) {
			WARNING("Unexpectedly large ID: %u", e->id);
			return NULL;
		}
		if (!id_to_color[e->id]) {
			if (last_color >= 256) {
				WARNING("Too many IDs");
				return NULL;
			}
			id_to_color[e->id] = ++last_color;
		}
	}

	unsigned w, h;
	a6_dims(a, &w, &h);

	if (w == 0 || w > 1920) {
		WARNING("Invalid width for image: %u\n", w);
		return NULL;
	}
	if (h == 0 || h > 1080) {
		WARNING("Invalid height for image: %u\n", h);
		return NULL;
	}

	struct cg *cg = cg_alloc_indexed(w, h);
#define SET_PAL(i, r, g, b) \
	cg->palette[i*4+0] = b; \
	cg->palette[i*4+1] = g; \
	cg->palette[i*4+2] = r;
	SET_PAL(0,  0x00, 0x00, 0x00);
	SET_PAL(1,  0xff, 0xff, 0xff);
	SET_PAL(2,  0xff, 0x00, 0x00);
	SET_PAL(3,  0x00, 0xff, 0x00);
	SET_PAL(4,  0x00, 0x00, 0xff);
	SET_PAL(5,  0xff, 0xff, 0x00);
	SET_PAL(6,  0x00, 0xff, 0xff);
	SET_PAL(7,  0xff, 0x00, 0xff);
	SET_PAL(8,  0x77, 0x77, 0x77);
	SET_PAL(9,  0x77, 0x00, 0x00);
	SET_PAL(10, 0x00, 0x77, 0x00);
	SET_PAL(11, 0x00, 0x00, 0x77);
	SET_PAL(12, 0x77, 0x77, 0x00);
	SET_PAL(13, 0x00, 0x77, 0x77);
	SET_PAL(14, 0x77, 0x00, 0x77);
	SET_PAL(15, 0xff, 0x77, 0x00);
	SET_PAL(16, 0xff, 0x00, 0x77);
	SET_PAL(17, 0x77, 0xff, 0x00);
	SET_PAL(18, 0x77, 0x00, 0xff);
	SET_PAL(19, 0x00, 0xff, 0x77);
	SET_PAL(20, 0x00, 0x77, 0xff);
	for (int i = 21; i < 256; i++) {
		SET_PAL(i, 0x33, 0x33, 0x33);
	}

	vector_foreach_reverse_p(e, a) {
		if (e->x_left > e->x_right || e->y_top > e->y_bot) {
			WARNING("Invalid rectangle: (%u,%u,%u,%u)", e->x_left, e->y_top,
					e->x_right, e->y_bot);
			cg_free(cg);
			return NULL;
		}
		unsigned x = e->x_left;
		unsigned y = e->y_top;
		unsigned w = (e->x_right - e->x_left) + 1;
		unsigned h = (e->y_bot - e->y_top) + 1;

		uint8_t *base = cg->pixels + y * cg->metrics.w + x;
		for (int row = 0; row < h; row++) {
			uint8_t *dst = base + row * cg->metrics.w;
			memset(dst, id_to_color[e->id], w);
		}
	}

	return cg;
}
