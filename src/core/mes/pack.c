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

void pack_string(struct buffer *mes, const string text, bool terminated)
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
		buffer_write_u8(mes, 0);
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
	case MES_EXPR_VAR:
	case MES_EXPR_ARRAY16_GET16:
	case MES_EXPR_ARRAY16_GET8:
	case MES_EXPR_ARRAY32_GET32:
	case MES_EXPR_ARRAY32_GET16:
	case MES_EXPR_ARRAY32_GET8:
	case MES_EXPR_VAR32:
		buffer_write_u8(mes, expr->arg8);
		break;
	case MES_EXPR_IMM16:
	case MES_EXPR_REG16:
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
	case MES_EXPR_REG8:
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
			pack_string(mes, param->str, true);
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
	case MES_STMT_TXT:
		if (stmt->TXT.unprefixed)
			mes->index--;
		pack_string(mes, stmt->TXT.text, stmt->TXT.terminated);
		break;
	case MES_STMT_STR:
		if (stmt->TXT.unprefixed)
			mes->index--;
		pack_string(mes, stmt->TXT.text, stmt->TXT.terminated);
		break;
	case MES_STMT_SETRBC:
		buffer_write_u16(mes, stmt->SETRBC.reg_no);
		pack_expression_list(mes, stmt->SETRBC.exprs);
		break;
	case MES_STMT_SETV:
		buffer_write_u8(mes, stmt->SETV.var_no);
		pack_expression_list(mes, stmt->SETV.exprs);
		break;
	case MES_STMT_SETRBE:
		pack_expression(mes, stmt->SETRBE.reg_expr);
		pack_expression_list(mes, stmt->SETRBE.val_exprs);
		break;
	case MES_STMT_SETAC:
		pack_expression(mes, stmt->SETAC.off_expr);
		buffer_write_u8(mes, stmt->SETAC.var_no);
		pack_expression_list(mes, stmt->SETAC.val_exprs);
		break;
	case MES_STMT_SETA_AT:
		pack_expression(mes, stmt->SETA_AT.off_expr);
		buffer_write_u8(mes, stmt->SETA_AT.var_no);
		pack_expression_list(mes, stmt->SETA_AT.val_exprs);
		break;
	case MES_STMT_SETAD:
		pack_expression(mes, stmt->SETAD.off_expr);
		buffer_write_u8(mes, stmt->SETAD.var_no);
		pack_expression_list(mes, stmt->SETAD.val_exprs);
		break;
	case MES_STMT_SETAW:
		pack_expression(mes, stmt->SETAW.off_expr);
		buffer_write_u8(mes, stmt->SETAW.var_no);
		pack_expression_list(mes, stmt->SETAW.val_exprs);
		break;
	case MES_STMT_SETAB:
		pack_expression(mes, stmt->SETAB.off_expr);
		buffer_write_u8(mes, stmt->SETAB.var_no);
		pack_expression_list(mes, stmt->SETAB.val_exprs);
		break;
	case MES_STMT_JZ:
		pack_expression(mes, stmt->JZ.expr);
		buffer_write_u32(mes, stmt->JZ.addr);
		break;
	case MES_STMT_JMP:
		buffer_write_u32(mes, stmt->JMP.addr);
		break;
	case MES_STMT_SYS:
		pack_expression(mes, stmt->SYS.expr);
		pack_parameter_list(mes, stmt->SYS.params);
		break;
	case MES_STMT_GOTO:
		pack_parameter_list(mes, stmt->GOTO.params);
		break;
	case MES_STMT_CALL:
		pack_parameter_list(mes, stmt->CALL.params);
		break;
	case MES_STMT_MENUI:
		pack_parameter_list(mes, stmt->MENUI.params);
		buffer_write_u32(mes, stmt->MENUI.addr);
		break;
	case MES_STMT_PROC:
		pack_parameter_list(mes, stmt->PROC.params);
		break;
	case MES_STMT_UTIL:
		pack_parameter_list(mes, stmt->UTIL.params);
		break;
	case MES_STMT_LINE:
		buffer_write_u8(mes, stmt->LINE.arg);
		break;
	case MES_STMT_PROCD:
		pack_expression(mes, stmt->PROCD.no_expr);
		buffer_write_u32(mes, stmt->PROCD.skip_addr);
		break;
	case MES_STMT_MENUS:
		break;
	case MES_STMT_SETRD:
		buffer_write_u8(mes, stmt->SETRD.var_no);
		pack_expression_list(mes, stmt->SETRD.val_exprs);
		break;
	}
}

uint8_t *mes_pack(mes_statement_list stmts, size_t *size_out)
{
	struct buffer mes;
	buffer_init(&mes, NULL, 0);

	struct mes_statement *stmt;
	vector_foreach(stmt, stmts) {
		pack_statement(&mes, stmt);
	}

	*size_out = mes.index;
	return mes.buf;
}
