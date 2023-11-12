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

#include <ctype.h>

#include "nulib.h"
#include "nulib/string.h"
#include "nulib/utfsjis.h"
#include "ai5/game.h"
#include "mes.h"

static uint32_t _expression_size(struct mes_expression *expr)
{
	uint32_t len = 1;
	switch (expr->op) {
	case MES_EXPR_VAR:
	case MES_EXPR_VAR32:
		len++;
		break;
	case MES_EXPR_ARRAY16_GET16:
	case MES_EXPR_ARRAY16_GET8:
	case MES_EXPR_ARRAY32_GET32:
	case MES_EXPR_ARRAY32_GET16:
	case MES_EXPR_ARRAY32_GET8:
		len += 1 + _expression_size(expr->sub_a);
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
		len += _expression_size(expr->sub_a) + _expression_size(expr->sub_b);
		break;
	case MES_EXPR_RAND:
		if (ai5_target_game == GAME_DOUKYUUSEI) {
			len += 2;
		} else {
			len += _expression_size(expr->sub_a);
		}
		break;
	case MES_EXPR_IMM16:
	case MES_EXPR_REG16:
		len += 2;
		break;
	case MES_EXPR_IMM32:
		len += 4;
		break;
	case MES_EXPR_REG8:
		len += _expression_size(expr->sub_a);
		break;
	case MES_EXPR_END:
		ERROR("MES_EXPR_END in mes_expression");
		break;
	default:
		break;
	}
	return len;
}

static uint32_t expression_size(struct mes_expression *expr)
{
	return _expression_size(expr) + 1;
}

static uint32_t expression_list_size(mes_expression_list expressions)
{
	uint32_t len = 0;
	struct mes_expression *expr;
	vector_foreach(expr, expressions) {
		len += expression_size(expr) + 1;
	}
	return len;
}

static uint32_t string_param_size(string str)
{
	uint32_t len = 0;
	const char *p = str;
	while (*p) {
		if (*p == '\\') {
			switch (p[1]) {
			case 'X': p += 6; len += 2; break;
			case 'x': p += 4; len += 1; break;
			default:  p += 2; len += 1; break;
			}
		} else {
			len += utf8_sjis_char_length(p, &p);
		}
	}
	return len;
}

static uint32_t parameter_list_size(mes_parameter_list params)
{
	uint32_t len = 0;
	struct mes_parameter *p;
	vector_foreach_p(p, params) {
		len++;
		if (p->type == MES_PARAM_STRING) {
			len += string_param_size(p->str) + 1;
		} else {
			len += expression_size(p->expr);
		}
	}
	return len + 1;
}

static uint32_t txt_size(string text)
{
	uint32_t len = 0;
	const char *p = text;
	while (*p) {
		if (p[0] == '\\') {
			assert(p[1] == 'X' && isxdigit(p[2]) && isxdigit(p[3])
					&& isxdigit(p[4]) && isxdigit(p[5]));
			p += 6;
			len += 2;
		} else {
			len += utf8_sjis_char_length(p, &p);
		}
	}
	return len;
}

static uint32_t str_size(string str)
{
	uint32_t len = 0;
	for (int i = 0; str[i];) {
		if (str[i] == '\\') {
			if (str[i+1] == 'x')
				i += 4;
			else
				i += 2;
			len++;
		} else {
			i++;
			len++;
		}
	}
	return len;
}

uint32_t mes_statement_size(struct mes_statement *stmt)
{
	uint32_t len = 1;
	switch (stmt->op) {
	case MES_STMT_END:
		break;
	case MES_STMT_TXT:
	case MES_STMT_STR:
		// FIXME: doesn't handle escapes properly
		if (stmt->op == MES_STMT_TXT) {
			len += txt_size(stmt->TXT.text);
		} else {
			len += str_size(stmt->TXT.text);
		}
		if (stmt->TXT.terminated)
			len++;
		if (stmt->TXT.unprefixed)
			len--;
		break;
	case MES_STMT_SETRBC:
		len += 2 + expression_list_size(stmt->SETRBC.exprs);
		break;
	case MES_STMT_SETV:
		len += 1 + expression_list_size(stmt->SETV.exprs);
		break;
	case MES_STMT_SETRBE:
		len += expression_size(stmt->SETRBE.reg_expr);
		len += expression_list_size(stmt->SETRBE.val_exprs);
		break;
	case MES_STMT_SETAC:
		len += expression_size(stmt->SETAC.off_expr) + 1;
		len += expression_list_size(stmt->SETAC.val_exprs);
		break;
	case MES_STMT_SETA_AT:
		len += expression_size(stmt->SETA_AT.off_expr) + 1;
		len += expression_list_size(stmt->SETA_AT.val_exprs);
		break;
	case MES_STMT_SETAD:
		len += expression_size(stmt->SETAD.off_expr) + 1;
		len += expression_list_size(stmt->SETAD.val_exprs);
		break;
	case MES_STMT_SETAW:
		len += expression_size(stmt->SETAB.off_expr) + 1;
		len += expression_list_size(stmt->SETAB.val_exprs);
		break;
	case MES_STMT_SETAB:
		len += expression_size(stmt->SETAB.off_expr) + 1;
		len += expression_list_size(stmt->SETAB.val_exprs);
		break;
	case MES_STMT_JZ:
		len += expression_size(stmt->JZ.expr) + 4;
		break;
	case MES_STMT_JMP:
		len += 4;
		break;
	case MES_STMT_SYS:
		len += expression_size(stmt->SYS.expr);
		len += parameter_list_size(stmt->SYS.params);
		break;
	case MES_STMT_GOTO:
		len += parameter_list_size(stmt->GOTO.params);
		break;
	case MES_STMT_CALL:
		len += parameter_list_size(stmt->CALL.params);
		break;
	case MES_STMT_MENUI:
		len += parameter_list_size(stmt->MENUI.params) + 4;
		break;
	case MES_STMT_PROC:
		len += parameter_list_size(stmt->PROC.params);
		break;
	case MES_STMT_UTIL:
		len += parameter_list_size(stmt->UTIL.params);
		break;
	case MES_STMT_LINE:
		len++;
		break;
	case MES_STMT_PROCD:
		len += expression_size(stmt->PROCD.no_expr) + 4;
		break;
	case MES_STMT_MENUS:
		break;
	case MES_STMT_SETRD:
		len += 1 + expression_list_size(stmt->SETRD.val_exprs);
		break;
	default:
		ERROR("invalid statement type");
	}
	return len;
}


