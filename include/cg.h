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

#ifndef ELF_TOOLS_CG_H
#define ELF_TOOLS_CG_H

#include <stdint.h>
#include <stdio.h>

struct archive_data;

enum cg_type {
	CG_TYPE_G16,
	CG_TYPE_G24,
	CG_TYPE_G32,
	CG_TYPE_PNG,
	//CG_TYPE_BMP,
};

struct cg_metrics {
	unsigned x;
	unsigned y;
	unsigned w;
	unsigned h;
	unsigned bpp;
	bool has_alpha;
};

struct cg {
	struct cg_metrics metrics;
	uint8_t *pixels;
};

enum cg_type cg_type_from_name(const char *name);

struct cg *cg_load(uint8_t *data, size_t size, enum cg_type type);
struct cg *cg_load_arcdata(struct archive_data *data);

bool cg_write(struct cg *cg, FILE *out, enum cg_type type);

void cg_free(struct cg *cg);

#endif // ELF_TOOLS_CG_H
