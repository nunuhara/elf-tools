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

#ifndef NULIB_FILE_H
#define NULIB_FILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>

#ifdef _WIN32
typedef _WDIR UDIR;
typedef struct _stat64 ustat;
#else
typedef DIR UDIR;
typedef struct stat ustat;
#endif

struct stat;

int closedir_utf8(UDIR *dir)
	attr_nonnull;

UDIR *opendir_utf8(const char *path)
	attr_dealloc(closedir_utf8, 1)
	attr_nonnull;

char *readdir_utf8(UDIR *dir)
	attr_nonnull;

int stat_utf8(const char *path, ustat *st)
	attr_warn_unused_result
	attr_nonnull;

char *realpath_utf8(const char *upath)
	attr_malloc
	attr_nonnull;

FILE *file_open_utf8(const char *path, const char *mode)
	attr_dealloc(fclose, 1)
	attr_nonnull;

void *file_read(const char *path, size_t *len_out)
	attr_malloc
	attr_nonnull_arg(1);

bool file_write(const char *path, uint8_t *data, size_t data_size)
	attr_warn_unused_result
	attr_nonnull;

bool file_copy(const char *src, const char *dst)
	attr_warn_unused_result
	attr_nonnull;

bool file_exists(const char *path)
	attr_warn_unused_result
	attr_nonnull;

off_t file_size(const char *path)
	attr_warn_unused_result
	attr_nonnull;

const char *file_extension(const char *path)
	attr_nonnull;

bool is_directory(const char *path)
	attr_warn_unused_result
	attr_nonnull;

int mkdir_p(const char *path)
	attr_warn_unused_result
	attr_nonnull;

char *path_dirname(const char *path)
	attr_nonnull;

char *path_basename(const char *path)
	attr_nonnull;

char *path_join(const char *dir, const char *base)
	attr_malloc
	attr_nonnull
	attr_returns_nonnull;

char *path_get_icase(const char *path)
	attr_malloc
	attr_nonnull;

#endif // NULIB_FILE_H
