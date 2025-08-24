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
#include <ctype.h>

#include "nulib.h"
#include "nulib/buffer.h"
#include "nulib/file.h"
#include "nulib/port.h"
#include "ai5/lzss.h"
#include "ai5/game.h"

#include "cli.h"
#include "arc.h"

enum arc_file_type {
	ARC_FILE_FS,
	ARC_FILE_MEM,
	ARC_FILE_ARCDATA,
};

struct arc_file {
	enum arc_file_type type;
	union {
		struct { string name; string path; } fs;
		struct { string name; uint8_t *data; size_t size; } mem;
		struct archive_data *arcdata;
	};
	uint32_t packed_offset;
	uint32_t packed_size;
};

static string arc_file_name(struct arc_file *f)
{
	switch (f->type) {
	case ARC_FILE_FS: return f->fs.path;
	case ARC_FILE_MEM: return f->mem.name;
	case ARC_FILE_ARCDATA: return f->arcdata->name;
	}
	ERROR("invalid arc_file type");
}

static void arc_file_free(struct arc_file *f)
{
	switch (f->type) {
	case ARC_FILE_FS:
		string_free(f->fs.path);
		break;
	case ARC_FILE_MEM:
		string_free(f->mem.name);
		free(f->mem.data);
		break;
	case ARC_FILE_ARCDATA:
		break;
	}
}

typedef vector_t(struct arc_file) arc_file_list;

static void arc_file_list_free(arc_file_list list)
{
	struct arc_file *f;
	vector_foreach_p(f, list) {
		arc_file_free(f);
	}
	vector_destroy(list);
}

void write_u32(struct port *out, uint32_t v)
{
	if (!port_write_u32(out, v))
		sys_error("Write failure: %s\n", strerror(errno));
}

void write_bytes(struct port *out, uint8_t *data, size_t size)
{
	if (!port_write_bytes(out, data, size))
		sys_error("Write failure: %s\n", strerror(errno));
}

static void arc_seek(struct port *out, unsigned off)
{
	if (!port_seek(out, off))
		sys_error("Seek failure: %s\n", strerror(errno));
}

static void arc_file_fs_write(struct port *out, struct arc_file *f)
{
	size_t size;
	uint8_t *data = file_read(f->fs.path, &size);
	write_bytes(out, data, size);
	free(data);
}

static void arc_file_mem_write(struct port *out, struct arc_file *f)
{
	write_bytes(out, f->mem.data, f->mem.size);
}

static void arc_file_arcdata_write(struct port *out, struct arc_file *f)
{
	if (!archive_data_load(f->arcdata))
		sys_error("Failed to load file from archive\n");
	write_bytes(out, f->arcdata->data, f->arcdata->size);
	archive_data_release(f->arcdata);
}

static void arc_file_write(struct port *out, struct arc_file *f)
{
	f->packed_offset = port_tell(out);

	switch (f->type) {
	case ARC_FILE_FS: arc_file_fs_write(out, f); break;
	case ARC_FILE_MEM: arc_file_mem_write(out, f); break;
	case ARC_FILE_ARCDATA: arc_file_arcdata_write(out, f); break;
	}

	f->packed_size = port_tell(out) - f->packed_offset;
}

static void kakyuusei_write_index(struct port *out, arc_file_list files)
{
	static uint8_t shuffle_table[20] = {
		17, 2, 8, 19, 0, 5, 10, 13, 1, 15, 6, 4, 11, 16, 3, 9, 18, 12, 7, 14
	};

	uint8_t enc[20];
	uint8_t raw[20];
	uint8_t key = vector_length(files);
	struct arc_file *f;
	vector_foreach_p(f, files) {
		string name = arc_file_name(f);
		for (int i = 0; i < 12; i++) {
			raw[i] = i < string_length(name) ? name[i] : 0;
		}
		le_put32(raw, 12, f->packed_size);
		le_put32(raw, 16, f->packed_offset);

		for (int i = 0; i < 20; i++) {
			enc[i] = raw[shuffle_table[i]] ^ key;
			key = ((int)key * 3 + 1) & 0xff;
		}
		write_bytes(out, enc, 20);
	}
}

static bool arc_write_kakyuusei(const char *path, arc_file_list files)
{
	struct port out;
	if (!port_file_open(&out, path))
		sys_error("Failed to open \"%s\": %s\n", path, strerror(errno));

	write_u32(&out, vector_length(files));
	arc_seek(&out, 4 + vector_length(files) * 20);

	struct arc_file *f;
	vector_foreach_p(f, files) {
		arc_file_write(&out, f);
	}

	arc_seek(&out, 4);
	kakyuusei_write_index(&out, files);
	return true;
}

static void write_entry_name(struct port *out, struct arc_file *f, struct arc_metadata *meta)
{
	uint8_t e_name[256];
	string name = arc_file_name(f);

	int i;
	for (i = 0; name[i]; i++) {
		e_name[i] = name[i] ^ meta->name_key;
	}
	for (; i < meta->name_length; i++) {
		e_name[i] = meta->name_key;
	}
	write_bytes(out, e_name, meta->name_length);
}

static void write_entry_NSO(struct port *out, struct arc_file *f, struct arc_metadata *meta)
{
	write_entry_name(out, f, meta);
	write_u32(out, f->packed_size ^ meta->size_key);
	write_u32(out, f->packed_offset ^ meta->size_key);
}

static void write_entry_NOS(struct port *out, struct arc_file *f, struct arc_metadata *meta)
{
	write_entry_name(out, f, meta);
	write_u32(out, f->packed_offset ^ meta->size_key);
	write_u32(out, f->packed_size ^ meta->size_key);
}

bool arc_write(const char *path, arc_file_list files, struct arc_metadata *meta)
{
	if (ai5_target_game == GAME_KAKYUUSEI)
		return arc_write_kakyuusei(path, files);

	// determine entry format
	void (*write_entry)(struct port*,struct arc_file*,struct arc_metadata*);
	if (meta->name_off == 0 && meta->size_off == meta->name_length
			&& meta->offset_off == meta->size_off + 4) {
		write_entry = write_entry_NSO;
	} else if (meta->name_off == 0 && meta->offset_off == meta->name_length
			&& meta->size_off == meta->offset_off + 4) {
		write_entry = write_entry_NOS;
	} else {
		sys_error("Unsupported archive entry format");
	}

	struct port out;
	if (!port_file_open(&out, path))
		sys_error("Failed to open \"%s\": %s\n", path, strerror(errno));

	write_u32(&out, vector_length(files));

	// write file data
	if (!port_seek(&out, 4 + meta->entry_size * vector_length(files)))
		sys_error("Seek failed: %s", strerror(errno));

	struct arc_file *f;
	vector_foreach_p(f, files) {
		arc_file_write(&out, f);
	}

	// write index
	if (!port_seek(&out, 4))
		sys_error("Seek failed: %s", strerror(errno));

	vector_foreach_p(f, files) {
		write_entry(&out, f, meta);
	}

	port_close(&out);
	return true;
}

/*
 * Prepare an arc_file struct for a given filesystem path
 * (not necessarily an ARC_FILE_FS, if conversion is involved).
 */
static struct arc_file arc_file_fs(string path, string name, bool compress)
{
	// handle conversions
	if (compress) {
		size_t raw_size;
		uint8_t *raw_data = file_read(path, &raw_size);
		if (!raw_data)
			sys_error("Read failure: %s", strerror(errno));

		size_t size;
		uint8_t *data;
		if (game_is_aiwin())
			data = lzss_bw_compress(raw_data, raw_size, &size);
		else
			data = lzss_compress(raw_data, raw_size, &size);
		if (!data)
			sys_error("Compression failure\n");

		free(raw_data);
		return (struct arc_file) {
			.type = ARC_FILE_MEM,
			.mem = {
				.name = name,
				.data = data,
				.size = size,
			}
		};
	}

	// plain file to be read from filesystem (without compression, etc.)
	return (struct arc_file) {
		.type = ARC_FILE_FS,
		.fs = {
			.name = name,
			.path = string_dup(path),
		}
	};
}

/*
 * Return the upper-cased basename of the path.
 */
static string path_to_arc_name(string path)
{
	char *p = strrchr(path, '/');
	if (!p)
		return string_dup(path);
	return string_new(p+1);
}

/*
 * Prepare an arc_file_list for an ARCPACK manifest.
 */
static arc_file_list arcpack_file_list(struct arc_arcpack_manifest *mf,
		struct archive **arc_out, struct arc_metadata *meta, bool compress)
{
	struct archive *arc = NULL;
	arc_file_list files = vector_initializer;

	// add files from archive
	if (mf->input_arc) {
		if (!(arc = archive_open(mf->input_arc, ARCHIVE_MMAP | ARCHIVE_RAW)))
			sys_error("Failed to open archive \"%s\"\n", mf->input_arc);

		struct archive_data *data;
		archive_foreach(data, arc) {
			const struct arc_file f = {
				.type = ARC_FILE_ARCDATA,
				.arcdata = data
			};
			vector_push(struct arc_file, files, f);
		}
	}

	// add files from manifest
	string path;
	vector_foreach(path, mf->input_files) {
		int i;
		string name = path_to_arc_name(path);
		if (string_length(name) >= meta->name_length)
			sys_error("File name too long: \"%s\"\n", name);
		if (arc && (i = archive_get_index(arc, name)) >= 0) {
			// if name appears in input archive, replace the entry
			struct arc_file *f = &vector_A(files, i);
			arc_file_free(f);
			*f = arc_file_fs(path, name, compress);
		} else {
			// otherwise add a new entry
			vector_push(struct arc_file, files, arc_file_fs(path, name, compress));
		}
	}

	*arc_out = arc;
	return files;
}

static void decode_key(const char *key, struct arc_metadata *dst)
{
	unsigned name_key;
	if (sscanf(key, "%08x%08x%02x%02x", &dst->offset_key, &dst->size_key,
				&name_key, &dst->name_length) != 4) {
		sys_error("Invalid key: %s", key);
	}
	dst->name_key = name_key;
}

#define NAME_SIZE_OFFSET(len) \
	.name_length = len, \
	.name_off = 0, \
	.size_off = len, \
	.offset_off = len + 4, \
	.entry_size = len + 8

#define NAME_OFFSET_SIZE(len) \
	.name_length = len, \
	.name_off = 0, \
	.offset_off = len, \
	.size_off = len + 4, \
	.entry_size = len + 8

static struct arc_metadata game_keys[] = {
	[GAME_YUKINOJOU] = {
		NAME_SIZE_OFFSET(20),
		.offset_key  = 0x87af1f1c,
		.size_key    = 0xf3107572,
		.name_key    = 0xfa,
	},
	[GAME_YUNO] = {
		NAME_SIZE_OFFSET(20),
		.offset_key  = 0x68820811,
		.size_key    = 0x33656755,
		.name_key    = 0x03,
	},
	[GAME_SHANGRLIA] = {
		NAME_SIZE_OFFSET(20),
		.offset_key  = 0x68820811,
		.size_key    = 0x33656755,
		.name_key    = 0x03,
	},
	[GAME_SHANGRLIA2] = {
		NAME_SIZE_OFFSET(20),
		.offset_key  = 0x68820811,
		.size_key    = 0x33656755,
		.name_key    = 0x03,
	},
	[GAME_BEYOND] = {
		NAME_SIZE_OFFSET(20),
		.offset_key  = 0x55aa55aa,
		.size_key    = 0xaa55aa55,
		.name_key    = 0x55,
	},
	[GAME_AI_SHIMAI] = {
		NAME_SIZE_OFFSET(20),
		.offset_key  = 0xd4c29ff9,
		.size_key    = 0x13f09573,
		.name_key    = 0x26,
	},
	[GAME_KOIHIME] = {
		NAME_SIZE_OFFSET(20),
		.offset_key  = 0x55aa55aa,
		.size_key    = 0xaa55aa55,
		.name_key    = 0x55,
	},
	[GAME_DOUKYUUSEI] = {
		NAME_SIZE_OFFSET(20),
		.offset_key  = 0x55aa55aa,
		.size_key    = 0xaa55aa55,
		.name_key    = 0x55,
	},
	[GAME_ISAKU] = {
		NAME_SIZE_OFFSET(20),
		.offset_key  = 0x55aa55aa,
		.size_key    = 0xaa55aa55,
		.name_key    = 0x55,
	},
	[GAME_ALLSTARS] = {
		NAME_SIZE_OFFSET(20),
		.offset_key  = 0x44bd44bd,
		.size_key    = 0xcf88cf88,
		.name_key    = 0x66,
	},
	[GAME_SHUUSAKU] = {
		NAME_OFFSET_SIZE(16),
		.offset_key  = 0,
		.size_key    = 0,
		.name_key    = 0,
	},
	[GAME_KAWARAZAKIKE] = {
		NAME_OFFSET_SIZE(32),
		.offset_key  = 0,
		.size_key    = 0,
		.name_key    = 0,
	},
};

static void set_key_by_game(const char *name, struct arc_metadata *meta)
{
	enum ai5_game_id id = ai5_parse_game_id(name);
	if (id == GAME_KAKYUUSEI)
		return;
	if (id >= ARRAY_SIZE(game_keys))
		sys_error("Key for game \"%s\" is unknown.\n", name);
	*meta = game_keys[id];
}

enum {
	LOPT_GAME = 256,
	LOPT_KEY,
	LOPT_COMPRESS,
	LOPT_NO_COMPRESS,
};

static int cli_arc_pack(int argc, char *argv[])
{
	struct arc_metadata meta = {
		.name_length = 20,
		.offset_key  = 0x55aa55aa,
		.size_key    = 0xaa55aa55,
		.name_key    = 0x55,
	};
	bool compress = false;
	bool no_compress = false;
	while (1) {
		int c = command_getopt(argc, argv, &cmd_arc_pack);
		if (c == -1)
break;
		switch (c) {
		case LOPT_KEY:
			decode_key(optarg, &meta);
			break;
		case 'g':
		case LOPT_GAME:
			ai5_set_game(optarg);
			set_key_by_game(optarg, &meta);
			break;
		case LOPT_COMPRESS:
			compress = true;
			break;
		case LOPT_NO_COMPRESS:
			no_compress = true;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		command_usage_error(&cmd_arc_pack, "Wrong number of arguments.\n");

	struct arc_manifest *mf = arc_manifest_parse(argv[0]);
	if (!mf)
		sys_error("Failed to parse archive manifest \"%s\".\n", argv[0]);

	if (mf->type != ARC_MF_ARCPACK)
		sys_error("Unsupported manifest type.\n");

	if (!compress && !no_compress)
		compress = arc_is_compressed(mf->output_path, ai5_target_game);

	struct archive *arc = NULL;
	arc_file_list files = arcpack_file_list(&mf->arcpack, &arc, &meta, compress);
	arc_write(mf->output_path, files, &meta);

	if (arc)
		archive_close(arc);
	arc_file_list_free(files);
	arc_manifest_free(mf);
	return 0;
}

struct command cmd_arc_pack = {
	.name = "pack",
	.usage = "[options...] <input-file>",
	.description = "List the contents of an archive file",
	.parent = &cmd_arc,
	.fun = cli_arc_pack,
	.options = {
		{ "key", 0, "Specify the index encryption key", required_argument, LOPT_KEY },
		{ "game", 'g', "Set the target game", required_argument, LOPT_GAME },
		{ "compress", 0, "Compress archived files", no_argument, LOPT_COMPRESS },
		{ "no-compress", 0, "Do not compress archived files", no_argument, LOPT_NO_COMPRESS },
		{ 0 }
	}
};
