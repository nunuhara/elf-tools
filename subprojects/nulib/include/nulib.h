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

#ifndef NULIB_H
#define NULIB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <assert.h>

// fix windows.h symbol conflict
#undef ERROR

#ifdef __cplusplus
#define _Noreturn [[ noreturn ]]
#endif

#define attr_const __attribute__((const))
#define attr_malloc __attribute__((malloc))
#define attr_alloc_size(...) __attribute__(( alloc_size (__VA_ARGS__)))
#define attr_unused __attribute__((unused))
#define attr_warn_unused_result __attribute__((warn_unused_result))
#define attr_nonnull_arg(...) __attribute__(( nonnull (__VA_ARGS__)))
#define attr_nonnull __attribute__((nonnull))
#define attr_returns_nonnull __attribute__((returns_nonnull))
#define attr_format(...) __attribute__(( format (__VA_ARGS__)))

#if defined(__clang__)
#define attr_dealloc(...)
#define attr_dealloc_free
#elif defined(__GNUC__)
#define attr_dealloc(...) __attribute__(( malloc (__VA_ARGS__)))
#define attr_dealloc_free attr_dealloc(__builtin_free, 1)
#endif

#define unlikely(x) __builtin_expect(x, 0)

#define ERROR(fmt, ...) \
	sys_error("*ERROR*(%s:%s:%d): " fmt "\n", __FILE__, __func__, __LINE__, ##__VA_ARGS__)

#define WARNING(fmt, ...) \
	sys_warning("*WARNING*(%s:%s:%d): " fmt "\n", __FILE__, __func__, __LINE__, ##__VA_ARGS__)

#define NOTICE(fmt, ...) \
	sys_message(fmt "\n", ##__VA_ARGS__)

extern bool sys_silent;
extern void (*sys_error_handler)(const char *msg);
_Noreturn void sys_verror(const char *fmt, va_list ap);
_Noreturn void sys_error(const char *fmt, ...) attr_format(printf, 1, 2);
void sys_vwarning(const char *fmt, va_list ap);
void sys_warning(const char *fmt, ...) attr_format(printf, 1, 2);
void sys_vmessage(const char *fmt, va_list ap);
void sys_message(const char *fmt, ...) attr_format(printf, 1, 2);

_Noreturn void sys_exit(int code);

#define xmalloc(size) _xmalloc(size, __func__)
void *_xmalloc(size_t size, const char *func)
	attr_malloc
	attr_alloc_size(1)
	attr_dealloc_free
	attr_returns_nonnull;

#define xcalloc(nmemb, size) _xcalloc(nmemb, size, __func__)
void *_xcalloc(size_t nmemb, size_t size, const char *func)
	attr_malloc
	attr_alloc_size(1, 2)
	attr_dealloc_free
	attr_returns_nonnull;

#define xrealloc(ptr, size) _xrealloc(ptr, size, __func__)
void *_xrealloc(void *ptr, size_t size, const char *func)
	attr_alloc_size(2)
	attr_dealloc_free
	attr_returns_nonnull;

#define xstrdup(str) _xstrdup(str, __func__)
char *_xstrdup(const char *in, const char *func)
	attr_malloc
	attr_dealloc_free
	attr_returns_nonnull;

#define max(a, b)				\
	({					\
		__typeof__ (a) _a = (a);	\
		__typeof__ (b) _b = (b);	\
		_a > _b ? _a : _b;		\
	})

#define min(a, b)				\
	({					\
		__typeof__ (a) _a = (a);	\
		__typeof__ (b) _b = (b);	\
		_a < _b ? _a : _b;		\
	})

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))

#endif // NULIB_H
