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
#include <ctype.h>

#include "nulib.h"
#include "nulib/buffer.h"
#include "nulib/hashtable.h"
#include "nulib/string.h"
#include "nulib/utfsjis.h"
#include "nulib/vector.h"
#include "mes.h"
#include "game.h"

#define DC_WARNING(addr, fmt, ...) \
	sys_warning("WARNING: At 0x%08x: " fmt "\n", (unsigned)addr, ##__VA_ARGS__)

#define DC_ERROR(addr, fmt, ...) \
	sys_warning("ERROR: At 0x%08x: " fmt "\n", (unsigned)addr, ##__VA_ARGS__)

#define STMT_OP_MAX 0x17
#define EXPR_OP_MAX 0x100

struct opcode_tables {
	uint8_t stmt_op_to_int[STMT_OP_MAX];
	enum mes_statement_op int_to_stmt_op[STMT_OP_MAX];
	uint8_t expr_op_to_int[EXPR_OP_MAX];
	enum mes_expression_op int_to_expr_op[EXPR_OP_MAX];
};

// default opcode tables {{{

#define DEFAULT_STMT_OP_TO_INT { \
	[MES_STMT_END]     = 0x00, \
	[MES_STMT_TXT]     = 0x01, \
	[MES_STMT_STR]     = 0x02, \
	[MES_STMT_SETRBC]  = 0x03, \
	[MES_STMT_SETV]    = 0x04, \
	[MES_STMT_SETRBE]  = 0x05, \
	[MES_STMT_SETAC]   = 0x06, \
	[MES_STMT_SETA_AT] = 0x07, \
	[MES_STMT_SETAD]   = 0x08, \
	[MES_STMT_SETAW]   = MES_STMT_INVALID, \
	[MES_STMT_SETAB]   = MES_STMT_INVALID, \
	[MES_STMT_JZ]      = 0x09, \
	[MES_STMT_JMP]     = 0x0A, \
	[MES_STMT_SYS]     = 0x0B, \
	[MES_STMT_GOTO]    = 0x0C, \
	[MES_STMT_CALL]    = 0x0D, \
	[MES_STMT_MENUI]   = 0x0E, \
	[MES_STMT_PROC]    = 0x0F, \
	[MES_STMT_UTIL]    = 0x10, \
	[MES_STMT_LINE]    = 0x11, \
	[MES_STMT_PROCD]   = 0x12, \
	[MES_STMT_MENUS]   = 0x13, \
	[MES_STMT_SETRD]   = 0x14, \
}

#define DEFAULT_INT_TO_STMT_OP { \
	[0x00] = MES_STMT_END, \
	[0x01] = MES_STMT_TXT, \
	[0x02] = MES_STMT_STR, \
	[0x03] = MES_STMT_SETRBC, \
	[0x04] = MES_STMT_SETV, \
	[0x05] = MES_STMT_SETRBE, \
	[0x06] = MES_STMT_SETAC, \
	[0x07] = MES_STMT_SETA_AT, \
	[0x08] = MES_STMT_SETAD, \
	[0x09] = MES_STMT_JZ, \
	[0x0A] = MES_STMT_JMP, \
	[0x0B] = MES_STMT_SYS, \
	[0x0C] = MES_STMT_GOTO, \
	[0x0D] = MES_STMT_CALL, \
	[0x0E] = MES_STMT_MENUI, \
	[0x0F] = MES_STMT_PROC, \
	[0x10] = MES_STMT_UTIL, \
	[0x11] = MES_STMT_LINE, \
	[0x12] = MES_STMT_PROCD, \
	[0x13] = MES_STMT_MENUS, \
	[0x14] = MES_STMT_SETRD, \
	[0x15] = MES_STMT_INVALID, \
	[0x16] = MES_STMT_INVALID, \
}

#define DEFAULT_EXPR_OP_TO_INT { \
	[MES_EXPR_VAR] = 0x80, \
	[MES_EXPR_ARRAY16_GET16] = 0xA0, \
	[MES_EXPR_ARRAY16_GET8] = 0xC0, \
	[MES_EXPR_PLUS] = 0xE0, \
	[MES_EXPR_MINUS] = 0xE1, \
	[MES_EXPR_MUL] = 0xE2, \
	[MES_EXPR_DIV] = 0xE3, \
	[MES_EXPR_MOD] = 0xE4, \
	[MES_EXPR_RAND] = 0xE5, \
	[MES_EXPR_AND] = 0xE6, \
	[MES_EXPR_OR] = 0xE7, \
	[MES_EXPR_BITAND] = 0xE8, \
	[MES_EXPR_BITIOR] = 0xE9, \
	[MES_EXPR_BITXOR] = 0xEA, \
	[MES_EXPR_LT] = 0xEB, \
	[MES_EXPR_GT] = 0xEC, \
	[MES_EXPR_LTE] = 0xED, \
	[MES_EXPR_GTE] = 0xEE, \
	[MES_EXPR_EQ] = 0xEF, \
	[MES_EXPR_NEQ] = 0xF0, \
	[MES_EXPR_IMM16] = 0xF1, \
	[MES_EXPR_IMM32] = 0xF2, \
	[MES_EXPR_REG16] = 0xF3, \
	[MES_EXPR_REG8] = 0xF4, \
	[MES_EXPR_ARRAY32_GET32] = 0xF5, \
	[MES_EXPR_VAR32] = 0xF6, \
	[MES_EXPR_END] = 0xFF, \
}

#define DEFAULT_INT_TO_EXPR_OP { \
	[0x80] = MES_EXPR_VAR, \
	[0xA0] = MES_EXPR_ARRAY16_GET16, \
	[0xC0] = MES_EXPR_ARRAY16_GET8, \
	[0xE0] = MES_EXPR_PLUS, \
	[0xE1] = MES_EXPR_MINUS, \
	[0xE2] = MES_EXPR_MUL, \
	[0xE3] = MES_EXPR_DIV, \
	[0xE4] = MES_EXPR_MOD, \
	[0xE5] = MES_EXPR_RAND, \
	[0xE6] = MES_EXPR_AND, \
	[0xE7] = MES_EXPR_OR, \
	[0xE8] = MES_EXPR_BITAND, \
	[0xE9] = MES_EXPR_BITIOR, \
	[0xEA] = MES_EXPR_BITXOR, \
	[0xEB] = MES_EXPR_LT, \
	[0xEC] = MES_EXPR_GT, \
	[0xED] = MES_EXPR_LTE, \
	[0xEE] = MES_EXPR_GTE, \
	[0xEF] = MES_EXPR_EQ, \
	[0xF0] = MES_EXPR_NEQ, \
	[0xF1] = MES_EXPR_IMM16, \
	[0xF2] = MES_EXPR_IMM32, \
	[0xF3] = MES_EXPR_REG16, \
	[0xF4] = MES_EXPR_REG8, \
	[0xF5] = MES_EXPR_ARRAY32_GET32, \
	[0xF6] = MES_EXPR_VAR32, \
	[0xFF] = MES_EXPR_END, \
}

struct opcode_tables default_tables = {
	.stmt_op_to_int = DEFAULT_STMT_OP_TO_INT,
	.int_to_stmt_op = DEFAULT_INT_TO_STMT_OP,
	.expr_op_to_int = DEFAULT_EXPR_OP_TO_INT,
	.int_to_expr_op = DEFAULT_INT_TO_EXPR_OP,
};

// default opcode tables }}}
// elf_classics {{{
#define CLASSICS_STMT_OP_TO_INT { \
	[MES_STMT_END]     = 0x00, \
	[MES_STMT_TXT]     = 0x01, \
	[MES_STMT_STR]     = 0x02, \
	[MES_STMT_SETRBC]  = 0x03, \
	[MES_STMT_SETV]    = 0x04, \
	[MES_STMT_SETRBE]  = 0x05, \
	[MES_STMT_SETAC]   = 0x06, \
	[MES_STMT_SETA_AT] = 0x07, \
	[MES_STMT_SETAD]   = 0x08, \
	[MES_STMT_SETAW]   = 0x09, \
	[MES_STMT_SETAB]   = 0x0A, \
	[MES_STMT_JZ]      = 0x0B, \
	[MES_STMT_JMP]     = 0x0C, \
	[MES_STMT_SYS]     = 0x0D, \
	[MES_STMT_GOTO]    = 0x0E, \
	[MES_STMT_CALL]    = 0x0F, \
	[MES_STMT_MENUI]   = 0x10, \
	[MES_STMT_PROC]    = 0x11, \
	[MES_STMT_UTIL]    = 0x12, \
	[MES_STMT_LINE]    = 0x13, \
	[MES_STMT_PROCD]   = 0x14, \
	[MES_STMT_MENUS]   = 0x15, \
	[MES_STMT_SETRD]   = 0x16, \
}

#define CLASSICS_INT_TO_STMT_OP { \
	[0x00] = MES_STMT_END, \
	[0x01] = MES_STMT_TXT, \
	[0x02] = MES_STMT_STR, \
	[0x03] = MES_STMT_SETRBC, \
	[0x04] = MES_STMT_SETV, \
	[0x05] = MES_STMT_SETRBE, \
	[0x06] = MES_STMT_SETAC, \
	[0x07] = MES_STMT_SETA_AT, \
	[0x08] = MES_STMT_SETAD, \
	[0x09] = MES_STMT_SETAW, \
	[0x0A] = MES_STMT_SETAB, \
	[0x0B] = MES_STMT_JZ, \
	[0x0C] = MES_STMT_JMP, \
	[0x0D] = MES_STMT_SYS, \
	[0x0E] = MES_STMT_GOTO, \
	[0x0F] = MES_STMT_CALL, \
	[0x10] = MES_STMT_MENUI, \
	[0x11] = MES_STMT_PROC, \
	[0x12] = MES_STMT_UTIL, \
	[0x13] = MES_STMT_LINE, \
	[0x14] = MES_STMT_PROCD, \
	[0x15] = MES_STMT_MENUS, \
	[0x16] = MES_STMT_SETRD, \
}

#define CLASSICS_EXPR_OP_TO_INT { \
	[MES_EXPR_IMM] = 0x00, \
	[MES_EXPR_VAR] = 0x80, \
	[MES_EXPR_ARRAY16_GET16] = 0xA0, \
	[MES_EXPR_ARRAY16_GET8] = 0xC0, \
	[MES_EXPR_PLUS] = 0xE0, \
	[MES_EXPR_MINUS] = 0xE1, \
	[MES_EXPR_MUL] = 0xE2, \
	[MES_EXPR_DIV] = 0xE3, \
	[MES_EXPR_MOD] = 0xE4, \
	[MES_EXPR_RAND] = 0xE5, \
	[MES_EXPR_AND] = 0xE6, \
	[MES_EXPR_OR] = 0xE7, \
	[MES_EXPR_BITAND] = 0xE8, \
	[MES_EXPR_BITIOR] = 0xE9, \
	[MES_EXPR_BITXOR] = 0xEA, \
	[MES_EXPR_LT] = 0xEB, \
	[MES_EXPR_GT] = 0xEC, \
	[MES_EXPR_LTE] = 0xED, \
	[MES_EXPR_GTE] = 0xEE, \
	[MES_EXPR_EQ] = 0xEF, \
	[MES_EXPR_NEQ] = 0xF0, \
	[MES_EXPR_IMM16] = 0xF1, \
	[MES_EXPR_IMM32] = 0xF2, \
	[MES_EXPR_REG16] = 0xF3, \
	[MES_EXPR_REG8] = 0xF4, \
	[MES_EXPR_ARRAY32_GET32] = 0xF5, \
	[MES_EXPR_ARRAY32_GET16] = 0xF6, \
	[MES_EXPR_ARRAY32_GET8] = 0xF7, \
	[MES_EXPR_VAR32] = 0xF8, \
	[MES_EXPR_END] = 0xFF, \
}

#define CLASSICS_INT_TO_EXPR_OP { \
	[0x80] = MES_EXPR_VAR, \
	[0xA0] = MES_EXPR_ARRAY16_GET16, \
	[0xC0] = MES_EXPR_ARRAY16_GET8, \
	[0xE0] = MES_EXPR_PLUS, \
	[0xE1] = MES_EXPR_MINUS, \
	[0xE2] = MES_EXPR_MUL, \
	[0xE3] = MES_EXPR_DIV, \
	[0xE4] = MES_EXPR_MOD, \
	[0xE5] = MES_EXPR_RAND, \
	[0xE6] = MES_EXPR_AND, \
	[0xE7] = MES_EXPR_OR, \
	[0xE8] = MES_EXPR_BITAND, \
	[0xE9] = MES_EXPR_BITIOR, \
	[0xEA] = MES_EXPR_BITXOR, \
	[0xEB] = MES_EXPR_LT, \
	[0xEC] = MES_EXPR_GT, \
	[0xED] = MES_EXPR_LTE, \
	[0xEE] = MES_EXPR_GTE, \
	[0xEF] = MES_EXPR_EQ, \
	[0xF0] = MES_EXPR_NEQ, \
	[0xF1] = MES_EXPR_IMM16, \
	[0xF2] = MES_EXPR_IMM32, \
	[0xF3] = MES_EXPR_REG16, \
	[0xF4] = MES_EXPR_REG8, \
	[0xF5] = MES_EXPR_ARRAY32_GET32, \
	[0xF6] = MES_EXPR_ARRAY32_GET16, \
	[0xF7] = MES_EXPR_ARRAY32_GET8, \
	[0xF8] = MES_EXPR_VAR32, \
	[0xFF] = MES_EXPR_END, \
}

static struct opcode_tables elf_classics_tables = {
	.stmt_op_to_int = CLASSICS_STMT_OP_TO_INT,
	.int_to_stmt_op = CLASSICS_INT_TO_STMT_OP,
	.expr_op_to_int = CLASSICS_EXPR_OP_TO_INT,
	.int_to_expr_op = CLASSICS_INT_TO_EXPR_OP,
};
// elf_classics }}}

struct opcode_tables op_tables = {
	.stmt_op_to_int = DEFAULT_STMT_OP_TO_INT,
	.int_to_stmt_op = DEFAULT_INT_TO_STMT_OP,
	.expr_op_to_int = DEFAULT_EXPR_OP_TO_INT,
	.int_to_expr_op = DEFAULT_INT_TO_EXPR_OP,
};

static void set_opcode_tables(struct opcode_tables *tables)
{
	memcpy(op_tables.stmt_op_to_int, tables->stmt_op_to_int, sizeof(op_tables.stmt_op_to_int));
	memcpy(op_tables.int_to_stmt_op, tables->int_to_stmt_op, sizeof(op_tables.int_to_stmt_op));
	memcpy(op_tables.expr_op_to_int, tables->expr_op_to_int, sizeof(op_tables.expr_op_to_int));
	memcpy(op_tables.int_to_expr_op, tables->int_to_expr_op, sizeof(op_tables.int_to_expr_op));
}

static struct opcode_tables *get_opcode_tables(enum game_id id)
{
	switch (id) {
	case GAME_ELF_CLASSICS:
		return &elf_classics_tables;
	default:
		return &default_tables;
	}
}

void mes_set_game(enum game_id id)
{
	set_opcode_tables(get_opcode_tables(id));
}

static enum mes_statement_op parse_stmt_op(uint8_t op)
{
	if (op >= ARRAY_SIZE(op_tables.int_to_stmt_op))
		return MES_STMT_INVALID;
	return op_tables.int_to_stmt_op[op];
}

static enum mes_expression_op parse_expr_op(uint8_t op)
{
	return op_tables.int_to_expr_op[op];
}

static struct mes_expression *stack_pop(unsigned addr, struct mes_expression **stack, int *stack_ptr)
{
	if (unlikely(*stack_ptr < 1)) {
		DC_WARNING(addr, "Stack empty in stack_pop");
		return NULL;
	}
	*stack_ptr = *stack_ptr - 1;
	return stack[*stack_ptr];
}

static struct mes_expression *mes_parse_expression(struct buffer *mes)
{
	int stack_ptr = 0;
	struct mes_expression *stack[4096];

	for (int i = 0; ; i++) {
		struct mes_expression *expr = xcalloc(1, sizeof(struct mes_expression));
		if (unlikely(stack_ptr >= 4096)) {
			DC_ERROR(mes->index, "Expression stack overflow");
			goto error;
		}
		uint8_t b = buffer_read_u8(mes);
		switch ((expr->op = parse_expr_op(b))) {
		case MES_EXPR_VAR:
			expr->arg8 = buffer_read_u8(mes);
			break;
		case MES_EXPR_ARRAY16_GET16:
			expr->arg8 = buffer_read_u8(mes);
			if (!(expr->sub_a = stack_pop(mes->index-1, stack, &stack_ptr)))
				goto error;
			break;
		case MES_EXPR_ARRAY16_GET8:
			expr->arg8 = buffer_read_u8(mes);
			if (!(expr->sub_a = stack_pop(mes->index-1, stack, &stack_ptr)))
				goto error;
			break;
		case MES_EXPR_PLUS:
		case MES_EXPR_MINUS:
		case MES_EXPR_MUL:
		case MES_EXPR_DIV:
		case MES_EXPR_MOD:
		case MES_EXPR_AND:
		case MES_EXPR_OR:
		case MES_EXPR_BITAND:
		case MES_EXPR_BITIOR:
		case MES_EXPR_BITXOR:
		case MES_EXPR_LT:
		case MES_EXPR_GT:
		case MES_EXPR_LTE:
		case MES_EXPR_GTE:
		case MES_EXPR_EQ:
		case MES_EXPR_NEQ:
			if (!(expr->sub_a = stack_pop(mes->index-1, stack, &stack_ptr)))
				goto error;
			if (!(expr->sub_b = stack_pop(mes->index-1, stack, &stack_ptr)))
				goto error;
			break;
		case MES_EXPR_RAND:
			if (target_game == GAME_DOUKYUUSEI) {
				// TODO: which other games do this?
				//       should test:
				//         Shuusaku (1998 CD version)
				//         Kakyuusei (1998 CD version)
				//         Kawarazaki-ke (1997 CD version)
				expr->sub_a = xcalloc(1, sizeof(struct mes_expression));
				expr->op = MES_EXPR_IMM16;
				expr->arg16 = buffer_read_u16(mes);
			} else {
				if (!(expr->sub_a = stack_pop(mes->index-1, stack, &stack_ptr)))
					goto error;
			}
			break;
		case MES_EXPR_IMM16:
			expr->arg16 = buffer_read_u16(mes);
			break;
		case MES_EXPR_IMM32:
			expr->arg32 = buffer_read_u32(mes);
			break;
		case MES_EXPR_REG16:
			expr->arg16 = buffer_read_u16(mes);
			break;
		case MES_EXPR_REG8:
			if (!(expr->sub_a = stack_pop(mes->index-1, stack, &stack_ptr)))
				goto error;
			break;
		case MES_EXPR_ARRAY32_GET32:
		case MES_EXPR_ARRAY32_GET16:
		case MES_EXPR_ARRAY32_GET8:
			expr->arg8 = buffer_read_u8(mes);
			if (!(expr->sub_a = stack_pop(mes->index-1, stack, &stack_ptr)))
				goto error;
			break;
		case MES_EXPR_VAR32:
			expr->arg8 = buffer_read_u8(mes);
			break;
		case MES_EXPR_END:
			if (unlikely(stack_ptr != 1)) {
				DC_ERROR(mes->index-1, "Invalid stack size at END expression");
				goto error;
			}
			free(expr);
			return stack[0];
		case MES_EXPR_IMM:
		default:
			expr->arg8 = b;
			break;
		}
		stack[stack_ptr++] = expr;
		continue;
error:
		if (expr->sub_a)
			mes_expression_free(expr->sub_a);
		if (expr->sub_b)
			mes_expression_free(expr->sub_b);
		free(expr);
		for (int i = 0; i < stack_ptr; i++) {
			mes_expression_free(stack[i]);
		}
		return NULL;
	}
}

static bool mes_parse_expression_list(struct buffer *mes, mes_expression_list *exprs)
{
	vector_init(*exprs);

	do {
		struct mes_expression *expr = mes_parse_expression(mes);
		if (!expr)
			goto error;
		vector_push(struct mes_expression*, *exprs, expr);
	} while (buffer_read_u8(mes));
	return true;
error:
	struct mes_expression *expr;
	vector_foreach(expr, *exprs) {
		mes_expression_free(expr);
	}
	vector_destroy(*exprs);
	vector_init(*exprs);
	return false;
}

static bool mes_parse_parameter_list(struct buffer *mes, mes_parameter_list *params)
{
	vector_init(*params);

	uint8_t b;
	for (int i = 0; (b = buffer_read_u8(mes)); i++) {
		vector_push(struct mes_parameter, *params, (struct mes_parameter){.type=b});
		if (b == MES_PARAM_STRING) {
			uint8_t c;
			int str_i;
			char *str = vector_A(*params, i).str;
			bool warned_overflow = false;
			for (str_i = 0; (c = buffer_read_u8(mes)); str_i++) {
				if (unlikely(str_i > 62)) {
					DC_ERROR(mes->index, "string parameter overflowed parse buffer");
					goto error;
				}
				if (unlikely(str_i > 22 && !warned_overflow)) {
					DC_WARNING(mes->index, "string parameter would overflow VM buffer");
					warned_overflow = true;
				}
				str[str_i] = c;
			}
			str[str_i] = '\0';
		} else if (b == MES_PARAM_EXPRESSION) {
			struct mes_expression *expr = mes_parse_expression(mes);
			if (!expr)
				goto error;
			vector_A(*params, i).expr = expr;
		} else {
			DC_ERROR(mes->index-1, "Unhandled parameter type: 0x%02x", (unsigned)b);
			goto error;
		}
	}
	return true;
error:
	struct mes_parameter p;
	vector_foreach(p, *params) {
		if (p.type == MES_PARAM_EXPRESSION && p.expr)
			mes_expression_free(p.expr);
	}
	vector_destroy(*params);
	vector_init(*params);
	return false;
}

static bool in_range(int v, int low, int high)
{
	return v >= low && v <= high;
}

static bool is_hankaku(uint8_t b)
{
	return !in_range(b, 0x81, 0x9f) && !in_range(b, 0xe0, 0xef);
}

static bool is_zenkaku(uint8_t b)
{
	return in_range(b, 0x81, 0x9f) || in_range(b, 0xe0, 0xef) || in_range(b, 0xfa, 0xfc);
}

#define TXT_BUF_SIZE 4096

static string mes_parse_txt(struct buffer *mes, bool *terminated)
{
	// FIXME: don't use fixed-size buffer
	size_t str_i = 0;
	char str[TXT_BUF_SIZE];

	uint8_t c;
	while ((c = buffer_peek_u8(mes))) {
		if (unlikely(str_i >= (TXT_BUF_SIZE - 7))) {
			DC_ERROR(mes->index, "TXT buffer overflow");
			return NULL;
		}
		if (unlikely(!is_zenkaku(c))) {
			DC_WARNING(mes->index, "Invalid byte in TXT statement: %02x", (unsigned)c);
			*terminated = false;
			goto unterminated;
		}
		if (sjis_char_is_valid(buffer_strdata(mes))) {
			str[str_i++] = buffer_read_u8(mes);
			str[str_i++] = buffer_read_u8(mes);
		} else {
			uint8_t b1 = buffer_read_u8(mes);
			uint8_t b2 = buffer_read_u8(mes);
			str_i += sprintf(str+str_i, "\\X%02x%02x", b1, b2);
		}
	}
	buffer_read_u8(mes);
	*terminated = true;
unterminated:
	str[str_i] = 0;
	return sjis_cstring_to_utf8(str, str_i);
}

static string mes_parse_str(struct buffer *mes, bool *terminated)
{
	// FIXME: don't use fixed-size buffer
	size_t str_i = 0;
	char str[TXT_BUF_SIZE];

	uint8_t c;
	while ((c = buffer_peek_u8(mes))) {
		if (unlikely(str_i >= (TXT_BUF_SIZE - 5))) {
			DC_ERROR(mes->index, "STR buffer overflow");
			return NULL;
		}
		if (unlikely(!is_hankaku(c))) {
			DC_WARNING(mes->index, "Invalid byte in STR statement: %02x", (unsigned)c);
			*terminated = false;
			goto unterminated;
		}
		switch (c) {
		case '\n':
			str[str_i++] = '\\';
			str[str_i++] = 'n';
			break;
		case '\t':
			str[str_i++] = '\\';
			str[str_i++] = 't';
			break;
		case '$':
			str[str_i++] = '\\';
			str[str_i++] = '$';
			break;
		case '\\':
			str[str_i++] = '\\';
			str[str_i++] = '\\';
			break;
		default:
			if (c > 0x7f || !isprint(c)) {
				str_i += sprintf(str+str_i, "\\x%02x", c);
			} else {
				str[str_i++] = c;
			}
		}
		buffer_read_u8(mes);
	}
	buffer_read_u8(mes);
	*terminated = true;
unterminated:
	str[str_i] = 0;
	return sjis_cstring_to_utf8(str, str_i);
}

#include "nulib/port.h"
static struct mes_statement *mes_parse_statement(struct buffer *mes)
{
	struct mes_statement *stmt = xcalloc(1, sizeof(struct mes_statement));
	stmt->address = mes->index;
	uint8_t b = buffer_read_u8(mes);
	switch ((stmt->op = parse_stmt_op(b))) {
	case MES_STMT_END:
		break;
	case MES_STMT_TXT:
		if (!(stmt->TXT.text = mes_parse_txt(mes, &stmt->TXT.terminated)))
			goto error;
		break;
	case MES_STMT_STR:
		if (!(stmt->TXT.text = mes_parse_str(mes, &stmt->TXT.terminated)))
			goto error;
		break;
	case MES_STMT_SETRBC:
		stmt->SETRBC.reg_no = buffer_read_u16(mes);
		if (!mes_parse_expression_list(mes, &stmt->SETRBC.exprs))
			goto error;
		break;
	case MES_STMT_SETV:
		stmt->SETV.var_no = buffer_read_u8(mes);
		if (!mes_parse_expression_list(mes, &stmt->SETV.exprs))
			goto error;
		break;
	case MES_STMT_SETRBE:
		if (!(stmt->SETRBE.reg_expr = mes_parse_expression(mes)))
			goto error;
		if (!mes_parse_expression_list(mes, &stmt->SETRBE.val_exprs)) {
			mes_expression_free(stmt->SETRBE.reg_expr);
			goto error;
		}
		break;
	case MES_STMT_SETAC:
		if (!(stmt->SETAC.off_expr = mes_parse_expression(mes)))
			goto error;
		stmt->SETAC.var_no = buffer_read_u8(mes);
		if (!mes_parse_expression_list(mes, &stmt->SETAC.val_exprs)) {
			mes_expression_free(stmt->SETAC.off_expr);
			goto error;
		}
		break;
	case MES_STMT_SETA_AT:
		if (!(stmt->SETA_AT.off_expr = mes_parse_expression(mes)))
			goto error;
		stmt->SETA_AT.var_no = buffer_read_u8(mes);
		if (!mes_parse_expression_list(mes, &stmt->SETA_AT.val_exprs)) {
			mes_expression_free(stmt->SETA_AT.off_expr);
			goto error;
		}
		break;
	case MES_STMT_SETAD:
		if (!(stmt->SETAD.off_expr = mes_parse_expression(mes)))
			goto error;
		stmt->SETAD.var_no = buffer_read_u8(mes);
		if (!mes_parse_expression_list(mes, &stmt->SETAD.val_exprs)) {
			mes_expression_free(stmt->SETAD.off_expr);
			goto error;
		}
		break;
	case MES_STMT_SETAW:
		if (!(stmt->SETAW.off_expr = mes_parse_expression(mes)))
			goto error;
		stmt->SETAW.var_no = buffer_read_u8(mes);
		if (!mes_parse_expression_list(mes, &stmt->SETAW.val_exprs)) {
			mes_expression_free(stmt->SETAW.off_expr);
			goto error;
		}
		break;
	case MES_STMT_SETAB:
		if (!(stmt->SETAB.off_expr = mes_parse_expression(mes)))
			goto error;
		stmt->SETAB.var_no = buffer_read_u8(mes);
		if (!mes_parse_expression_list(mes, &stmt->SETAB.val_exprs)) {
			mes_expression_free(stmt->SETAB.off_expr);
			goto error;
		}
		break;
	case MES_STMT_JZ:
		if (!(stmt->JZ.expr = mes_parse_expression(mes)))
			goto error;
		stmt->JZ.addr = buffer_read_u32(mes);
		break;
	case MES_STMT_JMP:
		stmt->JMP.addr = buffer_read_u32(mes);
		break;
	case MES_STMT_SYS:
		if (!(stmt->SYS.expr = mes_parse_expression(mes)))
			goto error;
		if (!mes_parse_parameter_list(mes, &stmt->SYS.params)) {
			mes_expression_free(stmt->SYS.expr);
			goto error;
		}
		break;
	case MES_STMT_GOTO:
		if (!mes_parse_parameter_list(mes, &stmt->GOTO.params))
			goto error;
		break;
	case MES_STMT_CALL:
		if (!mes_parse_parameter_list(mes, &stmt->CALL.params))
			goto error;
		break;
	case MES_STMT_MENUI:
		if (!mes_parse_parameter_list(mes, &stmt->MENUI.params))
			goto error;
		stmt->MENUI.addr = buffer_read_u32(mes);
		break;
	case MES_STMT_PROC:
		if (!mes_parse_parameter_list(mes, &stmt->PROC.params))
			goto error;
		break;
	case MES_STMT_UTIL:
		if (!mes_parse_parameter_list(mes, &stmt->UTIL.params))
			goto error;
		break;
	case MES_STMT_LINE:
		stmt->LINE.arg = buffer_read_u8(mes);
		break;
	case MES_STMT_PROCD:
		if (!(stmt->PROCD.no_expr = mes_parse_expression(mes)))
			goto error;
		stmt->PROCD.skip_addr = buffer_read_u32(mes);
		break;
	case MES_STMT_MENUS:
		break;
	case MES_STMT_SETRD:
		stmt->SETRD.var_no = buffer_read_u8(mes);
		if (!mes_parse_expression_list(mes, &stmt->SETRD.val_exprs))
			goto error;
		break;
	default:
		mes->index--;
		DC_WARNING(mes->index, "Unprefixed text: 0x%02x (possibly unhandled statement)", b);
		if (is_hankaku(buffer_peek_u8(mes))) {
			stmt->op = MES_STMT_STR;
			if (!(stmt->TXT.text = mes_parse_str(mes, &stmt->TXT.terminated)))
				goto error;
			stmt->TXT.unprefixed = true;
		} else {
			stmt->op = MES_STMT_TXT;
			if (!(stmt->TXT.text = mes_parse_txt(mes, &stmt->TXT.terminated)))
				goto error;
			stmt->TXT.unprefixed = true;
		}
	}
	stmt->next_address = mes->index;
	//port_printf(port_stdout(), "L_%08x:\n", stmt->address);
	//mes_statement_print(stmt, port_stdout());
	//fflush(stdout);
	return stmt;
error:
	free(stmt);
	return NULL;
}

declare_hashtable_int_type(addr_table, struct mes_statement*);
define_hashtable_int(addr_table, struct mes_statement*);

static void tag_jump_targets(mes_statement_list statements)
{
	hashtable_t(addr_table) table = hashtable_initializer(addr_table);

	struct mes_statement *p;
	vector_foreach(p, statements) {
		int ret;
		hashtable_iter_t k = hashtable_put(addr_table, &table, p->address, &ret);
		if (unlikely(ret == HASHTABLE_KEY_PRESENT))
			ERROR("multiple statements with same address");
		hashtable_val(&table, k) = p;
	}

	vector_foreach(p, statements) {
		hashtable_iter_t k;
		switch (p->op) {
		case MES_STMT_JZ:
			k = hashtable_get(addr_table, &table, p->JZ.addr);
			if (unlikely(k == hashtable_end(&table)))
				ERROR("invalid address in JZ statement");
			kh_value(&table, k)->is_jump_target = true;
			break;
		case MES_STMT_JMP:
			k = hashtable_get(addr_table, &table, p->JMP.addr);
			if (unlikely(k == hashtable_end(&table)))
				ERROR("invalid address in JMP statement");
			kh_value(&table, k)->is_jump_target = true;
			break;
		case MES_STMT_MENUI:
			k = hashtable_get(addr_table, &table, p->MENUI.addr);
			if (unlikely(k == hashtable_end(&table)))
				ERROR("invalid address in MENUI statement");
			kh_value(&table, k)->is_jump_target = true;
			break;
		case MES_STMT_PROCD:
			k = hashtable_get(addr_table, &table, p->PROCD.skip_addr);
			if (unlikely(k == hashtable_end(&table)))
				ERROR("invalid address in PROCD statement");
			kh_value(&table, k)->is_jump_target = true;
			break;
		default:
			break;
		}
	}

	hashtable_destroy(addr_table, &table);
}

bool mes_parse_statements(uint8_t *data, size_t data_size, mes_statement_list *statements)
{
	struct buffer mes;
	buffer_init(&mes, data, data_size);

	while (!buffer_end(&mes)) {
		struct mes_statement *stmt = mes_parse_statement(&mes);
		if (!stmt)
			goto error;
		vector_push(struct mes_statement*, *statements, stmt);
	}

	tag_jump_targets(*statements);
	return true;
error:
	struct mes_statement *stmt;
	vector_foreach(stmt, *statements) {
		mes_statement_free(stmt);
	}
	vector_destroy(*statements);
	vector_init(*statements);
	return false;
}

// parse }}}

void mes_expression_free(struct mes_expression *expr)
{
	if (!expr)
		return;
	mes_expression_free(expr->sub_a);
	mes_expression_free(expr->sub_b);
	free(expr);
}

void mes_expression_list_free(mes_expression_list list)
{
	for (unsigned i = 0; i < vector_length(list); i++) {
		mes_expression_free(vector_A(list, i));
	}
	vector_destroy(list);
}

void mes_parameter_list_free(mes_parameter_list list)
{
	for (unsigned i = 0; i < vector_length(list); i++) {
		if (vector_A(list, i).type == MES_PARAM_EXPRESSION)
			mes_expression_free(vector_A(list, i).expr);
	}
	vector_destroy(list);
}

void mes_statement_free(struct mes_statement *stmt)
{
	switch (stmt->op) {
	case MES_STMT_TXT:
		string_free(stmt->TXT.text);
		break;
	case MES_STMT_STR:
		string_free(stmt->TXT.text);
		break;
	case MES_STMT_SETRBC:
		mes_expression_list_free(stmt->SETRBC.exprs);
		break;
	case MES_STMT_SETV:
		mes_expression_list_free(stmt->SETV.exprs);
		break;
	case MES_STMT_SETRBE:
		mes_expression_free(stmt->SETRBE.reg_expr);
		mes_expression_list_free(stmt->SETRBE.val_exprs);
		break;
	case MES_STMT_SETAC:
		mes_expression_free(stmt->SETAC.off_expr);
		mes_expression_list_free(stmt->SETAC.val_exprs);
		break;
	case MES_STMT_SETA_AT:
		mes_expression_free(stmt->SETA_AT.off_expr);
		mes_expression_list_free(stmt->SETA_AT.val_exprs);
		break;
	case MES_STMT_SETAD:
		mes_expression_free(stmt->SETAD.off_expr);
		mes_expression_list_free(stmt->SETAD.val_exprs);
		break;
	case MES_STMT_SETAW:
		mes_expression_free(stmt->SETAW.off_expr);
		mes_expression_list_free(stmt->SETAW.val_exprs);
		break;
	case MES_STMT_SETAB:
		mes_expression_free(stmt->SETAB.off_expr);
		mes_expression_list_free(stmt->SETAB.val_exprs);
		break;
	case MES_STMT_JZ:
		mes_expression_free(stmt->JZ.expr);
		break;
	case MES_STMT_SYS:
		mes_expression_free(stmt->SYS.expr);
		mes_parameter_list_free(stmt->SYS.params);
		break;
	case MES_STMT_GOTO:
		mes_parameter_list_free(stmt->GOTO.params);
		break;
	case MES_STMT_CALL:
		mes_parameter_list_free(stmt->CALL.params);
		break;
	case MES_STMT_MENUI:
		mes_parameter_list_free(stmt->MENUI.params);
		break;
	case MES_STMT_PROC:
		mes_parameter_list_free(stmt->PROC.params);
		break;
	case MES_STMT_UTIL:
		mes_parameter_list_free(stmt->UTIL.params);
		break;
	case MES_STMT_PROCD:
		mes_expression_free(stmt->PROCD.no_expr);
		break;
	case MES_STMT_SETRD:
		mes_expression_list_free(stmt->SETRD.val_exprs);
		break;
	case MES_STMT_END:
	case MES_STMT_JMP:
	case MES_STMT_LINE:
	case MES_STMT_MENUS:
		break;
	}
	free(stmt);
}

void mes_statement_list_free(mes_statement_list list)
{
	for (unsigned i = 0; i < vector_length(list); i++) {
		mes_statement_free(vector_A(list, i));
	}
	vector_destroy(list);
}
