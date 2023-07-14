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

#ifndef NULIB_STRING_H
#define NULIB_STRING_H

/*
 * Wrapper around sds string library. This just changes function names and
 * consolidates the documentation.
 */
#include "sds.h"

typedef sds string;

#define STRING_NOINIT SDS_NOINIT

/*
 * Get the length of a string.
 */
#define string_length sdslen
size_t string_length(const string s);

/*
 * Create a new string with the content specified by the 'init' pointer
 * and 'initlen'.
 * If NULL is used for 'init' the string is initialized with zero bytes.
 * If STRING_NOINIT is used, the buffer is left uninitialized;
 *
 * The string is always null-terminated (all strings are, always) so
 * even if you create a string with:
 *
 * mystring = string_new_len("abc",3);
 *
 * You can print the string with printf() as there is an implicit \0 at the
 * end of the string. However the string is binary safe and can contain
 * \0 characters in the middle, as the length is stored in the string header.
 */
#define string_new_len sdsnewlen
string string_new_len(const void *init, size_t initlen);

/*
 * Create an empty (zero length) string. Even in this case the string
 * always has an implicit null term.
 */
#define string_empty sdsempty
string string_empty(void);

/*
 * Duplicate a string.
 */
#define string_dup sdsdup
string string_dup(const string s);

/*
 * Free a string. No operation is performed if 's' is NULL.
 */
#define string_free sdsfree
void string_free(string s);

/*
 * Grow the string to have the specified length. Bytes that were not part of
 * the original length of the string will be set to zero.
 *
 * if the specified length is smaller than the current length, no operation
 * is performed.
 */
#define string_grow sdsgrowzero
string string_grow(string s, size_t len);

/*
 * Append the specified binary-safe string pointed by 't' of 'len' bytes to the
 * end of the specified string 's'.
 *
 * After the call, the passed string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call.
 */
#define string_concat_len sdscatlen
string string_concat_len(string s, const void *t, size_t len);

/*
 * Append the specified null termianted C string to the string 's'.
 *
 * After the call, the passed string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call.
 */
#define string_concat_cstring sdscat
string string_concat_cstring(string s, const char *t);

/*
 * Append the specified string 't' to the existing string 's'.
 *
 * After the call, the modified string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call.
 */
#define string_concat sdscatsds
string string_concat(string s, const string t);

/*
 * Destructively modify the string 's' to hold the specified binary
 * safe string pointed by 't' of length 'len' bytes.
 */
#define string_copy_len sdscpylen
string string_copy_len(string s, const char *t, size_t len);

/*
 * Like string_copy_len() but 't' must be a null-terminated string so that the length
 * of the string is obtained with strlen().
 */
#define string_copy_cstring sdscpy
string string_copy_cstring(string s, const char *t);

/*
 * Like string_concat_printf() but gets va_list instead of being variadic.
 */
#define string_concat_vprintf sdscatvprintf
string string_concat_vprintf(string s, const char *fmt, va_list ap);

/*
 * Append to the string 's' a string obtained using printf-alike format
 * specifier.
 *
 * After the call, the modified string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call.
 *
 * Example:
 *
 * s = string_new("Sum is: ");
 * s = string_concat_printf(s,"%d+%d = %d",a,b,a+b).
 *
 * Often you need to create a string from scratch with the printf-alike
 * format. When this is the need, just use string_empty() as the target string:
 *
 * s = string_concat_printf(string_empty(), "... your format ...", args);
 */
#define string_concat_printf sdscatprintf
string string_concat_printf(string s, const char *fmt, ...);

/*
 * This function is similar to string_concat_printf, but much faster as it does
 * not rely on sprintf() family functions implemented by the libc that
 * are often very slow. Moreover directly handling the string as
 * new data is concatenated provides a performance improvement.
 *
 * However this function only handles an incompatible subset of printf-alike
 * format specifiers:
 *
 * %s - C String
 * %S - string
 * %i - signed int
 * %I - 64 bit signed integer (long long, int64_t)
 * %u - unsigned int
 * %U - 64 bit unsigned integer (unsigned long long, uint64_t)
 * %% - Verbatim "%" character.
 */
#define string_concat_fmt sdscatfmt
string string_concat_fmt(string s, char const *fmt, ...);

/*
 * Remove the part of the string from left and from right composed just of
 * contiguous characters found in 'cset', that is a null terminated C string.
 *
 * After the call, the modified string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call.
 *
 * Example:
 *
 * s = string_new("AA...AA.a.aa.aHelloWorld     :::");
 * s = string_trim(s,"Aa. :");
 * printf("%s\n", s);
 *
 * Output will be just "HelloWorld".
 */
#define string_trim sdstrim
string string_trim(string s, const char *cset);

/*
 * Turn the string into a smaller (or equal) string containing only the
 * substring specified by the 'start' and 'end' indexes.
 *
 * start and end can be negative, where -1 means the last character of the
 * string, -2 the penultimate character, and so forth.
 *
 * The interval is inclusive, so the start and end characters will be part
 * of the resulting string.
 *
 * The string is modified in-place.
 *
 * Example:
 *
 * s = string_new("Hello World");
 * string_range(s,1,-1); => "ello World"
 */
#define string_range sdsrange
void string_range(string s, ssize_t start, ssize_t end);

/*
 * Set the string length to the length as obtained with strlen(), so
 * considering as content only up to the first null term character.
 *
 * This function is useful when the string is hacked manually in some
 * way, like in the following example:
 *
 * s = string_new("foobar");
 * s[2] = '\0';
 * string_update_len(s);
 * printf("%d\n", string_length(s));
 *
 * The output will be "2", but if we comment out the call to string_update_len()
 * the output will be "6" as the string was modified but the logical length
 * remains 6 bytes.
 */
#define string_update_len sdsupdatelen
void string_update_len(string s);

/*
 * Modify a string in-place to make it empty (zero length).
 * However all the existing buffer is not discarded but set as free space
 * so that next append operations will not require allocations up to the
 * number of bytes previously available.
 */
#define string_clear sdsclear
void string_clear(string s);

/*
 * Compare two strings s1 and s2 with memcmp().
 *
 * Return value:
 *
 *     positive if s1 > s2.
 *     negative if s1 < s2.
 *     0 if s1 and s2 are exactly the same binary string.
 *
 * If two strings share exactly the same prefix, but one of the two has
 * additional characters, the longer string is considered to be greater than
 * the smaller one. */
#define string_compare sdscmp
int string_compare(const string s1, const string s2);

/*
 * Split 's' with separator in 'sep'. An array
 * of strings is returned. *count will be set
 * by reference to the number of tokens returned.
 *
 * On out of memory, zero length string, zero length
 * separator, NULL is returned.
 *
 * Note that 'sep' is able to split a string using
 * a multi-character separator. For example
 * sdssplit("foo_-_bar","_-_"); will return two
 * elements "foo" and "bar".
 */
#define string_split sdssplitlen
string *string_split(const char *s, ssize_t len, const char *sep, int seplen, int *count);

/*
 * Free the result returned by string_split(), or do nothing if 'tokens' is NULL.
 */
#define string_free_split_result sdsfreesplitres
void string_free_split_result(string *tokens, int count);

/*
 * Apply tolower() to every character of the string 's'.
 */
#define string_tolower sdstolower
void string_tolower(string s);

/*
 * Apply toupper() to every character of the string 's'.
 */
#define string_toupper sdstoupper
void string_toupper(string s);

/*
 * Append to the string "s" an escaped string representation where
 * all the non-printable characters (tested with isprint()) are turned into
 * escapes in the form "\n\r\a...." or "\x<hex-number>".
 *
 * After the call, the modified string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
#define string_escape sdscatrepr
string string_escape(string s, const char *p, size_t len);

/*
 * Split a line into arguments, where every argument can be in the
 * following programming-language REPL-alike form:
 *
 * foo bar "newline are supported\n" and "\xff\x00otherstuff"
 *
 * The number of arguments is stored into *argc, and an array
 * of strings is returned.
 *
 * The caller should free the resulting array of strings with
 * string_free_split_result().
 *
 * Note that string_escape() is able to convert back a string into
 * a quoted string in the same format string_split_args() is able to parse.
 *
 * The function returns the allocated tokens on success, even when the
 * input string is empty, or NULL if the input contains unbalanced
 * quotes or closed quotes followed by non space characters
 * as in: "foo"bar or "foo'
 */
#define string_split_args sdssplitargs
string *string_split_args(const char *line, int *argc);

/*
 * Modify the string substituting all the occurrences of the set of
 * characters specified in the 'from' string to the corresponding character
 * in the 'to' array.
 *
 * For instance: string_map_chars(mystring, "ho", "01", 2)
 * will have the effect of turning the string "hello" into "0ell1".
 *
 * The function returns the string pointer, that is always the same
 * as the input pointer since no resize is needed.
 */
#define string_map_chars sdsmapchars
string string_map_chars(string s, const char *from, const char *to, size_t setlen);

/*
 * Join an array of C strings using the specified separator (also a C string).
 * Returns the result as a nulib string.
 */
#define string_join_cstring sdsjoin
string string_join_cstring(char **argv, int argc, char *sep);

/*
 * Like sdsjoin, but joins an array of nulib strings.
 */
#define string_join sdsjoinsds
string string_join(string *argv, int argc, const char *sep, size_t seplen);

#endif // NULIB_STRING_H
