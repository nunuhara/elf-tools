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

#include <string.h>

#include "nulib.h"
#include "nulib/string.h"
#include "nulib/vector.h"
#include "mes.h"

#include "flat_parser.h"

struct mes_path_component {
	const char *name;
	int nr_children;
	struct mes_path_component **children;
};

static struct mes_path_component sys_set_font_size = { .name = "set_font_size" };

static struct mes_path_component sys_cursor_load = { .name = "load" };
static struct mes_path_component sys_cursor_refresh = { .name = "refresh" };
static struct mes_path_component sys_cursor_save_pos = { .name = "save_pos" };
static struct mes_path_component sys_cursor_set_pos = { .name = "set_pos" };
static struct mes_path_component sys_cursor_open = { .name = "open" };

static struct mes_path_component *sys_cursor_children[] = {
	[0] = &sys_cursor_load,
	[1] = &sys_cursor_refresh,
	[2] = &sys_cursor_save_pos,
	[3] = &sys_cursor_set_pos,
	[4] = &sys_cursor_open,
};

static struct mes_path_component sys_cursor = {
	.name = "Cursor",
	.nr_children = ARRAY_SIZE(sys_cursor_children),
	.children = sys_cursor_children
};

static struct mes_path_component sys_savedata_save = { .name = "save" };
static struct mes_path_component sys_savedata_load = { .name = "load" };
static struct mes_path_component sys_savedata_save_except_mes_name =
	{ .name = "save_except_mes_name" };
static struct mes_path_component sys_savedata_load_var4 = { .name = "load_var4" };
static struct mes_path_component sys_savedata_write_var4 = { .name = "write_var4" };
static struct mes_path_component sys_savedata_save_union_var4 = { .name = "save_union_var4" };
static struct mes_path_component sys_savedata_load_var4_slice = { .name = "load_var4_slice" };
static struct mes_path_component sys_savedata_save_var4_slice = { .name = "save_var4_slice" };
static struct mes_path_component sys_savedata_copy = { .name = "copy" };
static struct mes_path_component sys_savedata_set_mes_name = { .name = "set_mes_name" };

static struct mes_path_component *sys_savedata_children[] = {
	[1] = &sys_savedata_save,
	[2] = &sys_savedata_load,
	[3] = &sys_savedata_save_except_mes_name,
	[4] = &sys_savedata_load_var4,
	[5] = &sys_savedata_write_var4,
	[6] = &sys_savedata_save_union_var4,
	[7] = &sys_savedata_load_var4_slice,
	[8] = &sys_savedata_save_var4_slice,
	[9] = &sys_savedata_copy,
	[13] = &sys_savedata_set_mes_name,
};

static struct mes_path_component sys_savedata = {
	.name = "SaveData",
	.nr_children = ARRAY_SIZE(sys_savedata_children),
	.children = sys_savedata_children
};

static struct mes_path_component sys_audio = {
	.name = "Audio",
};

static struct mes_path_component sys_file_read = { .name = "read" };
static struct mes_path_component sys_file_write = { .name = "write" };

static struct mes_path_component *sys_file_children[] = {
	[0] = &sys_file_read,
	[1] = &sys_file_write,
};

static struct mes_path_component sys_file = {
	.name = "File",
	.nr_children = ARRAY_SIZE(sys_file_children),
	.children = sys_file_children
};

static struct mes_path_component sys_load_image = { .name = "load_image" };

static struct mes_path_component sys_palette = {
	.name = "Palette"
};

static struct mes_path_component sys_image = {
	.name = "Image"
};

static struct mes_path_component sys_set_text_colors = { .name = "set_text_colors" };
static struct mes_path_component sys_farcall = { .name = "farcall" };
static struct mes_path_component sys_get_time = { .name = "get_time" };
static struct mes_path_component sys_noop = { .name = "noop" };
static struct mes_path_component sys_noop2 = { .name = "noop2" };
static struct mes_path_component sys_strlen = { .name = "strlen" };

static struct mes_path_component *system_children[] = {
	[0] = &sys_set_font_size,
	[2] = &sys_cursor,
	[4] = &sys_savedata,
	[5] = &sys_audio,
	[7] = &sys_file,
	[8] = &sys_load_image,
	[9] = &sys_palette,
	[10] = &sys_image,
	[12] = &sys_set_text_colors,
	[13] = &sys_farcall,
	[16] = &sys_get_time,
	[17] = &sys_noop,
	[20] = &sys_noop2,
	[21] = &sys_strlen,
};

static struct mes_path_component syscalls = {
	.name = "System",
	.nr_children = ARRAY_SIZE(system_children),
	.children = system_children
};

static struct mes_path_component *_resolve_qname(struct mes_path_component *ctx,
		struct mes_qname_part *part, int *no)
{
	if (part->type == MES_QNAME_NUMBER) {
		*no = part->number;
		if (*no < ctx->nr_children && ctx->children[*no])
			return ctx->children[*no];
		return NULL;
	}
	for (int i = 0; i < ctx->nr_children; i++) {
		if (!ctx->children[i])
			continue;
		if (!strcmp(ctx->children[i]->name, part->ident)) {
			*no = i;
			return ctx->children[i];
		}
	}
	*no = -1;
	return NULL;
}

mes_parameter_list mes_resolve_syscall(mes_qname name, int *no)
{
	mes_parameter_list params = vector_initializer;

	// get syscall number
	if (vector_length(name) < 1)
		goto error;

	struct mes_path_component *ctx = _resolve_qname(&syscalls, &kv_A(name, 0), no);
	if (*no < 0)
		goto error;

	// resolve remaining identifiers
	for (int i = 1; ctx && i < vector_length(name); i++) {
		int part_no;
		struct mes_qname_part *part = &kv_A(name, i);
		ctx = _resolve_qname(ctx, part, &part_no);
		if (part_no < 0)
			goto error;
		if (part->type == MES_QNAME_IDENT)
			string_free(part->ident);
		part->type = MES_QNAME_NUMBER;
		part->number = part_no;
	}

	// create parameter list from resolved identifiers
	for (int i = 1; i < vector_length(name); i++) {
		struct mes_qname_part *part = &kv_A(name, i);
		if (part->type == MES_QNAME_IDENT)
			goto error;
		struct mes_parameter param = { 
			.type = MES_PARAM_EXPRESSION,
			.expr = xcalloc(1, sizeof(struct mes_expression))
		};
		param.expr->op = MES_EXPR_IMM;
		param.expr->arg8 = part->number;
		vector_push(struct mes_parameter, params, param);
	}

	mes_qname_free(name);
	return params;
error:
	vector_destroy(params);
	*no = -1;
	return (mes_parameter_list)vector_initializer;
}

const char *mes_system_var16_names[MES_NR_SYSTEM_VARIABLES] = {
	[MES_SYS_VAR_FLAGS] = "flags",
	[MES_SYS_VAR_TEXT_HOME_X] = "text_home_x",
	[MES_SYS_VAR_TEXT_HOME_Y] = "text_home_y",
	[MES_SYS_VAR_WIDTH] = "width",
	[MES_SYS_VAR_HEIGHT] = "height",
	[MES_SYS_VAR_TEXT_CURSOR_X] = "text_cursor_x",
	[MES_SYS_VAR_TEXT_CURSOR_Y] = "text_cursor_y",
	[MES_SYS_VAR_FONT_WIDTH] = "font_width",
	[MES_SYS_VAR_FONT_HEIGHT] = "font_height",
	[MES_SYS_VAR_FONT_WIDTH2] = "font_width2",
	[MES_SYS_VAR_FONT_HEIGHT2] = "font_height2",
	[MES_SYS_VAR_MASK_COLOR] = "mask_color",
};

const char *mes_system_var32_names[MES_NR_SYSTEM_VARIABLES] = {
	[MES_SYS_VAR_MEMORY] = "memory",
	[MES_SYS_VAR_PALETTE] = "palette",
	[MES_SYS_VAR_FILE_DATA] = "file_data",
	[MES_SYS_VAR_MENU_ENTRY_ADDRESSES] = "menu_entry_addresses",
	[MES_SYS_VAR_MENU_ENTRY_NUMBERS] = "menu_entry_numbers",
};

int mes_resolve_sysvar(string name, bool *dword)
{
	for (int i = 0; i < MES_NR_SYSTEM_VARIABLES; i++) {
		if (mes_system_var16_names[i] && !strcmp(name, mes_system_var16_names[i])) {
			*dword = false;
			return i;
		}
		if (mes_system_var32_names[i] && !strcmp(name, mes_system_var32_names[i])) {
			*dword = true;
			return i;
		}
	}
	return -1;
}
