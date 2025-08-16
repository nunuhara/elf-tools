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
#include <ctype.h>

#include "nulib.h"
#include "nulib/buffer.h"
#include "nulib/string.h"
#include "nulib/utfsjis.h"
#include "ai5/game.h"

#include "mes.h"

void pack_string(struct buffer *mes, const string text, bool terminated, uint8_t term)
{
	const char *p = text;
	while (*p) {
		if (*p == '\\') {
			switch (*(++p)) {
			case 'n':  buffer_write_u8(mes, '\n'); p++; break;
			case 't':  buffer_write_u8(mes, '\t'); p++; break;
			case '$':  buffer_write_u8(mes, *p); p++; break;
			case '\\': buffer_write_u8(mes, *p); p++; break;
			case 'x':
				if (!isxdigit(p[1]) || !isxdigit(p[2])) {
					WARNING("invalid escape sequence in string");
					break;
				}
				char digits[3] = { p[1], p[2], '\0' };
				long n = strtol(digits, NULL, 16);
				buffer_write_u8(mes, (uint8_t)n);
				p += 3;
				break;
			case 'X':
				if (!isxdigit(p[1]) || !isxdigit(p[2]) || !isxdigit(p[3])
						|| !isxdigit(p[4])) {
					WARNING("invalid escape sequence in text: %s", text);
					break;
				}
				char d1[3] = { p[1], p[2], '\0' };
				char d2[3] = { p[3], p[4], '\0' };
				long n1 = strtol(d1, NULL, 16);
				long n2 = strtol(d2, NULL, 16);
				buffer_write_u8(mes, (uint8_t)n1);
				buffer_write_u8(mes, (uint8_t)n2);
				p += 5;
				break;
			case '\0':
				WARNING("invalid escape sequence in string: %s", text);
				break;
			default:
				WARNING("invalid escape sequence in string: %s", text);
				buffer_write_u8(mes, *p);
				p++;
				break;
			}
		} else {
			buffer_reserve(mes, 2);
			mes->index += utf8_char_to_sjis(buffer_strdata(mes), p, &p);
		}
	}
	if (terminated)
		buffer_write_u8(mes, term);
}

static void _pack_expression(struct buffer *mes, struct mes_expression *expr)
{
	if (expr->op == MES_EXPR_RAND && ai5_target_game == GAME_DOUKYUUSEI) {
		assert(expr->sub_a);
		buffer_write_u8(mes, mes_expr_opcode(MES_EXPR_RAND));
		switch (expr->sub_a->op) {
		case MES_EXPR_IMM:
			buffer_write_u16(mes, expr->sub_a->arg8);
			break;
		case MES_EXPR_IMM16:
			buffer_write_u16(mes, expr->sub_a->arg16);
			break;
		default:
			ERROR("Invalid expression as RAND argument");
		};
		return;
	}

	if (expr->sub_b)
		_pack_expression(mes, expr->sub_b);
	if (expr->sub_a)
		_pack_expression(mes, expr->sub_a);

	buffer_write_u8(mes, mes_expr_opcode(expr->op));
	switch (expr->op) {
	case MES_EXPR_IMM:
		mes->index--;
		buffer_write_u8(mes, expr->arg8);
		break;
	case MES_EXPR_GET_VAR16:
	case MES_EXPR_PTR16_GET16:
	case MES_EXPR_PTR16_GET8:
	case MES_EXPR_PTR32_GET32:
	case MES_EXPR_PTR32_GET16:
	case MES_EXPR_PTR32_GET8:
	case MES_EXPR_GET_VAR32:
		buffer_write_u8(mes, expr->arg8);
		break;
	case MES_EXPR_IMM16:
	case MES_EXPR_GET_FLAG_CONST:
	case MES_EXPR_GET_ARG_CONST:
		buffer_write_u16(mes, expr->arg16);
		break;
	case MES_EXPR_IMM32:
		buffer_write_u32(mes, expr->arg32);
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
	case MES_EXPR_RAND:
	case MES_EXPR_GET_FLAG_EXPR:
	case MES_EXPR_GET_ARG_EXPR:
	case MES_EXPR_END:
		break;
	}
}

static void pack_expression(struct buffer *mes, struct mes_expression *expr)
{
	_pack_expression(mes, expr);
	buffer_write_u8(mes, mes_expr_opcode(MES_EXPR_END));
}

static void pack_expression_list(struct buffer *mes, mes_expression_list exprs)
{
	if (vector_empty(exprs)) {
		buffer_write_u8(mes, 0);
		return;
	}

	struct mes_expression *expr;
	vector_foreach(expr, exprs) {
		pack_expression(mes, expr);
		buffer_write_u8(mes, 1);
	}
	mes->index--;
	buffer_write_u8(mes, 0);
}

static void pack_parameter_list(struct buffer *mes, mes_parameter_list params)
{
	struct mes_parameter *param;
	vector_foreach_p(param, params) {
		buffer_write_u8(mes, param->type);
		if (param->type == MES_PARAM_STRING) {
			pack_string(mes, param->str, true, 0);
		} else {
			pack_expression(mes, param->expr);
		}
	}
	buffer_write_u8(mes, 0);
}

static void pack_statement(struct buffer *mes, struct mes_statement *stmt)
{
	buffer_write_u8(mes, mes_stmt_opcode(stmt->op));
	switch (stmt->op) {
	case MES_STMT_END:
		break;
	case MES_STMT_ZENKAKU:
		if (stmt->TXT.unprefixed)
			mes->index--;
		pack_string(mes, stmt->TXT.text, stmt->TXT.terminated, 0);
		break;
	case MES_STMT_HANKAKU:
		if (stmt->TXT.unprefixed)
			mes->index--;
		pack_string(mes, stmt->TXT.text, stmt->TXT.terminated, 0);
		break;
	case MES_STMT_SET_FLAG_CONST:
	case MES_STMT_SET_ARG_CONST:
		buffer_write_u16(mes, stmt->SET_VAR_CONST.var_no);
		pack_expression_list(mes, stmt->SET_VAR_CONST.val_exprs);
		break;
	case MES_STMT_SET_VAR16:
	case MES_STMT_SET_VAR32:
		buffer_write_u8(mes, stmt->SET_VAR_CONST.var_no);
		pack_expression_list(mes, stmt->SET_VAR_CONST.val_exprs);
		break;
	case MES_STMT_SET_FLAG_EXPR:
	case MES_STMT_SET_ARG_EXPR:
		pack_expression(mes, stmt->SET_VAR_EXPR.var_expr);
		pack_expression_list(mes, stmt->SET_VAR_EXPR.val_exprs);
		break;
	case MES_STMT_PTR16_SET8:
	case MES_STMT_PTR16_SET16:
	case MES_STMT_PTR32_SET8:
	case MES_STMT_PTR32_SET16:
	case MES_STMT_PTR32_SET32:
		pack_expression(mes, stmt->PTR_SET.off_expr);
		buffer_write_u8(mes, stmt->PTR_SET.var_no);
		pack_expression_list(mes, stmt->PTR_SET.val_exprs);
		break;
	case MES_STMT_JZ:
		pack_expression(mes, stmt->JZ.expr);
		buffer_write_u32(mes, stmt->JZ.addr);
		break;
	case MES_STMT_JMP:
	case MES_STMT_1F:
		buffer_write_u32(mes, stmt->JMP.addr);
		break;
	case MES_STMT_SYS:
		pack_expression(mes, stmt->SYS.expr);
		pack_parameter_list(mes, stmt->SYS.params);
		break;
	case MES_STMT_JMP_MES:
	case MES_STMT_CALL_MES:
	case MES_STMT_CALL_PROC:
	case MES_STMT_UTIL:
	case MES_STMT_CALL_SUB:
	case MES_STMT_1B:
		pack_parameter_list(mes, stmt->CALL.params);
		break;
	case MES_STMT_DEF_MENU:
		pack_parameter_list(mes, stmt->DEF_MENU.params);
		buffer_write_u32(mes, stmt->DEF_MENU.skip_addr);
		break;
	case MES_STMT_LINE:
		buffer_write_u8(mes, stmt->LINE.arg);
		break;
	case MES_STMT_DEF_PROC:
	case MES_STMT_DEF_SUB:
		pack_expression(mes, stmt->DEF_PROC.no_expr);
		buffer_write_u32(mes, stmt->DEF_PROC.skip_addr);
		break;
	case MES_STMT_MENU_EXEC:
		if (ai5_target_game == GAME_NONOMURA)
			pack_parameter_list(mes, stmt->DEF_MENU.params);
		break;
	case MES_STMT_17:
		buffer_write_u32(mes, stmt->JMP.addr);
		break;
	case MES_STMT_18:
		pack_expression(mes, stmt->SET_VAR_EXPR.var_expr);
		break;
	case MES_STMT_19:
	case MES_STMT_1A:
		break;
	}
}

static void aiw_pack_statement(struct buffer *mes, struct mes_statement *stmt);

uint8_t *mes_pack(mes_statement_list stmts, size_t *size_out)
{
	void (*pack)(struct buffer*,struct mes_statement*) = game_is_aiwin()
		? aiw_pack_statement
		: pack_statement;
	struct buffer mes;
	buffer_init(&mes, NULL, 0);

	struct mes_statement *stmt;
	vector_foreach(stmt, stmts) {
		pack(&mes, stmt);
	}

	// addr table
	if (ai5_target_game == GAME_NONOMURA) {
		struct buffer tab;
		buffer_init(&tab, NULL, 0);
		buffer_write_u32(&tab, 0);
		unsigned count = 0;
		vector_foreach(stmt, stmts) {
			if (stmt->op == MES_STMT_17) {
				buffer_write_u32(&tab, stmt->address);
				count++;
			}
		}
		buffer_write_u32_at(&tab, 0, count);
		buffer_write_bytes(&tab, mes.buf, mes.index);
		free(mes.buf);
		mes = tab;
	} else if (ai5_target_game == GAME_KAWARAZAKIKE) {
		// TODO: test if this actually works in-game
		// FIXME: avoid extra allocation/copy
		uint8_t *out = xmalloc(mes.index + 4);
		le_put32(out, 0, 0);
		memcpy(out + 4, mes.buf, mes.index);
		free(mes.buf);
		*size_out = mes.index + 4;
		return out;
	}

	*size_out = mes.index;
	return mes.buf;
}

static uint8_t aiw_mes_expr_opcode(enum aiw_mes_expression_op op)
{
	return op;
}

static void _aiw_pack_expression(struct buffer *mes, struct mes_expression *expr)
{
	if (expr->sub_b)
		_aiw_pack_expression(mes, expr->sub_b);
	if (expr->sub_a)
		_aiw_pack_expression(mes, expr->sub_a);

	buffer_write_u8(mes, aiw_mes_expr_opcode(expr->aiw_op));
	switch (expr->aiw_op) {
	case AIW_MES_EXPR_IMM:
		mes->index--;
		buffer_write_u8(mes, expr->arg8);
		break;
	case AIW_MES_EXPR_VAR32:
		mes->index--;
		buffer_write_u8(mes, expr->arg8 + 0x80);
		break;
	case AIW_MES_EXPR_PTR_GET8:
		mes->index--;
		buffer_write_u8(mes, expr->arg8 + 0xa0);
		break;
	case AIW_MES_EXPR_RAND:
	case AIW_MES_EXPR_IMM16:
	case AIW_MES_EXPR_GET_FLAG_CONST:
	case AIW_MES_EXPR_GET_VAR16_CONST:
	case AIW_MES_EXPR_GET_SYSVAR_CONST:
		buffer_write_u16(mes, expr->arg16);
		break;
	case AIW_MES_EXPR_IMM32:
		buffer_write_u32(mes, expr->arg32);
		break;
	case AIW_MES_EXPR_PLUS:
	case AIW_MES_EXPR_MINUS:
	case AIW_MES_EXPR_MUL:
	case AIW_MES_EXPR_DIV:
	case AIW_MES_EXPR_MOD:
	case AIW_MES_EXPR_AND:
	case AIW_MES_EXPR_OR:
	case AIW_MES_EXPR_BITAND:
	case AIW_MES_EXPR_BITIOR:
	case AIW_MES_EXPR_BITXOR:
	case AIW_MES_EXPR_LT:
	case AIW_MES_EXPR_GT:
	case AIW_MES_EXPR_LTE:
	case AIW_MES_EXPR_GTE:
	case AIW_MES_EXPR_EQ:
	case AIW_MES_EXPR_NEQ:
	case AIW_MES_EXPR_GET_FLAG_EXPR:
	case AIW_MES_EXPR_GET_VAR16_EXPR:
	case AIW_MES_EXPR_GET_SYSVAR_EXPR:
	case AIW_MES_EXPR_END:
		break;
	}
}

static void aiw_pack_expression(struct buffer *mes, struct mes_expression *expr)
{
	_aiw_pack_expression(mes, expr);
	buffer_write_u8(mes, aiw_mes_expr_opcode(AIW_MES_EXPR_END));
}

static void aiw_pack_expression_list(struct buffer *mes, mes_expression_list exprs)
{
	struct mes_expression *expr;
	vector_foreach(expr, exprs) {
		aiw_pack_expression(mes, expr);
	}
	buffer_write_u8(mes, 0xff);
}

static void aiw_pack_parameter_list(struct buffer *mes, mes_parameter_list params)
{
	if (vector_empty(params)) {
		buffer_write_u8(mes, 0xff);
		return;
	}

	struct mes_parameter *p;
	vector_foreach_p(p, params) {
		if (p->type == MES_PARAM_STRING) {
			buffer_write_u8(mes, 0xf5);
			pack_string(mes, p->str, true, 0xff);
		} else {
			aiw_pack_expression(mes, p->expr);
		}
	}

	buffer_write_u8(mes, 0xff);
}

struct aiw_menu_table_entry {
	uint32_t cond_addr;
	uint32_t body_addr;
};

static void aiw_pack_defmenu(struct buffer *mes, struct mes_statement *stmt)
{
	aiw_pack_expression(mes, stmt->AIW_DEF_MENU.expr);

	// table address will be written later
	size_t table_addr_pos = mes->index;
	mes->index += 4;

	struct aiw_menu_table_entry *table = xcalloc(vector_length(stmt->AIW_DEF_MENU.cases),
			sizeof(struct aiw_menu_table_entry));

	// write conditions/bodies, recording addresses in table
	int i = 0;
	struct aiw_mes_menu_case *c;
	vector_foreach_p(c, stmt->AIW_DEF_MENU.cases) {
		if (c->cond) {
			table[i].cond_addr = mes->index;
			aiw_pack_expression(mes, c->cond);
		}
		table[i].body_addr = mes->index;
		struct mes_statement *s;
		vector_foreach(s, c->body) {
			aiw_pack_statement(mes, s);
		}
		i++;
	}

	// write table
	buffer_write_u32_at(mes, table_addr_pos, mes->index);
	buffer_write_u8(mes, vector_length(stmt->AIW_DEF_MENU.cases));
	for (int i = 0; i < vector_length(stmt->AIW_DEF_MENU.cases); i++) {
		buffer_write_u32(mes, table[i].cond_addr);
		buffer_write_u32(mes, table[i].body_addr);
	}

	free(table);
}

static uint8_t aiw_mes_stmt_opcode(enum aiw_mes_statement_op op)
{
	return op;
}

static void aiw_pack_statement(struct buffer *mes, struct mes_statement *stmt)
{
	buffer_write_u8(mes, aiw_mes_stmt_opcode(stmt->aiw_op));
	switch (stmt->aiw_op) {
	case AIW_MES_STMT_21:
	case AIW_MES_STMT_FE:
	case AIW_MES_STMT_END:
		break;
	case AIW_MES_STMT_TXT:
		pack_string(mes, stmt->TXT.text, stmt->TXT.terminated,
				ai5_target_game == GAME_KAWARAZAKIKE ? 0 : 0xff);
		break;
	case AIW_MES_STMT_JMP:
		buffer_write_u32(mes, stmt->JMP.addr);
		break;
	case AIW_MES_STMT_UTIL:
	case AIW_MES_STMT_JMP_MES:
	case AIW_MES_STMT_CALL_MES:
	case AIW_MES_STMT_LOAD:
	case AIW_MES_STMT_SAVE:
	case AIW_MES_STMT_CALL_PROC:
	case AIW_MES_STMT_NUM:
	case AIW_MES_STMT_SET_TEXT_COLOR:
	case AIW_MES_STMT_WAIT:
	case AIW_MES_STMT_LOAD_IMAGE:
	case AIW_MES_STMT_SURF_COPY:
	case AIW_MES_STMT_SURF_COPY_MASKED:
	case AIW_MES_STMT_SURF_SWAP:
	case AIW_MES_STMT_SURF_FILL:
	case AIW_MES_STMT_SURF_INVERT:
	case AIW_MES_STMT_29:
	case AIW_MES_STMT_SHOW_HIDE:
	case AIW_MES_STMT_CROSSFADE:
	case AIW_MES_STMT_CROSSFADE2:
	case AIW_MES_STMT_CURSOR:
	case AIW_MES_STMT_ANIM:
	case AIW_MES_STMT_LOAD_AUDIO:
	case AIW_MES_STMT_LOAD_EFFECT:
	case AIW_MES_STMT_LOAD_VOICE:
	case AIW_MES_STMT_AUDIO:
	case AIW_MES_STMT_PLAY_MOVIE:
	case AIW_MES_STMT_34:
		aiw_pack_parameter_list(mes, stmt->CALL.params);
		break;
	case AIW_MES_STMT_SET_FLAG_CONST:
	case AIW_MES_STMT_SET_VAR16_CONST:
	case AIW_MES_STMT_SET_SYSVAR_CONST:
		buffer_write_u16(mes, stmt->SET_VAR_CONST.var_no);
		aiw_pack_expression_list(mes, stmt->SET_VAR_CONST.val_exprs);
		break;
	case AIW_MES_STMT_SET_FLAG_EXPR:
	case AIW_MES_STMT_SET_VAR16_EXPR:
	case AIW_MES_STMT_SET_SYSVAR_EXPR:
		aiw_pack_expression(mes, stmt->SET_VAR_EXPR.var_expr);
		aiw_pack_expression_list(mes, stmt->SET_VAR_EXPR.val_exprs);
		break;
	case AIW_MES_STMT_SET_VAR32:
		buffer_write_u8(mes, stmt->SET_VAR_CONST.var_no);
		aiw_pack_expression(mes, vector_A(stmt->SET_VAR_CONST.val_exprs, 0));
		break;
	case AIW_MES_STMT_PTR_SET8:
	case AIW_MES_STMT_PTR_SET16:
		buffer_write_u8(mes, stmt->PTR_SET.var_no);
		aiw_pack_expression(mes, stmt->PTR_SET.off_expr);
		aiw_pack_expression_list(mes, stmt->PTR_SET.val_exprs);
		break;
	case AIW_MES_STMT_JZ:
		aiw_pack_expression(mes, stmt->JZ.expr);
		buffer_write_u32(mes, stmt->JZ.addr);
		break;
	case AIW_MES_STMT_DEF_PROC:
		aiw_pack_expression(mes, stmt->DEF_PROC.no_expr);
		buffer_write_u32(mes, stmt->DEF_PROC.skip_addr);
		break;
	case AIW_MES_STMT_DEF_MENU:
		aiw_pack_defmenu(mes, stmt);
		break;
	case AIW_MES_STMT_MENU_EXEC:
		aiw_pack_expression_list(mes, stmt->AIW_MENU_EXEC.exprs);
		break;
	case AIW_MES_STMT_COMMIT_MESSAGE:
		if (ai5_target_game == GAME_KAWARAZAKIKE) {
			aiw_pack_parameter_list(mes, stmt->CALL.params);
		}
		break;
	case AIW_MES_STMT_35:
		buffer_write_u16(mes, stmt->AIW_0x35.a);
		buffer_write_u16(mes, stmt->AIW_0x35.b);
		break;
	case AIW_MES_STMT_37:
		buffer_write_u32(mes, stmt->JMP.addr);
		break;
	}
}
