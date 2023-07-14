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

#include "nulib.h"
#include "nulib/hashset.h"

#if __SIZEOF_POINTER__ == 4
__KHASH_IMPL(hashset, kh_inline klib_unused, khint32_t, char, 0,
		kh_int_hash_func, kh_int_hash_equal);
#elif __SIZEOF_POINTER__ == 8
__KHASH_IMPL(hashset, kh_inline klib_unused, khint64_t, char, 0,
		kh_int64_hash_func, kh_int64_hash_equal);
#else
#error "Only 32- and 64-bit systems are supported"
#endif
