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

extern int flat_lineno;

#define PARSE_ERROR(fmt, ...) \
	sys_error("ERROR: At line %d: " fmt "\n", flat_lineno, ##__VA_ARGS__)

mes_statement_list mf_parsed_code = vector_initializer;

uint32_t flat_instr_ptr = 0;

extern FILE *flat_in;
int flat_parse(void);

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
	flat_instr_ptr = 0;

	flat_in = open_file(path);
	if (flat_parse())
		ERROR("Failed to parse file: %s", path);

	if (flat_in != stdin)
		fclose(flat_in);
	flat_in = NULL;

	mes_statement_list r = mf_parsed_code;
	mf_parsed_code = (mes_statement_list) vector_initializer;
	return r;
}

void flat_error(const char *s)
{
	PARSE_ERROR("%s", s);
	//sys_error("%s: at line %d", s, flat_lineno);
	//sys_error("ERROR: At line %lu: %s\n", mf_line, s);
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

static void push_label_ref(struct mes_statement *stmt, string name)
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

static uint32_t _expression_length(struct mes_expression *expr)
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
		len += 1 + _expression_length(expr->sub_a);
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
		len += _expression_length(expr->sub_a) + _expression_length(expr->sub_b);
		break;
	case MES_EXPR_RAND:
		if (target_game == GAME_DOUKYUUSEI) {
			len += 2;
		} else {
			len += _expression_length(expr->sub_a);
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
		len += _expression_length(expr->sub_a);
		break;
	case MES_EXPR_END:
		ERROR("MES_EXPR_END in mes_expression");
		break;
	default:
		break;
	}
	return len;
}

static uint32_t expression_length(struct mes_expression *expr)
{
	return _expression_length(expr) + 1;
}

static uint32_t expression_list_length(mes_expression_list expressions)
{
	uint32_t len = 0;
	struct mes_expression *expr;
	vector_foreach(expr, expressions) {
		len += expression_length(expr) + 1;
	}
	return len;
}

static uint32_t string_param_length(string str)
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

static uint32_t parameter_list_length(mes_parameter_list params)
{
	uint32_t len = 0;
	struct mes_parameter *p;
	vector_foreach_p(p, params) {
		len++;
		if (p->type == MES_PARAM_STRING) {
			len += string_param_length(p->str) + 1;
		} else {
			len += expression_length(p->expr);
		}
	}
	return len + 1;
}

static uint32_t txt_length(string text)
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

static uint32_t str_length(string str)
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

static uint32_t statement_length(struct mes_statement *stmt)
{
	uint32_t len = 1;
	switch (stmt->op) {
	case MES_STMT_END:
		break;
	case MES_STMT_TXT:
	case MES_STMT_STR:
		// FIXME: doesn't handle escapes properly
		if (stmt->op == MES_STMT_TXT) {
			len += txt_length(stmt->TXT.text);
		} else {
			len += str_length(stmt->TXT.text);
		}
		if (stmt->TXT.terminated)
			len++;
		if (stmt->TXT.unprefixed)
			len--;
		break;
	case MES_STMT_SETRBC:
		len += 2 + expression_list_length(stmt->SETRBC.exprs);
		break;
	case MES_STMT_SETV:
		len += 1 + expression_list_length(stmt->SETV.exprs);
		break;
	case MES_STMT_SETRBE:
		len += expression_length(stmt->SETRBE.reg_expr);
		len += expression_list_length(stmt->SETRBE.val_exprs);
		break;
	case MES_STMT_SETAC:
		len += expression_length(stmt->SETAC.off_expr) + 1;
		len += expression_list_length(stmt->SETAC.val_exprs);
		break;
	case MES_STMT_SETA_AT:
		len += expression_length(stmt->SETA_AT.off_expr) + 1;
		len += expression_list_length(stmt->SETA_AT.val_exprs);
		break;
	case MES_STMT_SETAD:
		len += expression_length(stmt->SETAD.off_expr) + 1;
		len += expression_list_length(stmt->SETAD.val_exprs);
		break;
	case MES_STMT_SETAW:
		len += expression_length(stmt->SETAB.off_expr) + 1;
		len += expression_list_length(stmt->SETAB.val_exprs);
		break;
	case MES_STMT_SETAB:
		len += expression_length(stmt->SETAB.off_expr) + 1;
		len += expression_list_length(stmt->SETAB.val_exprs);
		break;
	case MES_STMT_JZ:
		len += expression_length(stmt->JZ.expr) + 4;
		break;
	case MES_STMT_JMP:
		len += 4;
		break;
	case MES_STMT_SYS:
		len += expression_length(stmt->SYS.expr);
		len += parameter_list_length(stmt->SYS.params);
		break;
	case MES_STMT_GOTO:
		len += parameter_list_length(stmt->GOTO.params);
		break;
	case MES_STMT_CALL:
		len += parameter_list_length(stmt->CALL.params);
		break;
	case MES_STMT_MENUI:
		len += parameter_list_length(stmt->MENUI.params) + 4;
		break;
	case MES_STMT_PROC:
		len += parameter_list_length(stmt->PROC.params);
		break;
	case MES_STMT_UTIL:
		len += parameter_list_length(stmt->UTIL.params);
		break;
	case MES_STMT_LINE:
		len++;
		break;
	case MES_STMT_PROCD:
		len += expression_length(stmt->PROCD.no_expr) + 4;
		break;
	case MES_STMT_MENUS:
		break;
	case MES_STMT_SETRD:
		len += 1 + expression_list_length(stmt->SETRD.val_exprs);
		break;
	default:
		ERROR("invalid statement type");
	}
	return len;
}

static void mf_assign_addresses(mes_statement_list statements)
{
	uint32_t ip = 0;
	struct mes_statement *stmt;
	vector_foreach(stmt, statements) {
		stmt->address = ip;
		ip += statement_length(stmt);
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

static struct mes_statement *mes_stmt(enum mes_statement_op op)
{
	struct mes_statement *stmt = xcalloc(1, sizeof(struct mes_statement));
	stmt->op = op;
	stmt->address = flat_instr_ptr;
	return stmt;
}

static struct mes_expression *alloc_expr(enum mes_expression_op op)
{
	struct mes_expression *expr = xcalloc(1, sizeof(struct mes_expression));
	expr->op = op;
	return expr;
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

struct mes_statement *mes_stmt_end(void)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_END);
	stmt->next_address = ++flat_instr_ptr;
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

struct mes_statement *mes_stmt_sys_named_var_set(string name, mes_expression_list vals)
{
	bool dword;
	int no = mes_resolve_sysvar(name, &dword);
	if (no < 0)
		PARSE_ERROR("Invalid system variable: %s", name);
	string_free(name);

	struct mes_expression *e = alloc_expr(MES_EXPR_IMM);
	e->arg8 = no;
	return dword ? mes_stmt_sys_var32_set(e, vals) : mes_stmt_sys_var16_set(e, vals);
}

struct mes_statement *mes_stmt_jz(struct mes_expression *cond, string target)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_JZ);
	stmt->JZ.expr = cond;
	push_label_ref(stmt, target);
	return stmt;
}

struct mes_statement *mes_stmt_jmp(string target)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_JMP);
	push_label_ref(stmt, target);
	return stmt;
}

struct mes_statement *mes_stmt_sys(struct mes_expression *expr, mes_parameter_list params)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_SYS);
	stmt->SYS.expr = expr;
	stmt->SYS.params = params;
	return stmt;
}

static mes_parameter_list append_params(mes_parameter_list a, mes_parameter_list b)
{
	struct mes_parameter *p;
	vector_foreach_p(p, b) {
		vector_push(struct mes_parameter, a, *p);
	}
	return a;
}

static struct mes_expression *_mes_expr_constant(long i)
{
	struct mes_expression *expr = alloc_expr(0);
	if (i < 0)
		PARSE_ERROR("value out of range: %ld", i);
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

struct mes_statement *mes_stmt_named_sys(mes_qname name, mes_parameter_list _params)
{
	int no;
	mes_parameter_list call = mes_resolve_syscall(name, &no);
	if (no < 0)
		PARSE_ERROR("Invalid System call");

	mes_parameter_list params = append_params(call, _params);
	struct mes_statement *stmt = mes_stmt(MES_STMT_SYS);
	stmt->SYS.expr = _mes_expr_constant(no);
	stmt->SYS.params = params;
	vector_destroy(_params);
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
	if (vector_length(params) < 1)
		PARSE_ERROR("Call with zero parameters");
	if (vector_A(params, 0).type == MES_PARAM_STRING) {
		struct mes_statement *stmt = mes_stmt(MES_STMT_CALL);
		stmt->CALL.params = params;
		return stmt;
	}
	struct mes_statement *stmt = mes_stmt(MES_STMT_PROC);
	stmt->PROC.params = params;
	return stmt;
}

struct mes_statement *mes_stmt_menui(mes_parameter_list params, string skip)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_MENUI);
	stmt->MENUI.params = params;
	push_label_ref(stmt, skip);
	return stmt;
}

struct mes_statement *mes_stmt_proc(mes_parameter_list params)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_PROC);
	stmt->PROC.params = params;
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

struct mes_statement *mes_stmt_procd(struct mes_expression *expr, string skip)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_PROCD);
	stmt->PROCD.no_expr = expr;
	push_label_ref(stmt, skip);
	return stmt;
}

struct mes_statement *mes_stmt_menus(void)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_MENUS);
	return stmt;
}

struct mes_expression *mes_expr_constant(string text)
{
	struct mes_expression *expr = _mes_expr_constant(parse_int(text));
	string_free(text);
	return expr;
}

struct mes_expression *mes_expr_var4(struct mes_expression *index)
{
	if (index->op == MES_EXPR_IMM) {
		struct mes_expression *expr = alloc_expr(MES_EXPR_REG16);
		expr->arg16 = index->arg8;
		free(index);
		return expr;
	}
	if (index->op == MES_EXPR_IMM16) {
		struct mes_expression *expr = alloc_expr(MES_EXPR_REG16);
		expr->arg16 = index->arg16;
		free(index);
		return expr;
	}
	struct mes_expression *expr = alloc_expr(MES_EXPR_REG8);
	expr->sub_a = index;
	return expr;
}

struct mes_expression *mes_expr_var16(string text)
{
	struct mes_expression *expr = alloc_expr(MES_EXPR_VAR);
	expr->arg8 = mf_parse_u8(text);
	return expr;
}

struct mes_expression *mes_expr_var32(string text)
{
	struct mes_expression *expr = alloc_expr(MES_EXPR_VAR32);
	expr->arg8 = mf_parse_u8(text);
	return expr;
}

struct mes_expression *mes_expr_system_var16(struct mes_expression *index)
{
	struct mes_expression *expr = alloc_expr(MES_EXPR_ARRAY16_GET16);
	expr->arg8 = 0;
	expr->sub_a = index;
	return expr;
}

struct mes_expression *mes_expr_system_var32(struct mes_expression *index)
{
	struct mes_expression *expr = alloc_expr(MES_EXPR_ARRAY32_GET32);
	expr->arg8 = 0;
	expr->sub_a = index;
	return expr;
}

struct mes_expression *mes_expr_named_sysvar(string name)
{
	bool dword;
	int no = mes_resolve_sysvar(name, &dword);
	if (no < 0)
		PARSE_ERROR("Invalid system variable: %s", name);
	string_free(name);

	struct mes_expression *index = alloc_expr(MES_EXPR_IMM);
	index->arg8 = no;
	return dword ? mes_expr_system_var32(index) : mes_expr_system_var16(index);
}

struct mes_expression *mes_expr_array_index(enum mes_expression_op op, string var_no,
		struct mes_expression *index)
{
	struct mes_expression *expr = alloc_expr(op);
	expr->arg8 = mf_parse_u8(var_no);
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
	struct mes_expression *expr = alloc_expr(MES_EXPR_RAND);
	expr->sub_a = limit;
	return expr;
}

struct mes_expression *mes_binary_expr(enum mes_expression_op op, struct mes_expression *a,
		struct mes_expression *b)
{
	struct mes_expression *expr = alloc_expr(op);
	expr->sub_a = a;
	expr->sub_b = b;
	return expr;
}

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
