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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "nulib.h"
#include "nulib/file.h"
#include "nulib/hashtable.h"
#include "nulib/little_endian.h"
#include "nulib/lzss.h"
#include "nulib/string.h"
#include "nulib/utfsjis.h"
#include "nulib/vector.h"

#include "arc.h"

#define MAX_SANE_FILES 100000

define_hashtable_string(arcindex, int);

static bool read_u32(FILE *fp, uint32_t *out)
{
	uint8_t buf[4];
	if (fread(buf, 4, 1, fp) != 1) {
		WARNING("fread: %s", strerror(errno));
		return false;
	}
	*out = le_get32(buf, 0);
	return true;
}

static bool read_u32_at(FILE *fp, off_t offset, uint32_t *out)
{
	if (fseek(fp, offset, SEEK_SET)) {
		WARNING("fseek: %s", strerror(errno));
		return false;
	}
	return read_u32(fp, out);
}

static bool read_u8_at(FILE *fp, off_t offset, uint8_t *out)
{
	if (fseek(fp, offset, SEEK_SET)) {
		WARNING("fseek: %s", strerror(errno));
		return false;
	}
	if (fread(out, 1, 1, fp) != 1) {
		WARNING("fread: %s", strerror(errno));
		return false;
	}
	return true;
}

static bool archive_read_index(FILE *fp, struct archive *arc, off_t arc_size)
{
	// read file count
	uint32_t nr_files;
	if (!read_u32(fp, &nr_files))
		return false;
	if (nr_files > MAX_SANE_FILES) {
		WARNING("archive file count is not sane: %u", nr_files);
		return false;
	}

	// determine encryption scheme
	const unsigned name_lengths[] = { 0x14, 0x1e, 0x20, 0x100 };
	uint32_t name_length = 0;
	uint32_t offset_key = 0;
	uint32_t size_key = 0;
	uint8_t name_key = 0;
	for (int i = 0; i < ARRAY_SIZE(name_lengths); i++) {
		uint32_t first_size, first_offset, second_offset;
		if (!read_u8_at(fp, 3 + name_lengths[i], &name_key))
			return false;
		if (!read_u32_at(fp, 4 + name_lengths[i], &first_size))
			return false;
		if (!read_u32_at(fp, 8 + name_lengths[i], &first_offset))
			return false;
		if (!read_u32_at(fp, (off_t)(8 + name_lengths[i]) * 2, &second_offset))
			return false;

		uint32_t data_offset = (name_lengths[i] + 8) * nr_files + 4;
		offset_key = data_offset ^ first_offset;
		second_offset ^= offset_key;
		if (second_offset < data_offset || second_offset >= arc_size)
			continue;

		size_key = (second_offset - data_offset) ^ first_size;
		if (offset_key && size_key) {
			name_length = name_lengths[i];
			break;
		}
	}
	if (!name_length)
		return false;

	if (fseek(fp, 4, SEEK_SET)) {
		WARNING("fseek: %s", strerror(errno));
		return false;
	}

	const size_t buf_len = (size_t)nr_files * (name_length + 8);
	size_t buf_pos = 0;
	uint8_t *buf = xmalloc(buf_len);
	if (fread(buf, buf_len, 1, fp) != 1) {
		WARNING("fread: %s", strerror(errno));
		return false;
	}

	// read file entries
	vector_init(arc->files);
	vector_resize(struct archive_data, arc->files, nr_files);
	for (int i = 0; i < nr_files; i++) {
		// decode file name
		for (int j = 0; j < name_length; j++) {
			buf[buf_pos + j] ^= name_key;
			if (buf[buf_pos + j] == 0)
				break;
		}
		buf[buf_pos + name_length - 1] = 0;

		uint32_t offset = le_get32(buf, buf_pos + name_length + 4) ^ offset_key;
		uint32_t raw_size = le_get32(buf, buf_pos + name_length) ^ size_key;
		string name = sjis_cstring_to_utf8((char*)buf + buf_pos, 0);
		if (offset + raw_size > arc_size) {
			ERROR("%s @ %u + %u extends beyond EOF (%u)", name, offset, raw_size, (unsigned)arc_size);
		}

		// store entry
		vector_A(arc->files, i) = (struct archive_data) {
			.offset = offset,
			.raw_size = raw_size,
			.name = name,
			.data = NULL,
			.ref = 0,
			.archive = arc
		};
		buf_pos += name_length + 8;
	}
	free(buf);

	// create index
	for (int i = 0; i < vector_length(arc->files); i++) {
		int ret;
		hashtable_iter_t k = hashtable_put(arcindex, &arc->index,
				vector_A(arc->files, i).name,
				&ret);
		if (ret == HASHTABLE_KEY_PRESENT) {
			WARNING("skipping duplicate file name in archive");
			continue;
		}
		hashtable_val(&arc->index, k) = i;
	}

	return true;
}

struct archive *archive_open(const char *path, unsigned flags)
{
	FILE *fp = NULL;
	struct archive *arc = xcalloc(1, sizeof(struct archive));

	// open archive file
	if (!(fp = file_open_utf8(path, "rb"))) {
		WARNING("file_open_utf8: %s", strerror(errno));
		goto error;
	}

	// get size of archive
	if (fseek(fp, 0, SEEK_END)) {
		WARNING("fseek: %s", strerror(errno));
		goto error;
	}
	off_t arc_size = ftell(fp);
	if (fseek(fp, 0, SEEK_SET)) {
		WARNING("fseek: %s", strerror(errno));
		goto error;
	}

	// read file index
	if (!archive_read_index(fp, arc, arc_size)) {
		goto error;
	}

	// store either mmap ptr/size or FILE* depending on flags
	if (flags & ARCHIVE_MMAP) {
		int fd = fileno(fp);
		if (fd == -1) {
			WARNING("fileno: %s", strerror(errno));
			goto error;
		}
		arc->map.data = mmap(0, arc_size, PROT_READ, MAP_SHARED, fd, 0);
		arc->map.size = arc_size;
		if (arc->map.data == MAP_FAILED) {
			WARNING("mmap: %s", strerror(errno));
			goto error;
		}
		if (fclose(fp)) {
			WARNING("fclose: %s", strerror(errno));
			goto error;
		}
		arc->mapped = true;
	} else {
		arc->fp = fp;
	}

	return arc;
error:
	vector_destroy(arc->files);
	free(arc);
	if (fp && fclose(fp))
		WARNING("fclose: %s", strerror(errno));
	return NULL;
}

void archive_close(struct archive *arc)
{
	if (arc->mapped) {
		if (munmap(arc->map.data, arc->map.size))
			WARNING("munmap: %s", strerror(errno));
	} else {
		if (fclose(arc->fp))
			WARNING("fclose: %s", strerror(errno));
	}
	for (unsigned i = 0; i < vector_length(arc->files); i++) {
		string_free(vector_A(arc->files, i).name);
	}
	vector_destroy(arc->files);
	hashtable_destroy(arcindex, &arc->index);
	free(arc);
}

bool archive_data_load(struct archive_data *data)
{
	// data already loaded by another caller
	if (data->ref) {
		data->ref++;
		return true;
	}

	assert(!data->data);

	// load data
	if (data->archive->mapped) {
		data->data = data->archive->map.data + data->offset;
		data->size = data->raw_size;
		data->mapped = true;
	} else {
		if (fseek(data->archive->fp, data->offset, SEEK_SET)) {
			WARNING("fseek: %s", strerror(errno));
			return false;
		}
		data->data = xmalloc(data->raw_size);
		if (fread(data->data, data->raw_size, 1, data->archive->fp) != 1) {
			WARNING("fread: %s", strerror(errno));
			free(data->data);
			return false;
		}
		data->size = data->raw_size;
	}

	// decompress compessed file types
	const char *ext = file_extension(data->name);
	static const char *compressed_ext[] = { "mes", "lib", "a", "a6", "msk", "x" };
	for (unsigned i = 0; i < ARRAY_SIZE(compressed_ext); i++) {
		if (strcasecmp(ext, compressed_ext[i]))
			continue;
		size_t decompressed_size;
		uint8_t *tmp = lzss_decompress(data->data, data->raw_size, &decompressed_size);
		if (!data->mapped)
			free(data->data);
		data->mapped = false;
		if (!tmp) {
			WARNING("lzss_decompress failed");
			data->data = NULL;
			data->size = 0;
			return false;
		}
		data->data = tmp;
		data->size = decompressed_size;
		break;
	}

	data->ref = 1;
	return true;
}

struct archive_data *archive_get(struct archive *arc, const char *name)
{
	hashtable_iter_t k = hashtable_get(arcindex, &arc->index, name);
	if (k == hashtable_end(&arc->index)) {
		return NULL;
	}

	// XXX: Because we're giving out pointers into arc->files, the vector must
	//      never be resized.
	struct archive_data *data = &vector_A(arc->files, hashtable_val(&arc->index, k));
	if (archive_data_load(data))
		return data;
	return NULL;
}

struct archive_data *archive_get_by_index(struct archive *arc, unsigned i)
{
	if (i >= vector_length(arc->files))
		return NULL;

	struct archive_data *data = &vector_A(arc->files, i);
	if (archive_data_load(data))
		return data;
	return NULL;
}

void archive_data_release(struct archive_data *data)
{
	if (data->ref == 0)
		ERROR("double-free of archive data");
	if (--data->ref == 0) {
		if (!data->mapped)
			free(data->data);
		data->data = NULL;
		data->size = 0;
	}
}
