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

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <Windows.h>
#include <direct.h>
#else
#include <libgen.h>
#endif

#include "nulib.h"
#include "nulib/file.h"

#ifdef _WIN32
static wchar_t *utf8_to_wchar(const char *str)
{
	int nr_wchars = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
	wchar_t *wstr = xmalloc(nr_wchars * sizeof(wchar_t));
	MultiByteToWideChar(CP_UTF8, 0, str, -1, wstr, nr_wchars);
	return wstr;
}

static char *wchar_to_utf8(const wchar_t *wstr)
{
	int nr_chars = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
	char *str = xmalloc(nr_chars);
	WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str, nr_chars, NULL, NULL);
	return str;
}

static int make_dir(const char *path, possibly_unused int mode)
{
	wchar_t *wpath = utf8_to_wchar(path);
	int r = _wmkdir(wpath);
	free(wpath);
	return r;
}

UDIR *opendir_utf8(const char *path)
{
	wchar_t *wpath = utf8_to_wchar(path);
	UDIR *r = _wopendir(wpath);
	free(wpath);
	return r;
}

int closedir_utf8(UDIR *dir)
{
	return _wclosedir(dir);
}

char *readdir_utf8(UDIR *dir)
{
	struct _wdirent *e = _wreaddir(dir);
	if (!e)
		return NULL;
	return wchar_to_utf8(e->d_name);
}

int stat_utf8(const char *path, ustat *st)
{
	wchar_t *wpath = utf8_to_wchar(path);
	int r = _wstat64(wpath, st);
	free(wpath);
	return r;
}

char *realpath_utf8(const char *upath)
{
	wchar_t realpath[PATH_MAX];
	wchar_t *relpath = utf8_to_wchar(upath);
	if (!_wfullpath(realpath, relpath, PATH_MAX)) {
		return NULL;
	}
	return wchar_to_utf8(realpath);
}

FILE *file_open_utf8(const char *path, const char *mode)
{
	wchar_t *wpath = utf8_to_wchar(path);
	wchar_t wmode[64];
	mbstowcs(wmode, mode, 64);

	FILE *f = _wfopen(wpath, wmode);
	free(wpath);
	return f;
}

static inline bool is_path_separator(char c)
{
	return c == '/' || c == '\\';
}

char *path_dirname(const char *path)
{
	static char buf[PATH_MAX];
	strncpy(buf, path, PATH_MAX-1);

	char *s = (isalpha(buf[0]) && buf[1] == ':') ? buf + 2 : buf;
	if (!*s)
		return ".";

	int i = strlen(s) - 1;
	if (is_path_separator(s[i])) {
		if (!i)
			return buf;
		i--;
	}
	while (!is_path_separator(s[i])) {
		if (!i) {
			strcpy(s, ".");
			return buf;
		}
		i--;
	}
	if (i && is_path_separator(s[i]))
		i--;
	s[i+1] = '\0';
	return buf;
}

char *path_basename(const char *path)
{
	if (isalpha(path[0]) && path[1] == ':')
		path += 2;
	if (!*path)
		return ".";

	static char buf[PATH_MAX];
	strncpy(buf, path, PATH_MAX-1);
	int i = strlen(buf) - 1;
	if (i && is_path_separator(buf[i]))
		buf[i--] = '\0';
	while (i && !is_path_separator(buf[i-1]))
		i--;
	return buf + i;
}

#else

#define make_dir(path, mode) mkdir(path, mode)

UDIR *opendir_utf8(const char *path)
{
	return opendir(path);
}

int closedir_utf8(UDIR *dir)
{
	return closedir(dir);
}

char *readdir_utf8(UDIR *dir)
{
	struct dirent *e = readdir(dir);
	if (!e)
		return NULL;
	return strdup(e->d_name);
}

int stat_utf8(const char *path, ustat *st)
{
	return stat(path, st);
}

char *realpath_utf8(const char *upath)
{
	return realpath(upath, NULL);
}

FILE *file_open_utf8(const char *path, const char *mode)
{
	return fopen(path, mode);
}

char *path_dirname(const char *path)
{
	static char buf[PATH_MAX];
	strncpy(buf, path, PATH_MAX-1);
	return dirname(buf);
}

char *path_basename(const char *path)
{
	static char buf[PATH_MAX];
	strncpy(buf, path, PATH_MAX-1);
	return basename(buf);
}
#endif

void *file_read(const char *path, size_t *len_out)
{
	long len;
	FILE *fp = NULL;
	uint8_t *buf = NULL;

	ustat s;
	if (stat_utf8(path, &s) < 0) {
		WARNING("stat_utf8: %s", strerror(errno));
		goto error;
	}
	if (!S_ISREG(s.st_mode)) {
		WARNING("'%s' is not a regular file", path);
		goto error;
	}
	if (!(fp = file_open_utf8(path, "rb"))) {
		WARNING("file_open_utf8: %s", strerror(errno));
		goto error;
	}

	if (fseek(fp, 0, SEEK_END) < 0) {
		WARNING("fseek: %s", strerror(errno));
		goto error;
	}
	if ((len = ftell(fp)) < 0) {
		WARNING("ftell: %s", strerror(errno));
		goto error;
	}
	if (fseek(fp, 0, SEEK_SET) < 0) {
		WARNING("fseek: %s", strerror(errno));
		goto error;
	}

	buf = xmalloc(len + 1);
	// codechecker_suppress [alpha.unix.Stream]
	if (fread(buf, len, 1, fp) != 1) {
		WARNING("fread: %s", strerror(errno));
		goto error;
	}
	if (fclose(fp))
		WARNING("fclose: %s", strerror(errno));

	if (len_out)
		*len_out = len;

	buf[len] = 0;
	return buf;
error:
	if (fp && fclose(fp))
		WARNING("fclose: %s", strerror(errno));
	free(buf);
	return NULL;
}

bool file_write(const char *path, uint8_t *data, size_t data_size)
{
	FILE *fp = file_open_utf8(path, "wb");
	if (!fp)
		return false;
	int r = fwrite(data, data_size, 1, fp);
	int tmp = errno;
	fclose(fp);
	errno = tmp;
	return r == 1;
}

bool file_copy(const char *src, const char *dst)
{
	size_t data_size;
	uint8_t *data = file_read(src, &data_size);
	if (!data)
		return false;
	bool r = file_write(dst, data, data_size);
	free(data);
	return r;
}

bool file_exists(const char *path)
{
#ifdef _WIN32
	wchar_t *wpath = utf8_to_wchar(path);
	bool r = _waccess(wpath, F_OK) != -1;
	free(wpath);
	return r;
#else
	return access(path, F_OK) != -1;
#endif
}

char *path_join(const char *dir, const char *base)
{
	size_t dir_len = strlen(dir);
	size_t base_len = strlen(base);
	if (!dir_len)
		return xstrdup(base);

	int need_sep = dir[dir_len-1] != '/' ? 1 : 0;
	char *path = xmalloc(dir_len + need_sep + base_len + 1);
	memcpy(path, dir, dir_len);
	if (need_sep)
		path[dir_len] = '/';
	memcpy(path + dir_len + need_sep, base, base_len+1);
	return path;
}

char *path_get_icase(const char *path)
{
	const char *dir_name = path_dirname(path);
	const char *base_name = path_basename(path);
	DIR *d = opendir(dir_name);
	if (!d) {
		// FIXME: shouldn't assume case of dir_name is correct
		return NULL;
	}

	struct dirent *dir;
	while ((dir = readdir(d)) != NULL) {
		if (!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..")) {
			continue;
		}

		if (!strcasecmp(dir->d_name, base_name)) {
			char *r = path_join(dir_name, dir->d_name);
			closedir(d);
			return r;
		}
	}

	closedir(d);
	return NULL;
}

const char *file_extension(const char *path)
{
	const char *ext = strrchr(path, '.');
	return ext ? ext+1 : NULL;
}

bool is_directory(const char *path)
{
	ustat s;
	if (stat_utf8(path, &s) < 0)
		return -1;
	return S_ISDIR(s.st_mode);
}

off_t file_size(const char *path)
{
	ustat s;
	if (stat_utf8(path, &s) < 0)
		return -1;
	if (!S_ISREG(s.st_mode))
		return -1;
	return s.st_size;
}

// Adapted from http://stackoverflow.com/a/2336245/119527
int mkdir_p(const char *path)
{
	const size_t len = strlen(path);
	char _path[PATH_MAX];
	char *p;

	errno = 0;

	// Copy string so its mutable
	if (len > sizeof(_path)-1) {
		errno = ENAMETOOLONG;
		return -1;
	}
	strcpy(_path, path);

	// Iterate the string
	for (p = _path + 1; *p; p++) {
		if (*p == '/') {
			// Temporarily truncate
			*p = '\0';

			if (make_dir(_path, S_IRWXU) != 0) {
				if (errno != EEXIST)
					return -1;
			}

			*p = '/';
		}
	}

	if (make_dir(_path, S_IRWXU) != 0) {
		if (errno != EEXIST)
			return -1;
	}

	return 0;
}
