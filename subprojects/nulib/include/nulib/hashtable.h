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

#ifndef NULIB_HASHTABLE_H
#define NULIB_HASHTABLE_H

#include "nulib.h"

#define kmalloc xmalloc
#define krealloc xrealloc
#include "khash.h"

/* An example:

#include "nulib/hashtable.h"
declare_hashtable_int_type(32, char);
define_hashtable_int(32, char);
int main(void)
{
	int ret, is_missing;
	hashtable_iter_t k;
	hashtable_t(32) h = hashtable_initializer(32);
	k = hashtable_put(32, &h, 5, &ret);
	hashtable_val(&h, k) = 10;
	k = hashtable_get(32, &h, 10);
	is_missing = (k == hashtable_end(&h));
	k = hashtable_get(32, &h, 5);
	hashtable_delete(32, &h, k);
	for (k = hashtable_begin(&h); k != hashtable_end(&h); ++k) {
		if (hashtable_exists(&h, k))
			hashtable_val(&h, k) = 1;
	}
	hashtable_destroy(32, &h);
	return 0;
}
*/

typedef khiter_t hashtable_iter_t;

#define HASHTABLE_KEY_PRESENT 0

/*! @function
  @abstract     Declare a hash map containing integer keys
  @param  name  Name of the hash table [symbol]
  @param  khval_t  Type of values [type]
 */
#define declare_hashtable_int_type(name, khval_t) __KHASH_TYPE(name, khint32_t, khval_t)

/*! @function
  @abstract     Instantiate a hash map containing integer keys
  @param  name  Name of the hash table [symbol]
  @param  khval_t  Type of values [type]
 */
#define define_hashtable_int(name, khval_t) \
	__KHASH_IMPL(name, static kh_inline klib_unused, khint32_t, khval_t, 1, \
			kh_int_hash_func, kh_int_hash_equal)

/*! @function
  @abstract     Declare a hash map containing const char* keys
  @param  name  Name of the hash table [symbol]
  @param  khval_t  Type of values [type]
 */
#define declare_hashtable_string_type(name, khval_t) __KHASH_TYPE(name, kh_cstr_t, khval_t)

/*! @function
  @abstract     Instantiate a hash map containing const char* keys
  @param  name  Name of the hash table [symbol]
  @param  khval_t  Type of values [type]
 */
#define define_hashtable_string(name, khval_t) \
	__KHASH_IMPL(name, static kh_inline klib_unused, kh_cstr_t, khval_t, 1, \
			kh_str_hash_func, kh_str_hash_equal)

/*!
  @abstract Type of the hash table.
  @param  name  Name of the hash table [symbol]
 */
#define hashtable_t(name) khash_t(name)

/*! @function
  @abstract     Create a hash table initializer value.
  @param  name  Name of the hash table [symbol]
  @return       Initializer value for the hash table [khash_t(name)]
 */
#define hashtable_initializer(name) (kh_##name##_t){0}

/*! @function
  @abstract     Destroy a hash table.
  @param  name  Name of the hash table [symbol]
  @param  h     Pointer to the hash table [khash_t(name)*]
 */
#define hashtable_destroy(name, h) kh_destroy(name, h)

/*! @function
  @abstract     Reset a hash table without deallocating memory.
  @param  name  Name of the hash table [symbol]
  @param  h     Pointer to the hash table [khash_t(name)*]
 */
#define hashtable_clear(name, h) kh_clear(name, h)

/*! @function
  @abstract     Resize a hash table.
  @param  name  Name of the hash table [symbol]
  @param  h     Pointer to the hash table [khash_t(name)*]
  @param  s     New size [khint_t]
 */
#define hashtable_resize(name, h, s) kh_resize(name, h, s)

/*! @function
  @abstract     Insert a key to the hash table.
  @param  name  Name of the hash table [symbol]
  @param  h     Pointer to the hash table [khash_t(name)*]
  @param  k     Key [type of keys]
  @param  r     Extra return code:
                0 if the key is present in the hash table;
                1 if the bucket is empty (never used);
		2 if the element in the bucket has been deleted [int*]
  @return       Iterator to the inserted element [khint_t]
 */
#define hashtable_put(name, h, k, r) kh_put(name, h, k, r)

/*! @function
  @abstract     Retrieve a key from the hash table.
  @param  name  Name of the hash table [symbol]
  @param  h     Pointer to the hash table [khash_t(name)*]
  @param  k     Key [type of keys]
  @return       Iterator to the found element, or kh_end(h) if the element is absent [khint_t]
 */
#define hashtable_get(name, h, k) kh_get(name, h, k)

/*! @function
  @abstract     Remove a key from the hash table.
  @param  name  Name of the hash table [symbol]
  @param  h     Pointer to the hash table [khash_t(name)*]
  @param  k     Iterator to the element to be deleted [khint_t]
 */
#define hashtable_delete(name, h, k) kh_del(name, h, k)

/*! @function
  @abstract     Test whether a bucket contains data.
  @param  h     Pointer to the hash table [khash_t(name)*]
  @param  x     Iterator to the bucket [khint_t]
  @return       1 if containing data; 0 otherwise [int]
 */
#define hashtable_exists(h, x) kh_exist(h, x)

/*! @function
  @abstract     Get key given an iterator
  @param  h     Pointer to the hash table [khash_t(name)*]
  @param  x     Iterator to the bucket [khint_t]
  @return       Key [type of keys]
 */
#define hashtable_key(h, x) kh_key(h, x)

/*! @function
  @abstract     Get value given an iterator
  @param  h     Pointer to the hash table [khash_t(name)*]
  @param  x     Iterator to the bucket [khint_t]
  @return       Value [type of values]
  @discussion   For hash sets, calling this results in segfault.
 */
#define hashtable_val(h, x) kh_val(h, x)

/*! @function
  @abstract     Get the start iterator
  @param  h     Pointer to the hash table [khash_t(name)*]
  @return       The start iterator [khint_t]
 */
#define hashtable_begin(h) kh_begin(h)

/*! @function
  @abstract     Get the end iterator
  @param  h     Pointer to the hash table [khash_t(name)*]
  @return       The end iterator [khint_t]
 */
#define hashtable_end(h) kh_end(h)

/*! @function
  @abstract     Get the number of elements in the hash table
  @param  h     Pointer to the hash table [khash_t(name)*]
  @return       Number of elements in the hash table [khint_t]
 */
#define hashtable_size(h) kh_size(h)

/*! @function
  @abstract     Get the number of buckets in the hash table
  @param  h     Pointer to the hash table [khash_t(name)*]
  @return       Number of buckets in the hash table [khint_t]
 */
#define hashtable_nr_buckets(h) kh_n_buckets(h)

/*! @function
  @abstract     Iterate over the entries in the hash table
  @param  h     Pointer to the hash table [khash_t(name)*]
  @param  kvar  Variable to which key will be assigned
  @param  vvar  Variable to which value will be assigned
  @param  code  Block of code to execute
 */
#define hashtable_foreach(h, kvar, vvar, code) kh_foreach(h, kvar, vvar, code)

/*! @function
  @abstract     Iterate over the values in the hash table
  @param  h     Pointer to the hash table [khash_t(name)*]
  @param  vvar  Variable to which value will be assigned
  @param  code  Block of code to execute
 */
#define hashtable_foreach_value(h, vvar, code) kh_foreach_value(h, vvar, code)

#endif // NULIB_HASHTABLE_H
