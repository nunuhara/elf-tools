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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "nulib.h"
#include "nulib/file.h"
#include "nulib/port.h"
#include "nulib/string.h"
#include "ai5/anim.h"
#include "ai5/arc.h"
#include "ai5/cg.h"
#include "ai5/game.h"

#include "cli.h"
#include "mes.h"

static bool raw = false;
static bool mes_text = false;
static bool mes_flat = false;
static int mes_name_fun = -1;

static bool open_output_file(const char *path, struct port *out)
{
	if (!path) {
		port_file_init(out, stdout);
		return true;
	}
	if (!port_file_open(out, path)) {
		WARNING("port_file_open: %s", strerror(errno));
		return false;
	}
	return true;
}

static bool extract_raw(struct archive_data *data, const char *output_file)
{
	struct port out;
	if (!open_output_file(output_file, &out))
		return false;
	if (!port_write_bytes(&out, data->data, data->size)) {
		WARNING("port_write_bytes: %s", strerror(errno));
		port_close(&out);
		return false;
	}
	port_close(&out);
	return true;
}

static bool extract_mes(struct archive_data *data, const char *output_file)
{
	struct port out;
	if (!open_output_file(output_file, &out))
		return false;

	mes_clear_labels();

	if (mes_flat || mes_text) {
		mes_statement_list statements = vector_initializer;
		if (!mes_parse_statements(data->data, data->size, &statements)) {
			sys_warning("Failed to parse .mes file \"%s\".\n", data->name);
			port_close(&out);
			return false;
		}
		if (mes_flat) {
			mes_flat_statement_list_print(statements, &out);
		} else {
			// write text file
			mes_text_print(statements, &out, mes_name_fun);
			// write mes file
			string mes_file = file_replace_extension(output_file, "MES.IN");
			if (!extract_raw(data, mes_file))
				sys_warning("Failed to write .mes file \"%s\".\n", mes_file);
			string_free(mes_file);
		}
		mes_statement_list_free(statements);
		port_close(&out);
		return true;
	}

	mes_ast_block toplevel = vector_initializer;
	if (!(mes_decompile(data->data, data->size, &toplevel))) {
		sys_warning("Failed to decompile .mes file \"%s\".\n", data->name);
		port_close(&out);
		return false;
	}
	mes_ast_block_print(toplevel, mes_name_fun, &out);
	mes_ast_block_free(toplevel);
	port_close(&out);
	return true;
}

static bool extract_cg(struct archive_data *data, const char *output_file)
{
	char name[512];
	snprintf(name, 512, "%s.PNG", output_file);
	struct port out;
	if (!open_output_file(name, &out))
		return false;

	struct cg *cg = cg_load_arcdata(data);
	if (!cg) {
		sys_warning("Failed to decode image file \"%s\".\n", data->name);
		port_close(&out);
		return false;
	}
	if (!cg_write(cg, out.file, CG_TYPE_PNG)) {
		sys_warning("Failed to encode image file.\n");
		cg_free(cg);
		port_close(&out);
		return false;
	}

	cg_free(cg);
	port_close(&out);
	return true;
}

static bool extract_anim(struct archive_data *data, const char *output_file)
{
	struct port out;
	if (!open_output_file(output_file, &out))
		return false;

	struct anim *anim = anim_parse(data->data, data->size);
	if (!anim) {
		sys_warning("Failed to decompile animation file \"%s\".\n", data->name);
		port_close(&out);
		return false;
	}
	anim_print(&out, anim);
	anim_free(anim);
	port_close(&out);
	return true;
}

static bool ext_is_cg(const char *ext)
{
	static const char * const cg_ext[] = {
		"GP8", "G16", "G24", "G32", "GCC", "GPX"
	};
	for (unsigned i = 0; i < ARRAY_SIZE(cg_ext); i++) {
		if (!strcasecmp(ext, cg_ext[i]))
			return true;
	}
	return false;
}

static bool extract_file(struct archive_data *data, const char *output_file)
{
	if (raw)
		return extract_raw(data, output_file);

	const char *ext = file_extension(data->name);
	if (!strcasecmp(ext, "MES"))
		return extract_mes(data, output_file);
	if (ext_is_cg(ext))
		return extract_cg(data, output_file);
	if (!strcasecmp(ext, "S4") || !strcasecmp(ext, "A"))
		return extract_anim(data, output_file);
	return extract_raw(data, output_file);
}

static void extract_one(struct archive *arc, const char *name, const char *output_file)
{
	struct archive_data *data = archive_get(arc, name);
	if (!data)
		sys_error("Failed to read file \"%s\" from archive.\n", name);
	if (!extract_file(data, output_file))
		sys_error("failed to write output file");

	archive_data_release(data);
}

// Add a trailing slash to a path
static char *output_dir_path(const char *path)
{
	if (!path || *path == '\0')
		return xstrdup("./"); // default is cwd

	size_t size = strlen(path);
	if (path[size-1] == '/')
		return xstrdup(path);

	char *s = xmalloc(size + 2);
	strcpy(s, path);
	s[size] = '/';
	s[size+1] = '\0';
	return s;
}

static string make_output_path(string dir, const char *name, const char *ext)
{
	string out_name = file_replace_extension(name, ext);
	dir = string_concat(dir, out_name);
	string_free(out_name);
	return dir;
}

static string get_output_path(const char *dir, const char *name)
{
	string path = string_new(dir);
	if (raw)
		return string_concat_cstring(path, name);

	const char *ext = file_extension(name);
	if (!strcasecmp(ext, "MES")) {
		if (mes_text)
			return make_output_path(path, name, "TXT");
		return make_output_path(path, name, "SMES");
	}
	if (!strcasecmp(ext, "G16") || !strcasecmp(ext, "G24") || !strcasecmp(ext, "G32"))
		return make_output_path(path, name, "PNG");
	if (!strcasecmp(ext, "S4") || !strcasecmp(ext, "A"))
		return make_output_path(path, name, "SA");
	return string_concat_cstring(path, name);
}

static void extract_all(struct archive *arc, const char *_output_dir)
{
	char *output_dir = output_dir_path(_output_dir);
	if (mkdir_p(output_dir) < 0) {
		sys_error("Failed to create output directory: %s.\n", strerror(errno));
	}

	struct archive_data *data;
	archive_foreach(data, arc) {
		if (!archive_data_load(data)) {
			sys_warning("Failed to read file \"%s\" from archive\n", data->name);
			continue;
		}
		string output_file = get_output_path(output_dir, data->name);
		sys_message("%s... ", output_file);
		if (extract_file(data, output_file)) {
			sys_message("OK\n");
		} else {
			sys_warning("failed to extract file \"%s\"\n", data->name);
		}
		string_free(output_file);
		archive_data_release(data);
	}

	free(output_dir);
}

enum archive_data_type {
	ARC_OTHER,
	ARC_MES,
	ARC_DATA,
	ARC_AUDIO,
};

static bool suffix_equal(const char *str, const char *suffix)
{
	size_t str_len = strlen(str);
	size_t suffix_len = strlen(suffix);
	if (str_len < suffix_len)
		return false;

	const char *start = str + (str_len - suffix_len);
	return !strcasecmp(start, suffix);
}

static enum archive_data_type arc_data_type(const char *path)
{
	if (suffix_equal(path, "mes.arc") || suffix_equal(path, "message.arc"))
		return ARC_MES;
	if (suffix_equal(path, "data.arc"))
		return ARC_DATA;
	if (suffix_equal(path, ".awd") || suffix_equal(path, ".awf"))
		return ARC_AUDIO;
	return ARC_OTHER;
}

bool arc_is_compressed(const char *path)
{
	switch (ai5_target_game) {
	case GAME_ALLSTARS:
	case GAME_KAWARAZAKIKE:
		return false;
	default:
		break;
	}

	enum archive_data_type t = arc_data_type(path);
	if (t == ARC_AUDIO)
		return false;
	if (ai5_target_game == GAME_KAKYUUSEI)
		return t == ARC_MES;

	// TODO: try to read bMESTYPE / bDATATYPE from AI5WIN.INI?
	return t == ARC_MES || t == ARC_DATA;
}

enum {
	LOPT_OUTPUT = 256,
	LOPT_GAME,
	LOPT_NAME,
	LOPT_RAW,
	LOPT_DECOMPRESS,
	LOPT_NO_DECOMPRESS,
	LOPT_MES_FLAT,
	LOPT_MES_TEXT,
	LOPT_MES_NAME,
	LOPT_KEY,
	LOPT_PCM,
	LOPT_STEREO,
};

int arc_extract(int argc, char *argv[])
{
	const char *output_file = NULL;
	const char *name = NULL;
	bool key = false;
	unsigned flags = 0;
	bool no_decompress = false;
	bool decompress = false;
	while (1) {
		int c = command_getopt(argc, argv, &cmd_arc_extract);
		if (c == -1)
			break;
		switch (c) {
		case 'o':
		case LOPT_OUTPUT:
			output_file = optarg;
			break;
		case 'g':
		case LOPT_GAME:
			ai5_set_game(optarg);
			break;
		case 'n':
		case LOPT_NAME:
			name = optarg;
			break;
		case LOPT_RAW:
			raw = true;
			break;
		case LOPT_DECOMPRESS:
			decompress = true;
			break;
		case LOPT_NO_DECOMPRESS:
			no_decompress = true;
			break;
		case LOPT_MES_FLAT:
			mes_flat = true;
			break;
		case LOPT_MES_TEXT:
			mes_text = true;
			break;
		case LOPT_MES_NAME:
			mes_name_fun = atoi(optarg);
			break;
		case LOPT_KEY:
			key = true;
			break;
		case LOPT_PCM:
			flags |= ARCHIVE_PCM;
			break;
		case LOPT_STEREO:
			flags |= ARCHIVE_STEREO;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		command_usage_error(&cmd_arc_extract, "Wrong number of arguments.\n");

	if (no_decompress || (!decompress && !arc_is_compressed(argv[0])))
		flags |= ARCHIVE_RAW;

	struct archive *arc = archive_open(argv[0], flags);
	if (!arc)
		sys_error("Failed to open archive file \"%s\".\n", argv[0]);

	if (key) {
		NOTICE("%08x%08x%02x%02x", arc->meta.offset_key, arc->meta.size_key,
				arc->meta.name_key, arc->meta.name_length);
	} else if (name) {
		extract_one(arc, name, output_file);
	} else {
		extract_all(arc, output_file);
	}

	archive_close(arc);
	return 0;
}

struct command cmd_arc_extract = {
	.name = "extract",
	.usage = "[options...] <input-file>",
	.description = "Extract an archive file",
	.parent = &cmd_arc,
	.fun = arc_extract,
	.options = {
		{ "output", 'o', "Set the output path", required_argument, LOPT_OUTPUT },
		{ "game", 'g', "Set the target game", required_argument, LOPT_GAME },
		{ "name", 'n', "Specify the file to extract", required_argument, LOPT_NAME },
		{ "raw", 0, "Do not convert (keep original file type)", no_argument, LOPT_RAW },
		{ "decompress", 0, "Decompress files", no_argument, LOPT_DECOMPRESS },
		{ "no-decompress", 0, "Do not decompress files", no_argument, LOPT_NO_DECOMPRESS },
		{ "mes-flat", 0, "Output flat mes files", no_argument, LOPT_MES_FLAT },
		{ "mes-text", 0, "Output text for mes files", no_argument, LOPT_MES_TEXT },
		{ "mes-name-function", 0, "Specify the name function number for mes files", no_argument, LOPT_MES_NAME },
		{ "pcm", 0, "Archive contains raw PCM data", no_argument, LOPT_PCM },
		{ "key", 0, "Print the index encryption key (do not extract)", no_argument, LOPT_KEY },
		{ "stereo", 0, "Raw PCM data is stereo (AWD/AWF archives)", no_argument, LOPT_STEREO },
		{ 0 }
	}
};
