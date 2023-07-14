/*
 * utfsjis.c -- utf-8/sjis related function
 *
 * Copyright (C) 1997 Yutaka OIWA <oiwa@is.s.u-tokyo.ac.jp>
 *
 * written for Satoshi KURAMOCHI's "eplaymidi"
 *                                   <satoshi@ueda.info.waseda.ac.jp>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nulib.h"
#include "nulib/string.h"
#include "nulib/utfsjis.h"
#include "s2utbl.h"

bool sjis_char_is_valid(const char *src)
{
	uint8_t b1 = *src;
	if (b1 < 0x7f)
		return true;
	uint8_t b2 = *(src + 1);
	if (b2 < 0x40)
		return false;
	if (s2u[b1 - 0x80][b2 - 0x40] == BAD_CH)
		return b1 == 0x81 && b2 == 0x45;
	return true;
}

// Convert a character-index to a byte-index for a given SJIS string.
int sjis_index(const char *_src, int index)
{
	const uint8_t *src = (uint8_t*)_src;
	int i, c;
	for (i = 0, c = 0; c < index && src[i]; i++, c++) {
		if (SJIS_2BYTE(src[i])) {
			i++;
			// check for invalid sjis data
			if (!src[i])
				return -1;
		}
	}
	return src[i] ? i : -1;
}

char *sjis_char2unicode(const char *_src, int *dst)
{
	const uint8_t *src = (const uint8_t*)_src;
	if (*src <= 0x7f) {
		*dst = *src;
		return (char*)src + 1;
	}

	if (*src >= 0xa0 && *src <= 0xdf) {
		*dst = 0xff60 + *src - 0xa0;
		return (char*)src + 1;
	}

	// guard against invalid byte sequence
	int c = *(src+1);
	if (c < 0x40 || c == 0x7f || c > 0xfc) {
		*dst = '?';
		return (char*)src + 1;
	}

	*dst = s2u[*src - 0x80][*(src+1) - 0x40];
	return (char*)src + 2;
}

static string sjis_to_utf8_internal(const char *_src, size_t len) {
	const uint8_t *src = (uint8_t*)_src;
	string dst = string_new_len(NULL, len * 3);
	uint8_t* dstp = (uint8_t*)dst;

	while (*src) {
		int c;
		src = (uint8_t*)sjis_char2unicode((const char*)src, &c);

		if (c <= 0x7f) {
			*dstp++ = c;
		} else if (c <= 0x7ff) {
			*dstp++ = 0xc0 | c >> 6;
			*dstp++ = 0x80 | (c & 0x3f);
		} else {
			*dstp++ = 0xe0 | c >> 12;
			*dstp++ = 0x80 | (c >> 6 & 0x3f);
			*dstp++ = 0x80 | (c & 0x3f);
		}
	}
	*dstp = '\0';
	sdssetlen(dst, dstp - (uint8_t*)dst);
	return dst;
}

string sjis_cstring_to_utf8(const char *src, size_t len)
{
	return sjis_to_utf8_internal(src, len ? len : strlen(src));
}

string sjis_to_utf8(string src)
{
	return sjis_to_utf8_internal(src, string_length(src));
}

static int unicode_to_sjis(int u) {
	for (int b1 = 0x81; b1 <= 0xff; b1++) {
		if (b1 >= 0xa0 && b1 <= 0xdf)
			continue;
		for (int b2 = 0x40; b2 <= 0xff; b2++) {
			if (u == s2u[b1 - 0x80][b2 - 0x40])
				return b1 << 8 | b2;
		}
	}
	return 0;
}

static string utf8_to_sjis_internal(const char *_src, size_t len) {
	const uint8_t *src = (uint8_t*)_src;
	string dst = string_new_len(NULL, len);
	uint8_t* dstp = (uint8_t*)dst;

	while (*src) {
		if (*src <= 0x7f) {
			*dstp++ = *src++;
			continue;
		}

		int u;
		if (*src <= 0xdf) {
			u = (src[0] & 0x1f) << 6 | (src[1] & 0x3f);
			src += 2;
		} else if (*src <= 0xef) {
			u = (src[0] & 0xf) << 12 | (src[1] & 0x3f) << 6 | (src[2] & 0x3f);
			src += 3;
		} else {
			*dstp++ = '?';
			do src++; while ((*src & 0xc0) == 0x80);
			continue;
		}

		if (u > 0xff60 && u <= 0xff9f) {
			*dstp++ = u - 0xff60 + 0xa0;
		} else {
			int c = unicode_to_sjis(u);
			if (c) {
				*dstp++ = c >> 8;
				*dstp++ = c & 0xff;
			} else {
				*dstp++ = '?';
			}
		}
	}
	*dstp = '\0';
	return dst;
}

string utf8_cstring_to_sjis(const char *src, size_t len)
{
	return utf8_to_sjis_internal(src, len ? len : strlen(src));
}

string utf8_to_sjis(string src)
{
	return utf8_to_sjis_internal(src, string_length(src));
}

/* src 内に半角カナもしくはASCII文字があるかどうか */
bool sjis_has_hankaku(const char *_src) {
	const uint8_t *src = (uint8_t*)_src;
	while(*src) {
		if (SJIS_2BYTE(*src)) {
			src++;
		} else {
			return true;
		}
		src++;
	}
	return false;
}

/* src 内に 全角文字があるかどうか */
bool sjis_has_zenkaku(const char *_src) {
	const uint8_t *src = (uint8_t*)_src;
	while(*src) {
		if (SJIS_2BYTE(*src)) {
			return true;
		}
		src++;
	}
	return false;
}

/* src 中の文字数を数える 全角文字も１文字 */
int sjis_count_char(const char *_src) {
	const uint8_t *src = (uint8_t*)_src;
	int c = 0;

	while(*src) {
		if (SJIS_2BYTE(*src)) {
			src++;
		}
		c++; src++;
	}
	return c;
}

/* SJIS(EUC) を含む文字列の ASCII を大文字化する */
void sjis_toupper(char *_src) {
	uint8_t *src = (uint8_t*)_src;
	while(*src) {
		if (SJIS_2BYTE(*src)) {
			src++;
		} else {
			if (*src >= 0x60 && *src <= 0x7a) {
				*src &= 0xdf;
			}
		}
		src++;
	}
}

/* SJIS を含む文字列の ASCII を大文字化する2 */
char *sjis_toupper2(const char *_src, size_t len) {
	const uint8_t *src = (uint8_t*)_src;
	uint8_t *dst;

	dst = malloc(len +1);
	if (dst == NULL) return NULL;
	strcpy((char*)dst, (char*)src);
	sjis_toupper((char*)dst);
	return (char*)dst;
}
