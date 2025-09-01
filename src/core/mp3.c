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
#include "nulib/little_endian.h"
#include "ai5/cg.h"
#include "map.h"

static uint8_t palette[] = {
	0x00, 0x00, 0x00, 0xff, // bg = black
	0xff, 0xff, 0xff, 0xff, // wall = white
	0x88, 0x88, 0x88, 0xff, // window = grey
	0x00, 0x00, 0xff, 0xff, // door = red
	0xff, 0xff, 0x00, 0xff,
	0x00, 0xff, 0xff, 0xff,
	0xff, 0x00, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0x77, 0x00, 0x00, 0xff,
	0x00, 0x77, 0x00, 0xff,
	0x00, 0x00, 0x77, 0xff,
	0x77, 0x77, 0x00, 0xff,
	0x00, 0x77, 0x77, 0xff,
	0x77, 0x00, 0x77, 0xff,
	0x77, 0x77, 0x77, 0xff,
	0xff, 0x77, 0x00, 0xff,
};

#define TILE_SIZE 16
#define BORDER_SIZE 2

enum direction { NORTH, EAST, SOUTH, WEST };

static void render_wall(struct cg *cg, unsigned tx, unsigned ty, enum direction side, uint8_t c)
{
	unsigned x, y, w, h;
	switch (side) {
	case NORTH:
		x = tx * TILE_SIZE + BORDER_SIZE;
		y = ty * TILE_SIZE;
		w = TILE_SIZE - BORDER_SIZE * 2;
		h = BORDER_SIZE;
		break;
	case SOUTH:
		x = tx * TILE_SIZE + BORDER_SIZE;
		y = ty * TILE_SIZE + (TILE_SIZE - BORDER_SIZE);
		w = TILE_SIZE - BORDER_SIZE * 2;
		h = BORDER_SIZE;
		break;
	case WEST:
		x = tx * TILE_SIZE;
		y = ty * TILE_SIZE + BORDER_SIZE;
		w = BORDER_SIZE;
		h = TILE_SIZE - BORDER_SIZE * 2;
		break;
	case EAST:
		x = tx * TILE_SIZE + (TILE_SIZE - BORDER_SIZE);
		y = ty * TILE_SIZE + BORDER_SIZE;
		w = BORDER_SIZE;
		h = TILE_SIZE - BORDER_SIZE * 2;
		break;
	}

	for (unsigned row = 0; row < h; row++) {
		uint8_t *p = cg->pixels + (y + row) * cg->metrics.w + x;
		for (unsigned col = 0; col < w; col++, p++) {
			*p = c;
		}
	}
}

struct cg *mp3_render(uint8_t *data, size_t data_size)
{
	uint16_t tw = le_get16(data, 0);
	uint16_t th = le_get16(data, 2);
	if (tw > 1000 || th > 1000) {
		WARNING("map dimensions are not sane: %ux%u", (unsigned)tw, (unsigned)th);
		return NULL;
	}

	struct cg *cg = cg_alloc_indexed(tw * 16, th * 16);

	// initialize palette
	memcpy(cg->palette, palette, ARRAY_SIZE(palette));

	uint8_t *tile = data + 4;
	for (int ty = 0; ty < th; ty++) {
		for (int tx = 0; tx < tw; tx++, tile += 4) {
			uint16_t walls = le_get16(tile, 0);
			render_wall(cg, tx, ty, NORTH, (walls & 0x7000) >> 12);
			render_wall(cg, tx, ty, EAST,  (walls & 0x0700) >> 8);
			render_wall(cg, tx, ty, SOUTH, (walls & 0x0070) >> 4);
			render_wall(cg, tx, ty, WEST,  (walls & 0x0007));
		}
	}

	return cg;
}
