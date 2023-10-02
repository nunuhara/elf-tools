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
#include <string.h>

#include "nulib.h"
#include "nulib/file.h"

#include "arc.h"
#include "cg.h"

struct cg *gxx_decode(uint8_t *data, size_t size, unsigned bpp);
struct cg *png_decode(uint8_t *data, size_t size);

bool gxx_write(struct cg *cg, FILE *out, unsigned bpp);
bool png_write(struct cg *cg, FILE *out);

struct cg *cg_load(uint8_t *data, size_t size, enum cg_type type)
{
	switch (type) {
	case CG_TYPE_G16: return gxx_decode(data, size, 16);
	case CG_TYPE_G24: return gxx_decode(data, size, 24);
	case CG_TYPE_G32: return gxx_decode(data, size, 32);
	case CG_TYPE_PNG: return png_decode(data, size);
	}
	ERROR("invalid CG type: %d", type);
}

enum cg_type cg_type_from_name(const char *name)
{
	const char *ext = file_extension(name);
	if (!strcasecmp(ext, "g16")) return CG_TYPE_G16;
	if (!strcasecmp(ext, "g24")) return CG_TYPE_G24;
	if (!strcasecmp(ext, "g32")) return CG_TYPE_G32;
	if (!strcasecmp(ext, "png")) return CG_TYPE_PNG;
	return -1;
}

struct cg *cg_load_arcdata(struct archive_data *data)
{
	enum cg_type type = cg_type_from_name(data->name);
	if (type < 0) {
		WARNING("Unrecognized CG type: %s", data->name);
		return NULL;
	}
	return cg_load(data->data, data->size, type);
}

bool cg_write(struct cg *cg, FILE *out, enum cg_type type)
{
	switch (type) {
	case CG_TYPE_G16: return gxx_write(cg, out, 16);
	case CG_TYPE_G24: return gxx_write(cg, out, 24);
	case CG_TYPE_G32: return gxx_write(cg, out, 32);
	case CG_TYPE_PNG: return png_write(cg, out);
	}
	ERROR("Invalid CG type: %d", type);
}

void cg_free(struct cg *cg)
{
	free(cg->pixels);
	free(cg);
}
