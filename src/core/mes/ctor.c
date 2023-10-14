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

#include "nulib.h"

#include "mes.h"

// {{{ statements

struct mes_statement *mes_stmt(enum mes_statement_op op)
{
	struct mes_statement *stmt = xcalloc(1, sizeof(struct mes_statement));
	stmt->op = op;
	return stmt;
}

struct mes_statement *mes_stmt_end(void)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_END);
	return stmt;
}

struct mes_statement *mes_stmt_txt(string str)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_TXT);
	stmt->TXT.text = str;
	stmt->TXT.terminated = true;
	return stmt;
}

struct mes_statement *mes_stmt_str(string str)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_STR);
	stmt->TXT.text = str;
	stmt->TXT.terminated = true;
	return stmt;
}

struct mes_statement *mes_stmt_setrbc(uint16_t no, mes_expression_list exprs)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_SETRBC);
	stmt->SETRBC.reg_no = no;
	stmt->SETRBC.exprs = exprs;
	return stmt;
}

struct mes_statement *mes_stmt_setrbe(struct mes_expression *expr, mes_expression_list exprs)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_SETRBE);
	stmt->SETRBE.reg_expr = expr;
	stmt->SETRBE.val_exprs = exprs;
	return stmt;
}

struct mes_statement *mes_stmt_setrbx(struct mes_expression *expr, mes_expression_list exprs)
{
	if (expr->op == MES_EXPR_IMM) {
		struct mes_statement *r = mes_stmt_setrbc(expr->arg8, exprs);
		free(expr);
		return r;
	}
	if (expr->op == MES_EXPR_IMM16) {
		struct mes_statement *r = mes_stmt_setrbc(expr->arg16, exprs);
		free(expr);
		return r;
	}
	return mes_stmt_setrbe(expr, exprs);
}

struct mes_statement *mes_stmt_setv(uint8_t no, mes_expression_list exprs)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_SETV);
	stmt->SETV.var_no = no;
	stmt->SETV.exprs = exprs;
	return stmt;
}

struct mes_statement *mes_stmt_setrd(uint8_t no, mes_expression_list exprs)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_SETRD);
	stmt->SETRD.var_no = no;
	stmt->SETRD.val_exprs = exprs;
	return stmt;
}

struct mes_statement *mes_stmt_setac(uint8_t no, struct mes_expression *off,
		mes_expression_list vals)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_SETAC);
	stmt->SETAC.var_no = no;
	stmt->SETAC.off_expr = off;
	stmt->SETAC.val_exprs = vals;
	return stmt;
}

struct mes_statement *mes_stmt_seta_at(uint8_t no, struct mes_expression *off,
		mes_expression_list vals)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_SETA_AT);
	stmt->SETA_AT.var_no = no;
	stmt->SETA_AT.off_expr = off;
	stmt->SETA_AT.val_exprs = vals;
	return stmt;
}

struct mes_statement *mes_stmt_setad(uint8_t no, struct mes_expression *off,
		mes_expression_list vals)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_SETAD);
	stmt->SETAD.var_no = no;
	stmt->SETAD.off_expr = off;
	stmt->SETAD.val_exprs = vals;
	return stmt;
}

struct mes_statement *mes_stmt_setaw(uint8_t no, struct mes_expression *off,
		mes_expression_list vals)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_SETAW);
	stmt->SETAW.var_no = no;
	stmt->SETAW.off_expr = off;
	stmt->SETAW.val_exprs = vals;
	return stmt;
}

struct mes_statement *mes_stmt_setab(uint8_t no, struct mes_expression *off,
		mes_expression_list vals)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_SETAB);
	stmt->SETAB.var_no = no;
	stmt->SETAB.off_expr = off;
	stmt->SETAB.val_exprs = vals;
	return stmt;
}

struct mes_statement *mes_stmt_sys_var16_set(struct mes_expression *no,
		mes_expression_list vals)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_SETA_AT);
	stmt->SETA_AT.var_no = 0;
	stmt->SETA_AT.off_expr = no;
	stmt->SETA_AT.val_exprs = vals;
	return stmt;
}

struct mes_statement *mes_stmt_sys_var32_set(struct mes_expression *no,
		mes_expression_list vals)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_SETAD);
	stmt->SETAD.var_no = 0;
	stmt->SETAD.off_expr = no;
	stmt->SETAD.val_exprs = vals;
	return stmt;
}

struct mes_statement *mes_stmt_jz(struct mes_expression *cond)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_JZ);
	stmt->JZ.expr = cond;
	return stmt;
}

struct mes_statement *mes_stmt_jmp(void)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_JMP);
	return stmt;
}

struct mes_statement *mes_stmt_sys(struct mes_expression *expr, mes_parameter_list params)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_SYS);
	stmt->SYS.expr = expr;
	stmt->SYS.params = params;
	return stmt;
}

struct mes_statement *mes_stmt_goto(mes_parameter_list params)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_GOTO);
	stmt->GOTO.params = params;
	return stmt;
}

struct mes_statement *mes_stmt_call(mes_parameter_list params)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_CALL);
	stmt->CALL.params = params;
	return stmt;
}

struct mes_statement *mes_stmt_proc(mes_parameter_list params)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_PROC);
	stmt->PROC.params = params;
	return stmt;
}

struct mes_statement *mes_stmt_menui(mes_parameter_list params)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_MENUI);
	stmt->MENUI.params = params;
	return stmt;
}

struct mes_statement *mes_stmt_util(mes_parameter_list params)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_UTIL);
	stmt->UTIL.params = params;
	return stmt;
}

struct mes_statement *mes_stmt_line(uint8_t arg)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_LINE);
	stmt->LINE.arg = arg;
	return stmt;
}

struct mes_statement *mes_stmt_procd(struct mes_expression *expr)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_PROCD);
	stmt->PROCD.no_expr = expr;
	return stmt;
}

struct mes_statement *mes_stmt_menus(void)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_MENUS);
	return stmt;
}

// }}} statements
// {{{ expressions

struct mes_expression *mes_expr(enum mes_expression_op op)
{
	struct mes_expression *expr = xcalloc(1, sizeof(struct mes_expression));
	expr->op = op;
	return expr;
}

struct mes_expression *mes_expr_constant(long i)
{
	struct mes_expression *expr = mes_expr(0);
	assert(i >= 0);
	if (i > 65535) {
		expr->op = MES_EXPR_IMM32;
		expr->arg32 = i;
	} else if (i > 255) {
		expr->op = MES_EXPR_IMM16;
		expr->arg16 = i;
	} else if (i < 0x80) {
		expr->op = MES_EXPR_IMM;
		expr->arg8 = i;
	} else {
		expr->op = MES_EXPR_IMM16;
		expr->arg16 = i;
	}
	return expr;
}

struct mes_expression *mes_expr_var4(struct mes_expression *index)
{
	if (index->op == MES_EXPR_IMM) {
		struct mes_expression *expr = mes_expr(MES_EXPR_REG16);
		expr->arg16 = index->arg8;
		free(index);
		return expr;
	}
	if (index->op == MES_EXPR_IMM16) {
		struct mes_expression *expr = mes_expr(MES_EXPR_REG16);
		expr->arg16 = index->arg16;
		free(index);
		return expr;
	}
	struct mes_expression *expr = mes_expr(MES_EXPR_REG8);
	expr->sub_a = index;
	return expr;
}

struct mes_expression *mes_expr_var16(uint8_t no)
{
	struct mes_expression *expr = mes_expr(MES_EXPR_VAR);
	expr->arg8 = no;
	return expr;
}

struct mes_expression *mes_expr_var32(uint8_t no)
{
	struct mes_expression *expr = mes_expr(MES_EXPR_VAR32);
	expr->arg8 = no;
	return expr;
}

struct mes_expression *mes_expr_system_var16(struct mes_expression *index)
{
	struct mes_expression *expr = mes_expr(MES_EXPR_ARRAY16_GET16);
	expr->arg8 = 0;
	expr->sub_a = index;
	return expr;
}

struct mes_expression *mes_expr_system_var32(struct mes_expression *index)
{
	struct mes_expression *expr = mes_expr(MES_EXPR_ARRAY32_GET32);
	expr->arg8 = 0;
	expr->sub_a = index;
	return expr;
}

struct mes_expression *mes_expr_array_index(enum mes_expression_op op, uint8_t var_no,
		struct mes_expression *index)
{
	struct mes_expression *expr = mes_expr(op);
	expr->arg8 = var_no;
	switch (op) {
	case MES_EXPR_ARRAY16_GET16:
	case MES_EXPR_ARRAY32_GET8:
	case MES_EXPR_ARRAY32_GET16:
	case MES_EXPR_ARRAY32_GET32:
		expr->arg8++;
		break;
	default:
		break;
	}
	expr->sub_a = index;
	return expr;
}

struct mes_expression *mes_expr_random(struct mes_expression *limit)
{
	struct mes_expression *expr = mes_expr(MES_EXPR_RAND);
	expr->sub_a = limit;
	return expr;
}

struct mes_expression *mes_binary_expr(enum mes_expression_op op, struct mes_expression *b,
		struct mes_expression *a)
{
	struct mes_expression *expr = mes_expr(op);
	expr->sub_a = a;
	expr->sub_b = b;
	return expr;
}

// }}} expressions
// {{{ parameters

struct mes_parameter mes_param_str(string text)
{
	struct mes_parameter p;
	p.type = MES_PARAM_STRING;
	p.str = text;
	return p;
}

struct mes_parameter mes_param_expr(struct mes_expression *expr)
{
	return (struct mes_parameter) {
		.type = MES_PARAM_EXPRESSION,
		.expr = expr
	};
}

// }}} parameters
