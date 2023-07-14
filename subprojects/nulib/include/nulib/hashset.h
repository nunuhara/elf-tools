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

#ifndef NULIB_HASHSET_H
#define NULIB_HASHSET_H

#include "nulib.h"

#define kmalloc xmalloc
#define krealloc xrealloc
#include "khash.h"

#if __SIZEOF_POINTER__ == 4
__KHASH_TYPE(hashset, khint32_t, char);
__KHASH_PROTOTYPES(hashset, khint32_t, char);
typedef khash_t(hashset) hashset_t;
typedef khint32_t khptr_t;
#elif __SIZEOF_POINTER__ == 8
__KHASH_TYPE(hashset, khint64_t, char);
__KHASH_PROTOTYPES(hashset, khint64_t, char);
typedef khash_t(hashset) hashset_t;
typedef khint64_t khptr_t;
#else
#error "Only 32- and 64-bit systems are supported"
#endif

#define hashset_initializer (hashset_t){0};
#define hashset_destroy(h) kh_destroy(hashset, h)
#define hashset_clear(h) kh_clear(hashset, h)

static inline bool hashset_exists(hashset_t *set, void *p)
{
	return kh_get(hashset, set, (khptr_t)p) != kh_end(set);
}

static inline void hashset_add(hashset_t *set, void *p)
{
	int ret;
	kh_put(hashset, set, (khptr_t)p, &ret);
}

static inline void hashset_delete(hashset_t *set, void *p)
{
	khiter_t k = kh_get(hashset, set, (khptr_t)p);
	if (k != kh_end(set))
		kh_del(hashset, set, k);
}

// a += b
static inline void hashset_union(hashset_t *a, hashset_t *b)
{
	for (khint_t i = kh_begin(b); i != kh_end(b); i++) {
		if (!kh_exist(b, i))
			continue;
		hashset_add(a, (void*)kh_key(b, i));
	}
}

// a -= b
static inline void hashset_subtract(hashset_t *a, hashset_t *b)
{
	for (khint_t i = kh_begin(b); i != kh_end(b); i++) {
		if (!kh_exist(b, i))
			continue;
		hashset_delete(a, (void*)kh_key(b, i));
	}
}

#define hashset_foreach(h, kvar, code) kh_foreach_key(h, kvar, code)

#endif // NULIB_HASHSET_H
