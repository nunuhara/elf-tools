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
	case MES_EXPR_GET_ARG_CONST:
		len += 2;
		break;
	case MES_EXPR_IMM32:
		len += 4;
		break;
	case MES_EXPR_GET_FLAG_EXPR:
	case MES_EXPR_GET_ARG_EXPR:
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
	if (game_is_aiwin())
		return aiw_mes_statement_size(stmt);

	uint32_t len = 1;
	switch (stmt->op) {
	case MES_STMT_END:
		return len;
	case MES_STMT_ZENKAKU:
	case MES_STMT_HANKAKU:
		if (stmt->op == MES_STMT_ZENKAKU) {
			len += txt_size(stmt->TXT.text);
		} else {
			len += str_size(stmt->TXT.text);
		}
		if (stmt->TXT.terminated)
			len++;
		if (stmt->TXT.unprefixed)
			len--;
		return len;
	case MES_STMT_SET_FLAG_CONST:
	case MES_STMT_SET_ARG_CONST:
		len += 2 + expression_list_size(stmt->SET_VAR_CONST.val_exprs);
		return len;
	case MES_STMT_SET_VAR16:
	case MES_STMT_SET_VAR32:
		len += 1 + expression_list_size(stmt->SET_VAR_CONST.val_exprs);
		return len;
	case MES_STMT_SET_FLAG_EXPR:
	case MES_STMT_SET_ARG_EXPR:
		len += expression_size(stmt->SET_VAR_EXPR.var_expr);
		len += expression_list_size(stmt->SET_VAR_EXPR.val_exprs);
		return len;
	case MES_STMT_PTR16_SET8:
	case MES_STMT_PTR16_SET16:
	case MES_STMT_PTR32_SET8:
	case MES_STMT_PTR32_SET16:
	case MES_STMT_PTR32_SET32:
		len += expression_size(stmt->PTR_SET.off_expr) + 1;
		len += expression_list_size(stmt->PTR_SET.val_exprs);
		return len;
	case MES_STMT_JZ:
		len += expression_size(stmt->JZ.expr) + 4;
		return len;
	case MES_STMT_JMP:
	case MES_STMT_17:
	case MES_STMT_1F:
		len += 4;
		return len;
	case MES_STMT_SYS:
		len += expression_size(stmt->SYS.expr);
		len += parameter_list_size(stmt->SYS.params);
		return len;
	case MES_STMT_JMP_MES:
	case MES_STMT_CALL_MES:
	case MES_STMT_CALL_PROC:
	case MES_STMT_UTIL:
	case MES_STMT_CALL_SUB:
	case MES_STMT_1B:
		len += parameter_list_size(stmt->CALL.params);
		return len;
	case MES_STMT_DEF_MENU:
		len += parameter_list_size(stmt->DEF_MENU.params) + 4;
		return len;
	case MES_STMT_LINE:
		len++;
		return len;
	case MES_STMT_DEF_PROC:
	case MES_STMT_DEF_SUB:
		len += expression_size(stmt->DEF_PROC.no_expr) + 4;
		return len;
	case MES_STMT_MENU_EXEC:
		if (ai5_target_game == GAME_NONOMURA)
			len += parameter_list_size(stmt->DEF_MENU.params);
		return len;
	case MES_STMT_18:
		len += expression_size(stmt->SET_VAR_EXPR.var_expr);
		return len;
	case MES_STMT_19:
	case MES_STMT_1A:
		return len;
	}
	ERROR("invalid statement type");
}

static uint32_t _aiw_expression_size(struct mes_expression *expr)
{
	uint32_t len = 1;
	switch (expr->aiw_op) {
	case AIW_MES_EXPR_IMM:
	case AIW_MES_EXPR_VAR32:
	case AIW_MES_EXPR_PTR_GET8:
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
		len += _aiw_expression_size(expr->sub_a) + _aiw_expression_size(expr->sub_b);
		break;
	case AIW_MES_EXPR_RAND:
	case AIW_MES_EXPR_IMM16:
	case AIW_MES_EXPR_GET_FLAG_CONST:
	case AIW_MES_EXPR_GET_VAR16_CONST:
	case AIW_MES_EXPR_GET_SYSVAR_CONST:
		len += 2;
		break;
	case AIW_MES_EXPR_IMM32:
		len += 4;
		break;
	case AIW_MES_EXPR_GET_FLAG_EXPR:
	case AIW_MES_EXPR_GET_VAR16_EXPR:
	case AIW_MES_EXPR_GET_SYSVAR_EXPR:
		len += _aiw_expression_size(expr->sub_a);
		break;
	case AIW_MES_EXPR_END:
		ERROR("MES_EXPR_END in mes_expression");
	default:
		ERROR("invalid expression type");
	}
	return len;
}

static uint32_t aiw_expression_size(struct mes_expression *expr)
{
	return _aiw_expression_size(expr) + 1;
}

static uint32_t aiw_expression_list_size(mes_expression_list expressions)
{
	uint32_t len = 0;
	struct mes_expression *expr;
	vector_foreach(expr, expressions) {
		len += aiw_expression_size(expr);
	}
	return len + 1;
}

static uint32_t aiw_parameter_list_size(mes_parameter_list params)
{
	uint32_t len = 0;
	struct mes_parameter *p;
	vector_foreach_p(p, params) {
		if (p->type == MES_PARAM_STRING) {
			len += string_param_size(p->str) + 2;
		} else {
			len += aiw_expression_size(p->expr);
		}
	}
	len++;
	return len;
}

uint32_t aiw_mes_statement_size(struct mes_statement *stmt)
{
	uint32_t len = 1;
	switch (stmt->aiw_op) {
	case AIW_MES_STMT_21:
	case AIW_MES_STMT_FE:
	case AIW_MES_STMT_END:
		break;
	case AIW_MES_STMT_TXT:
		len += txt_size(stmt->TXT.text);
		if (stmt->TXT.terminated)
			len++;
		if (stmt->TXT.unprefixed)
			len--;
		break;
	case AIW_MES_STMT_JMP:
		len += 4;
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
		len += aiw_parameter_list_size(stmt->CALL.params);
		break;
	case AIW_MES_STMT_SET_FLAG_CONST:
	case AIW_MES_STMT_SET_VAR16_CONST:
	case AIW_MES_STMT_SET_SYSVAR_CONST:
		len += 2;
		len += aiw_expression_list_size(stmt->SET_VAR_CONST.val_exprs);
		break;
	case AIW_MES_STMT_SET_FLAG_EXPR:
	case AIW_MES_STMT_SET_VAR16_EXPR:
	case AIW_MES_STMT_SET_SYSVAR_EXPR:
		len += aiw_expression_size(stmt->SET_VAR_EXPR.var_expr);
		len += aiw_expression_list_size(stmt->SET_VAR_EXPR.val_exprs);
		break;
	case AIW_MES_STMT_SET_VAR32:
		len += 1;
		len += aiw_expression_size(vector_A(stmt->SET_VAR_CONST.val_exprs, 0));
		break;
	case AIW_MES_STMT_PTR_SET8:
	case AIW_MES_STMT_PTR_SET16:
		len += 1;
		len += aiw_expression_size(stmt->PTR_SET.off_expr);
		len += aiw_expression_list_size(stmt->PTR_SET.val_exprs);
		break;
	case AIW_MES_STMT_JZ:
		len += aiw_expression_size(stmt->JZ.expr);
		len += 4;
		break;
	case AIW_MES_STMT_DEF_PROC:
		len += aiw_expression_size(stmt->DEF_PROC.no_expr);
		len += 4;
		break;
	case AIW_MES_STMT_DEF_MENU: {
		len += aiw_expression_size(stmt->AIW_DEF_MENU.expr);
		len += 4;
		len += 1;
		struct aiw_mes_menu_case *c;
		vector_foreach_p(c, stmt->AIW_DEF_MENU.cases) {
			len += 8; // table entry
			if (c->cond)
				len += aiw_expression_size(c->cond);
			struct mes_statement *s;
			vector_foreach(s, c->body) {
				len += aiw_mes_statement_size(s);
			}
		}
		break;
	}
	case AIW_MES_STMT_MENU_EXEC:
		len += aiw_expression_list_size(stmt->AIW_MENU_EXEC.exprs);
		break;
	case AIW_MES_STMT_COMMIT_MESSAGE:
		if (ai5_target_game == GAME_KAWARAZAKIKE)
			len += aiw_parameter_list_size(stmt->CALL.params);
		break;
	case AIW_MES_STMT_35:
		len += 2;
		len += 2;
		break;
	case AIW_MES_STMT_37:
		len += 4;
		break;
	default:
		ERROR("invalid statement type");
	}
	return len;
}
