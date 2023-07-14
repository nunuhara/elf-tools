/*
 * utfsjis.h -- utf-8/sjis related function
 *
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
*/
/* $Id: eucsjis.h,v 1.2 2000/09/20 10:33:16 chikama Exp $ */

#ifndef NULIB_UTFSJIS
#define NULIB_UTFSJIS

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef char * string;

#define SJIS_2BYTE(b) ( ((b) & 0xe0) == 0x80 || ((b) & 0xe0) == 0xe0 )

extern string sjis_cstring_to_utf8(const char *src, size_t len);
extern string sjis_to_utf8(string src);
extern string utf8_cstring_to_sjis(const char *src, size_t len);
extern string utf8_to_sjis(string src);

extern bool sjis_char_is_valid(const char *src);
extern int   sjis_index(const char *src, int index);
extern char *sjis_char2unicode(const char *src, int *dst);
extern bool  sjis_has_hankaku(const char *src);
extern bool  sjis_has_zenkaku(const char *src);
extern int   sjis_count_char(const char *src);
extern void  sjis_toupper(char *src);
extern char* sjis_toupper2(const char *src, size_t len);

// Returns (big-endian) SJIS codepoint for first character in a string.
static inline uint16_t sjis_code(const char *_str)
{
	uint8_t *str = (uint8_t*)_str;
	if (SJIS_2BYTE(*str)) {
		return (*str << 8) | *(str+1);
	}
	return *str;
}

static inline char *sjis_skip_char(const char *str)
{
	return (char*)(str + (SJIS_2BYTE(*str) ? 2 : 1));
}

#endif // NULIB_UTFSJIS
