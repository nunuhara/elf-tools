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
	case MES_EXPR_GET_VAR16:
	case MES_EXPR_GET_VAR32:
		len++;
		break;
	case MES_EXPR_PTR16_GET16:
	case MES_EXPR_PTR16_GET8:
	case MES_EXPR_PTR32_GET32:
	case MES_EXPR_PTR32_GET16:
	case MES_EXPR_PTR32_GET8:
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
	case MES_EXPR_GET_FLAG_CONST:
		len += 2;
		break;
	case MES_EXPR_IMM32:
		len += 4;
		break;
	case MES_EXPR_GET_FLAG_EXPR:
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
	case MES_STMT_ZENKAKU:
	case MES_STMT_HANKAKU:
		// FIXME: doesn't handle escapes properly
		if (stmt->op == MES_STMT_ZENKAKU) {
			len += txt_size(stmt->TXT.text);
		} else {
			len += str_size(stmt->TXT.text);
		}
		if (stmt->TXT.terminated)
			len++;
		if (stmt->TXT.unprefixed)
			len--;
		break;
	case MES_STMT_SET_FLAG_CONST:
		len += 2 + expression_list_size(stmt->SET_VAR_CONST.val_exprs);
		break;
	case MES_STMT_SET_VAR16:
	case MES_STMT_SET_VAR32:
		len += 1 + expression_list_size(stmt->SET_VAR_CONST.val_exprs);
		break;
	case MES_STMT_SET_FLAG_EXPR:
		len += expression_size(stmt->SET_VAR_EXPR.var_expr);
		len += expression_list_size(stmt->SET_VAR_EXPR.val_exprs);
		break;
	case MES_STMT_PTR16_SET8:
	case MES_STMT_PTR16_SET16:
	case MES_STMT_PTR32_SET8:
	case MES_STMT_PTR32_SET16:
	case MES_STMT_PTR32_SET32:
		len += expression_size(stmt->PTR_SET.off_expr) + 1;
		len += expression_list_size(stmt->PTR_SET.val_exprs);
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
	case MES_STMT_JMP_MES:
	case MES_STMT_CALL_MES:
	case MES_STMT_CALL_PROC:
	case MES_STMT_UTIL:
		len += parameter_list_size(stmt->CALL.params);
		break;
	case MES_STMT_DEF_MENU:
		len += parameter_list_size(stmt->DEF_MENU.params) + 4;
		break;
	case MES_STMT_LINE:
		len++;
		break;
	case MES_STMT_DEF_PROC:
		len += expression_size(stmt->DEF_PROC.no_expr) + 4;
		break;
	case MES_STMT_MENU_EXEC:
		break;
	default:
		ERROR("invalid statement type");
	}
	return len;
}


