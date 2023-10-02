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
#include <errno.h>

#include "nulib.h"
#include "nulib/little_endian.h"
#include "nulib/lzss.h"

#include "cg.h"

static unsigned gxx_stride(struct cg_metrics *metrics)
{
	return ((metrics->bpp / 8) * metrics->w + 3) & ~3;
}

static uint8_t *bgr555_to_rgba(uint8_t *data, size_t size, struct cg_metrics *metrics)
{
	unsigned stride = gxx_stride(metrics);
	if (size != stride * metrics->h) {
		WARNING("Unexpected size for CG: expected %u; got %u",
				stride * metrics->h, (unsigned)size);
		return NULL;
	}

	uint8_t *out = xmalloc(metrics->w * metrics->h * 4);
	uint8_t *out_p = out;
	for (int row = metrics->h - 1; row >= 0; row--) {
		uint8_t *px = data + stride * row;
		for (int col = 0; col < metrics->w; col++) {
			uint16_t c = le_get16(px, col * 2);
			*out_p++ = (c & 0x7c00) >> 7;
			*out_p++ = (c & 0x03e0) >> 2;
			*out_p++ = (c & 0x001f) << 3;
			*out_p++ = 0xff;
		}
	}

	return out;
}

static uint8_t *rgba_to_bgr555(uint8_t *data, struct cg_metrics *metrics)
{
	unsigned stride = gxx_stride(metrics);
	uint8_t *out = xcalloc(metrics->h, stride);
	for (int row = metrics->h - 1; row >= 0; row--) {
		uint8_t *out_p = out + stride * row;
		for (int col = 0; col < metrics->w; col++) {
			uint16_t c = 0;
			c |= (*data++ & 0xf8) << 7;
			c |= (*data++ & 0xf8) << 2;
			c |= (*data++ & 0xf8) >> 3;
			data++; // drop alpha
			le_put16(out_p, col * 2, c);
		}
	}
	return out;
}

static uint8_t *bgr_to_rgba(uint8_t *data, size_t size, struct cg_metrics *metrics)
{
	unsigned stride = gxx_stride(metrics);
	if (size != stride * metrics->h) {
		WARNING("Unexpected size for CG: expected %u; got %u",
				stride * metrics->h, (unsigned)size);
		return NULL;
	}

	uint8_t *out = xmalloc(metrics->w * metrics->h * 4);
	uint8_t *out_p = out;
	for (int row = metrics->h - 1; row >= 0; row--) {
		uint8_t *px = data + stride * row;
		for (int col = 0; col < metrics->w; col++) {
			*out_p++ = px[col * 3 + 2];
			*out_p++ = px[col * 3 + 1];
			*out_p++ = px[col * 3 + 0];
			*out_p++ = 0xff;
		}
	}

	return out;
}

static uint8_t *rgba_to_bgr(uint8_t *data, struct cg_metrics *metrics)
{
	unsigned stride = gxx_stride(metrics);
	uint8_t *out = xmalloc(metrics->h * stride);
	for (int row = metrics->h - 1; row >= 0; row--) {
		uint8_t *out_p = out + stride * row;
		for (int col = 0; col < metrics->w; col++) {
			out_p[col * 3 + 2] = *data++;
			out_p[col * 3 + 1] = *data++;
			out_p[col * 3 + 0] = *data++;
			data++; // drop alpha
		}
	}
	return out;
}

static uint8_t *bgra_to_rgba(uint8_t *data, size_t size, struct cg_metrics *metrics)
{
	unsigned stride = gxx_stride(metrics);
	if (size != stride * metrics->h) {
		WARNING("Unexpected size for CG: expected %u; got %u",
				stride * metrics->h, (unsigned)size);
		return NULL;
	}

	uint8_t *out = xmalloc(metrics->w * metrics->h * 4);
	uint8_t *out_p = out;
	for (int row = metrics->h - 1; row >= 0; row--) {
		uint8_t *px = data + stride * row;
		for (int col = 0; col < metrics->w; col++) {
			*out_p++ = px[col * 4 + 2];
			*out_p++ = px[col * 4 + 1];
			*out_p++ = px[col * 4 + 0];
			*out_p++ = px[col * 4 + 3];;
		}
	}

	return out;
}

static uint8_t *rgba_to_bgra(uint8_t *data, struct cg_metrics *metrics)
{
	unsigned stride = gxx_stride(metrics);
	uint8_t *out = xmalloc(metrics->h * stride);
	for (int row = metrics->h - 1; row >= 0; row--) {
		uint8_t *out_p = out + stride * row;
		for (int col = 0; col < metrics->w; col++) {
			out_p[col * 4 + 2] = *data++;
			out_p[col * 4 + 1] = *data++;
			out_p[col * 4 + 0] = *data++;
			out_p[col * 4 + 3] = *data++;
		}
	}
	return out;
}

struct cg *gxx_decode(uint8_t *data, size_t size, unsigned bpp)
{
	struct cg *cg = xcalloc(1, sizeof(struct cg));
	cg->metrics.x = le_get16(data, 0);
	cg->metrics.y = le_get16(data, 2);
	cg->metrics.w = le_get16(data, 4);
	cg->metrics.h = le_get16(data, 6);
	cg->metrics.bpp = bpp;
	cg->metrics.has_alpha = false;

	size_t px_size;
	uint8_t *px_data = lzss_decompress(data+8, size-8, &px_size);

	if (bpp == 16)
		cg->pixels = bgr555_to_rgba(px_data, px_size, &cg->metrics);
	else if (bpp == 24)
		cg->pixels = bgr_to_rgba(px_data, px_size, &cg->metrics);
	else if (bpp == 32)
		cg->pixels = bgra_to_rgba(px_data, px_size, &cg->metrics);
	else
		ERROR("unsupported bpp: %u", bpp);

	free(px_data);
	if (!cg->pixels) {
		free(cg);
		return NULL;
	}
	return cg;
}

static bool write_u16(FILE *out, uint32_t v)
{
	uint8_t b[2];
	le_put16(b, 0, v);
	if (fwrite(b, 2, 1, out) != 1) {
		WARNING("Write failure: %s", strerror(errno));
		return false;
	}
	return true;
}

bool gxx_write(struct cg *cg, FILE *out, unsigned bpp)
{
	uint8_t *data;
	struct cg_metrics metrics = cg->metrics;
	metrics.bpp = bpp;
	size_t data_size = gxx_stride(&metrics) * metrics.h;
	if (bpp == 16)
		data = rgba_to_bgr555(cg->pixels, &metrics);
	else if (bpp == 24)
		data = rgba_to_bgr(cg->pixels, &metrics);
	else if (bpp == 32)
		data = rgba_to_bgra(cg->pixels, &metrics);
	else
		ERROR("unsupported bpp: %u", bpp);

	size_t zipped_size;
	uint8_t *zipped = lzss_compress(data, data_size, &zipped_size);
	free(data);

	if (!write_u16(out, metrics.x)) return false;
	if (!write_u16(out, metrics.y)) return false;
	if (!write_u16(out, metrics.w)) return false;
	if (!write_u16(out, metrics.h)) return false;
	if (fwrite(zipped, zipped_size, 1, out) != 1) {
		WARNING("Write failure: %s", strerror(errno));
		free(zipped);
		return false;
	}

	free(zipped);
	return true;
}
