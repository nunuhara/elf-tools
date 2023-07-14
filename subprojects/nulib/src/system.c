/* Copyright (C) 2019 Nunuhara Cabbage <nunuhara@haniwa.technology>
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
#include <stdio.h>
#include <string.h>

#include "nulib.h"

bool sys_silent;

void (*sys_error_handler)(const char *msg) = NULL;

void *_xmalloc(size_t size, const char *func)
{
	void *ptr = malloc(size);
	if (!ptr) {
		sys_error("*ERROR*(%s): Out of memory\n", func);
	}
	return ptr;
}

void *_xcalloc(size_t nmemb, size_t size, const char *func)
{
	void *ptr = calloc(nmemb, size);
	if (!ptr) {
		sys_error("*ERROR*(%s): Out of memory\n", func);
	}
	return ptr;
}

void *_xrealloc(void *ptr, size_t size, const char *func)
{
	ptr = realloc(ptr, size);
	if (!ptr) {
		sys_error("*ERROR*(%s): Out of memory\n", func);
	}
	return ptr;
}

char *_xstrdup(const char *in, const char *func)
{
	char *out = strdup(in);
	if (!out) {
		sys_error("*ERROR*(%s): Out of memory\n", func);
	}
	return out;
}

_Noreturn void sys_verror(const char *fmt, va_list ap)
{
	if (sys_error_handler) {
		char msg[4096];
		(void)vsnprintf(msg, 4096, fmt, ap);
		sys_error_handler(msg);
	}
	vfprintf(stderr, fmt, ap);
	sys_exit(1);
}

_Noreturn void sys_error(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	sys_verror(fmt, ap);
	va_end(ap);
}

void sys_vwarning(const char *fmt, va_list ap)
{
	vfprintf(stderr, fmt, ap);
}

void sys_warning(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	sys_vwarning(fmt, ap);
	va_end(ap);
}

void sys_vmessage(const char *fmt, va_list ap)
{
	if (sys_silent)
		return;
	vprintf(fmt, ap);
	fflush(stdout);
}

void sys_message(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	sys_vmessage(fmt, ap);
	va_end(ap);
}

_Noreturn void sys_exit(int code)
{
	exit(code);
}
