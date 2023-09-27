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

#ifndef ELF_TOOLS_MES_FLAT_PARSER_H_
#define ELF_TOOLS_MES_FLAT_PARSER_H_

#include "mes.h"

int flat_lex();
void flat_error(const char *s);

extern mes_statement_list mf_parsed_code;
extern unsigned long mf_line;
void mf_push_label(string label, struct mes_statement *stmt);
void mf_program(mes_statement_list statements);
uint8_t mf_parse_u8(string str);
mes_statement_list mf_parse_string_literal(string str);

static inline mes_statement_list mf_push_statement(mes_statement_list list,
		struct mes_statement *stmt)
{
	vector_push(struct mes_statement*, list, stmt);
	return list;
}

static inline mes_statement_list mf_append_statements(mes_statement_list dst,
		mes_statement_list src)
{
	struct mes_statement *stmt;
	vector_foreach(stmt, src) {
		vector_push(struct mes_statement*, dst, stmt);
	}
	vector_destroy(src);
	return dst;
}

static inline mes_expression_list mf_push_expression(mes_expression_list list,
		struct mes_expression *expr)
{
	vector_push(struct mes_expression*, list, expr);
	return list;
}

static inline mes_parameter_list mf_push_param(mes_parameter_list list,
		struct mes_parameter param)
{
	vector_push(struct mes_parameter, list, param);
	return list;
}

static inline mes_qname mf_push_qname_ident(mes_qname name, string ident)
{
	struct mes_qname_part part = { .type = MES_QNAME_IDENT, .ident = ident };
	vector_push(struct mes_qname_part, name, part);
	return name;
}

static inline mes_qname mf_push_qname_number(mes_qname name, int number)
{
	struct mes_qname_part part = { .type = MES_QNAME_NUMBER, .number = number };
	vector_push(struct mes_qname_part, name, part);
	return name;
}

struct mes_statement *mes_stmt_end(void);
struct mes_statement *mes_stmt_txt(string str);
struct mes_statement *mes_stmt_str(string str);
struct mes_statement *mes_stmt_setrbc(uint16_t no, mes_expression_list exprs);
struct mes_statement *mes_stmt_setrbe(struct mes_expression *expr, mes_expression_list exprs);
struct mes_statement *mes_stmt_setrbx(struct mes_expression *expr, mes_expression_list exprs);
struct mes_statement *mes_stmt_setv(uint8_t no, mes_expression_list exprs);
struct mes_statement *mes_stmt_setrd(uint8_t no, mes_expression_list exprs);
struct mes_statement *mes_stmt_setac(uint8_t no, struct mes_expression *off,
		mes_expression_list vals);
struct mes_statement *mes_stmt_seta_at(uint8_t no, struct mes_expression *off,
		mes_expression_list vals);
struct mes_statement *mes_stmt_setad(uint8_t no, struct mes_expression *off,
		mes_expression_list vals);
struct mes_statement *mes_stmt_setaw(uint8_t no, struct mes_expression *off,
		mes_expression_list vals);
struct mes_statement *mes_stmt_setab(uint8_t no, struct mes_expression *off,
		mes_expression_list vals);
struct mes_statement *mes_stmt_sys_var16_set(struct mes_expression *no,
		mes_expression_list vals);
struct mes_statement *mes_stmt_sys_var32_set(struct mes_expression *no,
		mes_expression_list vals);
struct mes_statement *mes_stmt_sys_named_var_set(string name, mes_expression_list vals);
struct mes_statement *mes_stmt_jz(struct mes_expression *cond, string target);
struct mes_statement *mes_stmt_jmp(string target);
struct mes_statement *mes_stmt_sys(struct mes_expression *expr, mes_parameter_list params);
struct mes_statement *mes_stmt_named_sys(mes_qname name, mes_parameter_list params);
struct mes_statement *mes_stmt_goto(mes_parameter_list params);
struct mes_statement *mes_stmt_call(mes_parameter_list params);
struct mes_statement *mes_stmt_menui(mes_parameter_list params, string skip);
struct mes_statement *mes_stmt_proc(mes_parameter_list params);
struct mes_statement *mes_stmt_util(mes_parameter_list params);
struct mes_statement *mes_stmt_line(uint8_t arg);
struct mes_statement *mes_stmt_procd(struct mes_expression *expr, string skip);
struct mes_statement *mes_stmt_menus(void);

struct mes_expression *mes_expr_constant(string text);
struct mes_expression *mes_expr_var4(struct mes_expression *index);
struct mes_expression *mes_expr_var16(string text);
struct mes_expression *mes_expr_var32(string text);
struct mes_expression *mes_expr_system_var16(struct mes_expression *index);
struct mes_expression *mes_expr_system_var32(struct mes_expression *index);
struct mes_expression *mes_expr_named_sysvar(string name);
struct mes_expression *mes_expr_array_index(enum mes_expression_op op, string var_no,
		struct mes_expression *index);
struct mes_expression *mes_expr_random(struct mes_expression *limit);
struct mes_expression *mes_binary_expr(enum mes_expression_op op, struct mes_expression *a,
		struct mes_expression *b);

struct mes_parameter mes_param_str(string text);
struct mes_parameter mes_param_expr(struct mes_expression *expr);

#endif // ELF_TOOLS_MES_FLAT_PARSER_H_
