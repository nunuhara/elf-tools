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

#ifndef NULIB_VECTOR_H
#define NULIB_VECTOR_H

/*
 * Wrapper around kvec (from klib). This just changes function names.
 */

#include "kvec.h"

/* An example:

#include "nulib/vector.h"
int main() {
	vector_t(int) array;
	vector_init(array);
	vector_push(int, array, 10); // append
	vector_a(int, array, 20) = 5; // dynamic
	vector_A(array, 20) = 4; // static
	vector_destroy(array);
	return 0;
}

*/

#define vector_initializer {0}

#define vector_t kvec_t
#define vector_init kv_init
#define vector_destroy kv_destroy
#define vector_a kv_a
#define vector_A kv_A
#define vector_pop kv_pop
#define vector_length kv_size
#define vector_empty(v) (vector_length(v) == 0)
#define vector_capacity kv_max
#define vector_set_capacity kv_resize
#define vector_push kv_push
#define vector_pushp kv_pushp

#define vector_resize(t, v, n) \
	do { \
		vector_set_capacity(t, v, n); \
		vector_length(v) = n; \
	} while (0)

#define vector_foreach(var, vec) \
	for (__typeof__(var) *__i = (vec).a; (__i - (vec).a < (vec).n) && (var = *__i, true); __i++)

#define vector_foreach_p(var, vec) \
	for (var = (vec).a; var - (vec).a < (vec).n; var++)

#endif // NULIB_VECTOR_H
