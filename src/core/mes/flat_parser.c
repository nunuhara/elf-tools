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
#include <errno.h>
#include <ctype.h>

#include "nulib.h"
#include "nulib/file.h"
#include "nulib/hashtable.h"
#include "nulib/string.h"
#include "nulib/vector.h"
#include "nulib/utfsjis.h"
#include "mes.h"

#include "flat_parser.h"
#include "game.h"

extern int mf_lineno;

#define PARSE_ERROR(fmt, ...) \
	sys_error("ERROR: At line %d: " fmt "\n", mf_lineno, ##__VA_ARGS__)

mes_statement_list mf_parsed_code = vector_initializer;

extern FILE *mf_in;
int mf_parse(void);

static FILE *open_file(const char *file)
{
	if (!strcmp(file, "-"))
		return stdin;
	FILE *f = file_open_utf8(file, "rb");
	if (!f)
		ERROR("Opening input file '%s': %s", file, strerror(errno));
	return f;
}

mes_statement_list mes_flat_parse(const char *path)
{
	mf_parsed_code = (mes_statement_list) vector_initializer;

	mf_in = open_file(path);
	if (mf_parse())
		ERROR("Failed to parse file: %s", path);

	if (mf_in != stdin)
		fclose(mf_in);
	mf_in = NULL;

	mes_statement_list r = mf_parsed_code;
	mf_parsed_code = (mes_statement_list) vector_initializer;
	return r;
}

void mf_error(const char *s)
{
	PARSE_ERROR("%s", s);
}

// hash table associating labels with addresses
declare_hashtable_string_type(label_table, struct mes_statement*);
define_hashtable_string(label_table, struct mes_statement*);
hashtable_t(label_table) labels;

// list of statements with unresolved label references
struct label_ref { struct mes_statement *stmt; string name; };
vector_t(struct label_ref) label_refs;

void mf_push_label(string label, struct mes_statement *stmt)
{
	int ret;
	hashtable_iter_t k = hashtable_put(label_table, &labels, label, &ret);
	if (unlikely(ret == HASHTABLE_KEY_PRESENT))
		PARSE_ERROR("Multiple definitions of label: \"%s\"\n", label);
	hashtable_val(&labels, k) = stmt;
}

void mf_push_label_ref(struct mes_statement *stmt, string name)
{
	struct label_ref ref = { stmt, name };
	vector_push(struct label_ref, label_refs, ref);
}

static void mf_resolve_labels(mes_statement_list statements)
{
	struct label_ref ref;
	vector_foreach(ref, label_refs) {
		hashtable_iter_t k = hashtable_get(label_table, &labels, ref.name);
		if (unlikely(k == hashtable_end(&labels)))
			PARSE_ERROR("Undefined label: %s", ref.name);
		struct mes_statement *stmt = hashtable_val(&labels, k);
		switch (ref.stmt->op) {
		case MES_STMT_JZ:
			ref.stmt->JZ.addr = stmt->address;
			break;
		case MES_STMT_JMP:
			ref.stmt->JMP.addr = stmt->address;
			break;
		case MES_STMT_MENUI:
			ref.stmt->MENUI.addr = stmt->address;
			break;
		case MES_STMT_PROCD:
			ref.stmt->PROCD.skip_addr = stmt->address;
			break;
		default:
			ERROR("invalid opcode for label reference: %d", ref.stmt->op);
		}
	}
}

static void mf_assign_addresses(mes_statement_list statements)
{
	uint32_t ip = 0;
	struct mes_statement *stmt;
	vector_foreach(stmt, statements) {
		stmt->address = ip;
		ip += mes_statement_size(stmt);
	}
}

void mf_program(mes_statement_list statements)
{
	mf_assign_addresses(statements);
	mf_resolve_labels(statements);
	mf_parsed_code = statements;
}

static long parse_int(string str)
{
	char *endptr;
	long i = strtol(str, &endptr, 0);
	if (*endptr != '\0')
		PARSE_ERROR("invalid integer constant: %s", str);
	return i;
}

uint8_t mf_parse_u8(string str)
{
	long i = parse_int(str);
	if (i < 0 || i >= 256)
		PARSE_ERROR("value out of range: %s", str);
	string_free(str);
	return i;
}

static struct mes_statement *read_string_literal(const char *in, const char **out)
{
	const char *p = in;
	if (mes_char_is_zenkaku(*p) || (p[0] == '\\' && p[1] == 'X')) {
		int i = 0;
		while (*p) {
			// XXX: \X escapes should be included in zenkaku part
			if (*p == '\\') {
				if (p[1] != 'X')
					break;
				// XXX: already checked by lexer
				assert(isxdigit(p[2]) && isxdigit(p[3]) && isxdigit(p[4])
						&& isxdigit(p[5]));
				i++;
				p += 6;
				continue;
			}
			if (!mes_char_is_zenkaku(*p))
				break;
			i++;
			p += 2;
		}
		struct mes_statement *stmt = mes_stmt(MES_STMT_TXT);
		stmt->TXT.text = sjis_cstring_to_utf8(in, i*2);
		stmt->TXT.terminated = true;
		*out = p;
		return stmt;
	} else if (mes_char_is_hankaku(*p)) {
		int i = 1;
		p++;
		while (*p && mes_char_is_hankaku(*p)) {
			i++;
			p++;
		}
		struct mes_statement *stmt = mes_stmt(MES_STMT_STR);
		stmt->TXT.text = string_new_len(in, i);
		stmt->TXT.terminated = true;
		*out = p;
		return stmt;
	}
	PARSE_ERROR("Invalid character in string literal: %02x", (unsigned)*in);
}

mes_statement_list mf_parse_string_literal(string str)
{
	mes_statement_list stmts = vector_initializer;

	string sjis = utf8_to_sjis(str);
	string_free(str);

	const char *p = sjis;
	while (*p) {
		vector_push(struct mes_statement*, stmts, read_string_literal(p, &p));
	}
	string_free(sjis);

	return stmts;
}

struct mes_statement *mf_stmt_sys_named_var_set(string name, mes_expression_list vals)
{
	bool dword;
	int no = mes_resolve_sysvar(name, &dword);
	if (no < 0)
		PARSE_ERROR("Invalid system variable: %s", name);
	string_free(name);

	struct mes_expression *e = mes_expr(MES_EXPR_IMM);
	e->arg8 = no;
	return dword ? mes_stmt_sys_var32_set(e, vals) : mes_stmt_sys_var16_set(e, vals);
}

static mes_parameter_list append_params(mes_parameter_list a, mes_parameter_list b)
{
	struct mes_parameter *p;
	vector_foreach_p(p, b) {
		vector_push(struct mes_parameter, a, *p);
	}
	return a;
}

struct mes_statement *mf_stmt_named_sys(mes_qname name, mes_parameter_list _params)
{
	int no;
	mes_parameter_list call = mes_resolve_syscall(name, &no);
	if (no < 0)
		PARSE_ERROR("Invalid System call");

	mes_parameter_list params = append_params(call, _params);
	struct mes_statement *stmt = mes_stmt(MES_STMT_SYS);
	stmt->SYS.expr = mes_expr_constant(no);
	stmt->SYS.params = params;
	vector_destroy(_params);
	return stmt;
}

struct mes_statement *mf_stmt_call(mes_parameter_list params)
{
	if (vector_length(params) < 1)
		PARSE_ERROR("Call with zero parameters");
	if (vector_A(params, 0).type == MES_PARAM_STRING)
		return mes_stmt_call(params);
	return mes_stmt_proc(params);
}

struct mes_expression *mf_parse_constant(string text)
{
	long i = parse_int(text);
	if (i < 0)
		PARSE_ERROR("value out of range: %ld", i);
	struct mes_expression *expr = mes_expr_constant(parse_int(text));
	string_free(text);
	return expr;
}

struct mes_expression *mf_expr_named_sysvar(string name)
{
	bool dword;
	int no = mes_resolve_sysvar(name, &dword);
	if (no < 0)
		PARSE_ERROR("Invalid system variable: %s", name);
	string_free(name);

	struct mes_expression *index = mes_expr(MES_EXPR_IMM);
	index->arg8 = no;
	return dword ? mes_expr_system_var32(index) : mes_expr_system_var16(index);
}

