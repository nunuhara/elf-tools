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

int mf_lex();
void mf_error(const char *s);

extern mes_statement_list mf_parsed_code;
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

void mf_push_label_ref(struct mes_statement *stmt, string name);

struct mes_statement *mf_stmt_sys_named_var_set(string name, mes_expression_list vals);
struct mes_statement *mf_stmt_named_sys(mes_qname name, mes_parameter_list params);
struct mes_statement *mf_stmt_util(mes_qname name, mes_parameter_list params);
struct mes_statement *mf_stmt_call(mes_parameter_list params);

struct mes_expression *mf_parse_constant(string text);
struct mes_expression *mf_expr_named_sysvar(string name);

#endif // ELF_TOOLS_MES_FLAT_PARSER_H_
