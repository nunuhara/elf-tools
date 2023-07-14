/* Copyright (C) 2019 Nunuhara Cabbage <nunuhara@haniwa.technology>
 *
 * Based on code from xsystem35
 * Copyright (C) 1997-1998 Masaki Chikama (Wren) <chikama@kasumi.ipl.mech.nagoya-u.ac.jp>
 *               1998-                           <masaki-c@is.aist-nara.ac.jp>
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

#ifndef NULIB_LITTLE_ENDIAN_H
#define NULIB_LITTLE_ENDIAN_H

#include <stddef.h>
#include <stdint.h>

static inline int32_t le_get32(const uint8_t *b, size_t i)
{
	int c0, c1, c2, c3;
	int d0, d1;
	c0 = *(b + i  + 0);
	c1 = *(b + i  + 1);
	c2 = *(b + i  + 2);
	c3 = *(b + i  + 3);
	d0 = c0 + (c1 << 8);
	d1 = c2 + (c3 << 8);
	return d0 + (d1 << 16);
}

static inline int16_t le_get16(const uint8_t *b, size_t i)
{
	int c0, c1;
	c0 = *(b + i + 0);
	c1 = *(b + i + 1);
	return c0 + (c1 << 8);
}

static inline void le_put32(uint8_t *dst, int i, uint32_t dword)
{
	dst[i]   = (uint8_t)(dword);
	dst[i+1] = (uint8_t)(dword >> 8);
	dst[i+2] = (uint8_t)(dword >> 16);
	dst[i+3] = (uint8_t)(dword >> 24);
}

static inline void le_put16(uint8_t *dst, int i, uint16_t word)
{
	dst[i]   = word & 0xFF;
	dst[i+1] = word >> 8;
}

#endif // NULIB_LITTLE_ENDIAN_H
