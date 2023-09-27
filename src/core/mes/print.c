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

#include <stdio.h>

#include "nulib.h"
#include "nulib/port.h"
#include "nulib/string.h"
#include "mes.h"

// expressions {{{

static const char *binary_op_to_string(enum mes_expression_op op)
{
	switch (op) {
	case MES_EXPR_PLUS: return "+";
	case MES_EXPR_MINUS: return "-";
	case MES_EXPR_MUL: return "*";
	case MES_EXPR_DIV: return "/";
	case MES_EXPR_MOD: return "%";
	case MES_EXPR_AND: return "&&";
	case MES_EXPR_OR: return "||";
	case MES_EXPR_BITAND: return "&";
	case MES_EXPR_BITIOR: return "|";
	case MES_EXPR_BITXOR: return "^";
	case MES_EXPR_LT: return "<";
	case MES_EXPR_GT: return ">";
	case MES_EXPR_LTE: return "<=";
	case MES_EXPR_GTE: return ">=";
	case MES_EXPR_EQ: return "==";
	case MES_EXPR_NEQ: return "!=";
	default: ERROR("invalid binary operator: %d", op);
	}
}

static bool is_binary_op(enum mes_expression_op op)
{
	switch (op) {
	case MES_EXPR_PLUS:
	case MES_EXPR_MINUS:
	case MES_EXPR_MUL:
	case MES_EXPR_DIV:
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
		return true;
	default:
		return false;
	}
}

static bool binary_parens_required(enum mes_expression_op op, struct mes_expression *sub)
{
	if (!is_binary_op(sub->op))
		return false;

	switch (op) {
	case MES_EXPR_IMM:
	case MES_EXPR_VAR:
	case MES_EXPR_ARRAY16_GET16:
	case MES_EXPR_ARRAY16_GET8:
	case MES_EXPR_RAND:
	case MES_EXPR_IMM16:
	case MES_EXPR_IMM32:
	case MES_EXPR_REG16:
	case MES_EXPR_REG8:
	case MES_EXPR_ARRAY32_GET32:
	case MES_EXPR_ARRAY32_GET16:
	case MES_EXPR_ARRAY32_GET8:
	case MES_EXPR_VAR32:
case MES_EXPR_END:
		ERROR("invalid binary operator: %d", op);
	case MES_EXPR_MUL:
	case MES_EXPR_DIV:
	case MES_EXPR_MOD:
		return true;
	case MES_EXPR_PLUS:
	case MES_EXPR_MINUS:
		switch (sub->op) {
		case MES_EXPR_MUL:
		case MES_EXPR_DIV:
		case MES_EXPR_MOD:
			return false;
		default:
			return true;
		}
	case MES_EXPR_LT:
	case MES_EXPR_GT:
	case MES_EXPR_GTE:
	case MES_EXPR_LTE:
	case MES_EXPR_EQ:
	case MES_EXPR_NEQ:
		switch (sub->op) {
		case MES_EXPR_PLUS:
		case MES_EXPR_MINUS:
		case MES_EXPR_MUL:
		case MES_EXPR_DIV:
		case MES_EXPR_MOD:
			return false;
		default:
			return true;
		}
	case MES_EXPR_BITAND:
	case MES_EXPR_BITIOR:
	case MES_EXPR_BITXOR:
		return true;
	case MES_EXPR_AND:
	case MES_EXPR_OR:
		switch (sub->op) {
		case MES_EXPR_AND:
		case MES_EXPR_OR:
			return true;
		default:
			return false;
		}
	}
	return true;
}

static void mes_binary_expression_print(enum mes_expression_op op, struct mes_expression *lhs,
		struct mes_expression *rhs, struct port *out)
{
	if (binary_parens_required(op, rhs)) {
		port_putc(out, '(');
		mes_expression_print(rhs, out);
		port_putc(out, ')');
	} else {
		mes_expression_print(rhs, out);
	}
	port_printf(out, " %s ", binary_op_to_string(op));
	if (binary_parens_required(op, lhs)) {
		port_putc(out, '(');
		mes_expression_print(lhs, out);
		port_putc(out, ')');
	} else {
		mes_expression_print(lhs, out);
	}
}

static const char *system_var16_name(uint8_t no)
{
	if (no < MES_NR_SYSTEM_VARIABLES)
		return mes_system_var16_names[no];
	return NULL;
}

static void op_array16_get16_print(struct mes_expression *expr, struct port *out)
{
	// if arg is 0, we're reading a system variable
	const char *name;
	if (expr->arg8 == 0 && expr->sub_a->op == MES_EXPR_IMM
			&& (name = system_var16_name(expr->sub_a->arg8))) {
		port_printf(out, "System.%s", name);
		return;
	}

	// system variable with non-immediate index or unknown name
	if (expr->arg8 == 0) {
		port_puts(out, "System.var16[");
		mes_expression_print(expr->sub_a, out);
		port_putc(out, ']');
		return;
	}

	// read short from memory
	// variable is offset from start of memory
	// expression is index into short array at that offset
	port_printf(out, "var16[%d]->word[", (int)expr->arg8 - 1);
	mes_expression_print(expr->sub_a, out);
	port_putc(out, ']');
}

static const char *system_var32_name(uint8_t no)
{
	if (no < MES_NR_SYSTEM_VARIABLES)
		return mes_system_var32_names[no];
	return NULL;
}

static void op_array32_get32_print(struct mes_expression *expr, struct port *out)
{
	// if arg is 0, we're reading a system pointer
	const char *name;
	if (expr->arg8 == 0 && expr->sub_a->op == MES_EXPR_IMM
			&& (name = system_var32_name(expr->sub_a->arg8))) {
		port_printf(out, "System.%s", name);
		return;
	}

	// system pointer with non-immediate index or unknown name
	if (expr->arg8 == 0) {
		port_puts(out, "System.var32[");
		mes_expression_print(expr->sub_a, out);
		port_putc(out, ']');
		return;
	}

	port_printf(out, "var32[%d]->dword[", (int)expr->arg8 - 1);
	mes_expression_print(expr->sub_a, out);
	port_putc(out, ']');
}

static void print_number(uint32_t n, struct port *out)
{
	if (n < 255) {
		port_printf(out, "%u", n);
	} else if ((n & (n - 1)) == 0 || ((n+1) & n) == 0) {
		port_printf(out, "0x%x", n);
	} else {
		port_printf(out, "%u", n);
	}
}

void mes_expression_print(struct mes_expression *expr, struct port *out)
{
	switch (expr->op) {
	case MES_EXPR_IMM:
		print_number(expr->arg8, out);
		break;
	case MES_EXPR_VAR:
		port_printf(out, "var16[%u]", (unsigned)expr->arg8);
		break;
	case MES_EXPR_ARRAY16_GET16:
		op_array16_get16_print(expr, out);
		break;
	case MES_EXPR_ARRAY16_GET8:
		port_printf(out, "var16[%d]->byte[", (int)expr->arg8);
		mes_expression_print(expr->sub_a, out);
		port_putc(out, ']');
		break;
	case MES_EXPR_PLUS:
		mes_binary_expression_print(MES_EXPR_PLUS, expr->sub_a, expr->sub_b, out);
		break;
	case MES_EXPR_MINUS:
		mes_binary_expression_print(MES_EXPR_MINUS, expr->sub_a, expr->sub_b, out);
		break;
	case MES_EXPR_MUL:
		mes_binary_expression_print(MES_EXPR_MUL, expr->sub_a, expr->sub_b, out);
		break;
	case MES_EXPR_DIV:
		mes_binary_expression_print(MES_EXPR_DIV, expr->sub_a, expr->sub_b, out);
		break;
	case MES_EXPR_MOD:
		mes_binary_expression_print(MES_EXPR_MOD, expr->sub_a, expr->sub_b, out);
		break;
	case MES_EXPR_RAND:
		port_puts(out, "rand(");
		mes_expression_print(expr->sub_a, out);
		port_putc(out, ')');
		break;
	case MES_EXPR_AND:
		mes_binary_expression_print(MES_EXPR_AND, expr->sub_a, expr->sub_b, out);
		break;
	case MES_EXPR_OR:
		mes_binary_expression_print(MES_EXPR_OR, expr->sub_a, expr->sub_b, out);
		break;
	case MES_EXPR_BITAND:
		mes_binary_expression_print(MES_EXPR_BITAND, expr->sub_a, expr->sub_b, out);
		break;
	case MES_EXPR_BITIOR:
		mes_binary_expression_print(MES_EXPR_BITIOR, expr->sub_a, expr->sub_b, out);
		break;
	case MES_EXPR_BITXOR:
		mes_binary_expression_print(MES_EXPR_BITXOR, expr->sub_a, expr->sub_b, out);
		break;
	case MES_EXPR_LT:
		mes_binary_expression_print(MES_EXPR_LT, expr->sub_a, expr->sub_b, out);
		break;
	case MES_EXPR_GT:
		mes_binary_expression_print(MES_EXPR_GT, expr->sub_a, expr->sub_b, out);
		break;
	case MES_EXPR_LTE:
		mes_binary_expression_print(MES_EXPR_LTE, expr->sub_a, expr->sub_b, out);
		break;
	case MES_EXPR_GTE:
		mes_binary_expression_print(MES_EXPR_GTE, expr->sub_a, expr->sub_b, out);
		break;
	case MES_EXPR_EQ:
		mes_binary_expression_print(MES_EXPR_EQ, expr->sub_a, expr->sub_b, out);
		break;
	case MES_EXPR_NEQ:
		mes_binary_expression_print(MES_EXPR_NEQ, expr->sub_a, expr->sub_b, out);
		break;
	case MES_EXPR_IMM16:
		print_number(expr->arg16, out);
		break;
	case MES_EXPR_IMM32:
		print_number(expr->arg32, out);
		break;
	case MES_EXPR_REG16:
		port_printf(out, "var4[%u]", (unsigned)expr->arg16);
		break;
	case MES_EXPR_REG8:
		port_puts(out, "var4[");
		mes_expression_print(expr->sub_a, out);
		port_putc(out, ']');
		break;
	case MES_EXPR_ARRAY32_GET32:
		op_array32_get32_print(expr, out);
		break;
	case MES_EXPR_ARRAY32_GET16:
		port_printf(out, "var32[%d]->word[", (int)expr->arg8 - 1);
		mes_expression_print(expr->sub_a, out);
		port_putc(out, ']');
		break;
	case MES_EXPR_ARRAY32_GET8:
		port_printf(out, "var32[%d]->byte[", (int)expr->arg8 - 1);
		mes_expression_print(expr->sub_a, out);
		port_putc(out, ']');
		break;
	case MES_EXPR_VAR32:
		port_printf(out, "var32[%d]", (int)expr->arg8);
		break;
	case MES_EXPR_END:
		ERROR("encountered END expression when printing");
	}
}

void mes_expression_list_print(mes_expression_list list, struct port *out)
{
	for (unsigned i = 0; i < vector_length(list); i++) {
		if (i > 0)
			port_putc(out, ',');
		mes_expression_print(vector_A(list, i), out);
	}
}

// expressions }}}
// parameters {{{

void mes_parameter_print(struct mes_parameter *param, struct port *out)
{
	switch (param->type) {
	case MES_PARAM_STRING:
		port_printf(out, "\"%s\"", param->str);
		break;
	case MES_PARAM_EXPRESSION:
		mes_expression_print(param->expr, out);
		break;
	}
}

static void mes_parameter_list_print_from(mes_parameter_list list, unsigned start,
		struct port *out)
{
	port_putc(out, '(');
	for (unsigned i = start; i < vector_length(list); i++) {
		if (i > start)
			port_putc(out, ',');
		mes_parameter_print(&vector_A(list, i), out);
	}
	port_putc(out, ')');
}

void mes_parameter_list_print(mes_parameter_list list, struct port *out)
{
	mes_parameter_list_print_from(list, 0, out);
}

static void indent_print(struct port *out, int indent)
{
	for (int i = 0; i < indent; i++) {
		port_putc(out, '\t');
	}
}

// parameters }}}
// ASM statements {{{

static void mes_asm_statement_print(struct mes_statement *stmt, struct port *out, int indent)
{
	if (stmt->is_jump_target) {
		indent_print(out, indent - 1);
		port_printf(out, "L_%08x:\n", stmt->address);
	}
	indent_print(out, indent);

	switch (stmt->op) {
	case MES_STMT_END:
		port_puts(out, "END;\n");
		break;
	case MES_STMT_TXT:
		port_printf(out, "TXT \"%s\";\n", stmt->TXT.text);
		break;
	case MES_STMT_STR:
		port_printf(out, "STR \"%s\";\n", stmt->TXT.text);
		break;
	case MES_STMT_SETRBC:
		port_printf(out, "SETRBC[%u] = ", (unsigned)stmt->SETRBC.reg_no);
		mes_expression_list_print(stmt->SETRBC.exprs, out);
		port_puts(out, ";\n");
		break;
	case MES_STMT_SETV:
		port_printf(out, "SETV[%u] = ", (unsigned)stmt->SETV.var_no);
		mes_expression_list_print(stmt->SETV.exprs, out);
		port_puts(out, ";\n");
		break;
	case MES_STMT_SETRBE:
		port_puts(out, "SETRBE[");
		mes_expression_print(stmt->SETRBE.reg_expr, out);
		port_puts(out, "] = ");
		mes_expression_list_print(stmt->SETRBE.val_exprs, out);
		port_puts(out, ";\n");
		break;
	case MES_STMT_SETAC:
		port_printf(out, "SETAC[%u][", (unsigned)stmt->SETAC.var_no);
		mes_expression_print(stmt->SETAC.off_expr, out);
		port_puts(out, "] = ");
		mes_expression_list_print(stmt->SETAC.val_exprs, out);
		port_puts(out, ";\n");
		break;
	case MES_STMT_SETA_AT:
		port_printf(out, "SETA@[%u][", (unsigned)stmt->SETA_AT.var_no);
		mes_expression_print(stmt->SETA_AT.off_expr, out);
		port_puts(out, "] = ");
		mes_expression_list_print(stmt->SETA_AT.val_exprs, out);
		port_puts(out, ";\n");
		break;
	case MES_STMT_SETAD:
		port_printf(out, "SETAD[%u][", (unsigned)stmt->SETAD.var_no);
		mes_expression_print(stmt->SETAD.off_expr, out);
		port_puts(out, "] = ");
		mes_expression_list_print(stmt->SETAD.val_exprs, out);
		port_puts(out, ";\n");
		break;
	case MES_STMT_SETAW:
		port_printf(out, "SETAW[%u][", (unsigned)stmt->SETAW.var_no);
		mes_expression_print(stmt->SETAW.off_expr, out);
		port_puts(out, "] = ");
		mes_expression_list_print(stmt->SETAW.val_exprs, out);
		port_puts(out, ";\n");
		break;
	case MES_STMT_SETAB:
		port_printf(out, "SETAB[%u][", (unsigned)stmt->SETAB.var_no);
		mes_expression_print(stmt->SETAB.off_expr, out);
		port_puts(out, "] = ");
		mes_expression_list_print(stmt->SETAB.val_exprs, out);
		port_puts(out, ";\n");
		break;
	case MES_STMT_JZ:
		port_puts(out, "JZ ");
		mes_expression_print(stmt->JZ.expr, out);
		port_printf(out, " L_%08x;\n", stmt->JZ.addr);
		break;
	case MES_STMT_JMP:
		port_printf(out, "JMP L_%08x;\n", stmt->JMP.addr);
		break;
	case MES_STMT_SYS:
		port_puts(out, "SYS[");
		mes_expression_print(stmt->SYS.expr, out);
		port_putc(out, ']');
		mes_parameter_list_print(stmt->SYS.params, out);
		port_puts(out, ";\n");
		break;
	case MES_STMT_GOTO:
		port_puts(out, "GOTO");
		mes_parameter_list_print(stmt->GOTO.params, out);
		port_puts(out, ";\n");
		break;
	case MES_STMT_CALL:
		port_puts(out, "CALL");
		mes_parameter_list_print(stmt->CALL.params, out);
		port_puts(out, ";\n");
		break;
	case MES_STMT_MENUI:
		port_puts(out, "MENUI");
		mes_parameter_list_print(stmt->MENUI.params, out);
		port_printf(out, " L_%08x;\n", stmt->MENUI.addr);
		break;
	case MES_STMT_PROC:
		port_puts(out, "PROC");
		mes_parameter_list_print(stmt->PROC.params, out);
		port_puts(out, ";\n");
		break;
	case MES_STMT_UTIL:
		port_puts(out, "UTIL");
		mes_parameter_list_print(stmt->UTIL.params, out);
		port_puts(out, ";\n");
		break;
	case MES_STMT_LINE:
		port_printf(out, "LINE %u;\n", (unsigned)stmt->LINE.arg);
		break;
	case MES_STMT_PROCD:
		port_puts(out, "PROCD ");
		mes_expression_print(stmt->PROCD.no_expr, out);
		port_printf(out, " L_%08x;\n", stmt->PROCD.skip_addr);
		break;
	case MES_STMT_MENUS:
		port_puts(out, "MENUS;\n");
		break;
	case MES_STMT_SETRD:
		port_printf(out, "SETRD[%u] = ", (unsigned)stmt->SETRD.var_no);
		mes_expression_list_print(stmt->SETRD.val_exprs, out);
		port_puts(out, ";\n");
		break;
	}
}

void mes_asm_statement_list_print(mes_statement_list statements, struct port *out)
{
	struct mes_statement *stmt;
	vector_foreach(stmt, statements) {
		mes_asm_statement_print(stmt, out, 1);
	}
}

// ASM statements }}}
// statements {{{

static void stmt_seta_at_print(struct mes_statement *stmt, struct port *out)
{
	const char *name;
	if (stmt->SETA_AT.var_no == 0 && stmt->SETA_AT.off_expr->op == MES_EXPR_IMM
			&& (name = system_var16_name(stmt->SETA_AT.off_expr->arg8))) {
		port_printf(out, "System.%s", name);
	} else if (stmt->SETA_AT.var_no == 0) {
		port_puts(out, "System.var16[");
		mes_expression_print(stmt->SETA_AT.off_expr, out);
		port_putc(out, ']');
	} else {
		port_printf(out, "var16[%d]->word[", (int)stmt->SETA_AT.var_no - 1);
		mes_expression_print(stmt->SETA_AT.off_expr, out);
		port_putc(out, ']');
	}
	port_puts(out, " = ");
	mes_expression_list_print(stmt->SETA_AT.val_exprs, out);
	port_puts(out, ";\n");
}

static void stmt_setad_print(struct mes_statement *stmt, struct port *out)
{
	const char *name;
	if (stmt->SETAD.var_no == 0 && stmt->SETAD.off_expr->op == MES_EXPR_IMM
			&& (name = system_var32_name(stmt->SETAD.off_expr->arg8))) {
		port_printf(out, "System.%s", name);
	} else if (stmt->SETAD.var_no == 0) {
		port_puts(out, "System.var32[");
		mes_expression_print(stmt->SETAD.off_expr, out);
		port_putc(out, ']');
	} else {
		port_printf(out, "var32[%d]->dword[", (int)stmt->SETAD.var_no - 1);
		mes_expression_print(stmt->SETAD.off_expr, out);
		port_putc(out, ']');
	}
	port_puts(out, " = ");
	mes_expression_list_print(stmt->SETAD.val_exprs, out);
	port_puts(out, ";\n");
}

static int get_int_parameter(mes_parameter_list params, unsigned i)
{
	if (i >= vector_length(params))
		return -1;
	struct mes_parameter *p = &vector_A(params, i);
	if (p->type != MES_PARAM_EXPRESSION || p->expr->op != MES_EXPR_IMM)
		return -1;
	return p->expr->arg8;
}

static void stmt_sys_print(struct mes_statement *stmt, struct port *out)
{
	if (stmt->SYS.expr->op != MES_EXPR_IMM)
		goto fallback;

	int cmd;
	switch (stmt->SYS.expr->arg8) {
	case 0:
		port_puts(out, "System.set_font_size");
		mes_parameter_list_print(stmt->SYS.params, out);
		break;
	case 2:
		if ((cmd = get_int_parameter(stmt->SYS.params, 0)) < 0)
			goto fallback;
		if (cmd == 0)
			port_puts(out, "System.Cursor.load");
		else if (cmd == 1)
			port_puts(out, "System.Cursor.refresh");
		else if (cmd == 2)
			port_puts(out, "System.Cursor.save_pos");
		else if (cmd == 3)
			port_puts(out, "System.Cursor.set_pos");
		else if (cmd == 4)
			port_puts(out, "System.Cursor.open");
		else
			port_printf(out, "System.Cursor.function[%d]", cmd);
		mes_parameter_list_print_from(stmt->SYS.params, 1, out);
		break;
	case 3:
		if ((cmd = get_int_parameter(stmt->SYS.params, 0)) < 0)
			goto fallback;
		port_printf(out, "System.function[3].function[%d]", cmd);
		mes_parameter_list_print_from(stmt->SYS.params, 1, out);
		break;
	case 4:
		if ((cmd = get_int_parameter(stmt->SYS.params, 0)) < 0)
			goto fallback;
		else if (cmd == 1)
			port_puts(out, "System.SaveData.save");
		else if (cmd == 2)
			port_puts(out, "System.SaveData.load");
		else if (cmd == 3)
			port_puts(out, "System.SaveData.save_except_mes_name");
		else if (cmd == 4)
			port_puts(out, "System.SaveData.load_var4");
		else if (cmd == 5)
			port_puts(out, "System.SaveData.write_var4");
		else if (cmd == 6)
			port_puts(out, "System.SaveData.save_union_var4");
		else if (cmd == 7)
			port_puts(out, "System.SaveData.load_var4_slice");
		else if (cmd == 8)
			port_puts(out, "System.SaveData.save_var4_slice");
		else if (cmd == 9)
			port_puts(out, "System.SaveData.copy");
		else if (cmd == 11)
			// load var4[0x15]
			// load mes name
			// load var4[0x12]
			// load var4[0x1d]
			// load var4[0x32 - 0x59]
			// load var4[0x96 - 1999]
			// load mes file
			// init VM
			//  --- what to call this?
			port_puts(out, "System.SaveData.function[11]");
		else if (cmd == 12)
			// save savedMesName
			// save var4[0x32 - 0x59]
			// save var4[0x96 - 2000]
			// --- what to call this?
			port_puts(out, "System.SaveData.function[12]");
		else if (cmd == 13)
			port_puts(out, "System.SaveData.set_mes_name");
		else
			port_printf(out, "System.SaveData.function[%d]", cmd);
		mes_parameter_list_print_from(stmt->SYS.params, 1, out);
		break;
	case 5:
		if ((cmd = get_int_parameter(stmt->SYS.params, 0)) < 0)
			goto fallback;
		port_printf(out, "System.Audio.function[%d]", cmd);
		mes_parameter_list_print_from(stmt->SYS.params, 1, out);
		break;
	case 7:
		if ((cmd = get_int_parameter(stmt->SYS.params, 0)) < 0)
			goto fallback;
		if (cmd == 0)
			port_puts(out, "System.File.read");
		else if (cmd == 1)
			port_puts(out, "System.File.write");
		else
			port_printf(out, "System.File.function[%d]", cmd);
		mes_parameter_list_print_from(stmt->SYS.params, 1, out);
		break;
	case 8:
		port_puts(out, "System.load_image");
		mes_parameter_list_print(stmt->SYS.params, out);
		break;
	case 9:
		if ((cmd = get_int_parameter(stmt->SYS.params, 0)) < 0)
			goto fallback;
		port_printf(out, "System.Palette.function[%d]", cmd);
		mes_parameter_list_print_from(stmt->SYS.params, 1, out);
		break;
	case 10:
		if ((cmd = get_int_parameter(stmt->SYS.params, 0)) < 0)
			goto fallback;
		port_printf(out, "System.Image.function[%d]", cmd);
		mes_parameter_list_print_from(stmt->SYS.params, 1, out);
		break;
	case 12:
		port_puts(out, "System.set_text_colors");
		mes_parameter_list_print(stmt->SYS.params, out);
		break;
	case 13:
		port_puts(out, "System.farcall");
		mes_parameter_list_print(stmt->SYS.params, out);
		break;
	case 16:
		port_puts(out, "System.get_time");
		mes_parameter_list_print(stmt->SYS.params, out);
		break;
	case 17:
		port_puts(out, "System.noop");
		mes_parameter_list_print(stmt->SYS.params, out);
		break;
	case 19:
		if ((cmd = get_int_parameter(stmt->SYS.params, 0)) < 0)
			goto fallback;
		port_printf(out, "System.function[19].function[%d]", cmd);
		mes_parameter_list_print_from(stmt->SYS.params, 1, out);
		break;
	case 20:
		port_puts(out, "System.noop2");
		mes_parameter_list_print(stmt->SYS.params, out);
		break;
	case 21:
		port_puts(out, "System.strlen");
		mes_parameter_list_print(stmt->SYS.params, out);
		break;
	case 22:
		if ((cmd = get_int_parameter(stmt->SYS.params, 0)) < 0)
			goto fallback;
		port_printf(out, "System.function[22].function[%d]", cmd);
		mes_parameter_list_print_from(stmt->SYS.params, 1, out);
		break;
	default:
		goto fallback;
	}
	port_puts(out, ";\n");
	return;

fallback:
	port_puts(out, "System.function[");
	mes_expression_print(stmt->SYS.expr, out);
	port_putc(out, ']');
	mes_parameter_list_print(stmt->SYS.params, out);
	port_puts(out, ";\n");
}

static void _mes_statement_print(struct mes_statement *stmt, struct port *out, int indent)
{
	indent_print(out, indent);

	switch (stmt->op) {
	case MES_STMT_END:
		port_puts(out, "return;\n");
		break;
	case MES_STMT_TXT:
		// SJIS string
		if (stmt->TXT.unprefixed)
			port_puts(out, "unprefixed ");
		if (!stmt->TXT.terminated)
			port_puts(out, "unterminated ");
		port_printf(out, "\"%s\";\n", stmt->TXT.text);
		break;
	case MES_STMT_STR:
		// ASCII string
		if (stmt->TXT.unprefixed)
			port_puts(out, "unprefixed ");
		if (!stmt->TXT.terminated)
			port_puts(out, "unterminated ");
		port_printf(out, "\"%s\";\n", stmt->TXT.text);
		break;
	case MES_STMT_SETRBC:
		// var4[v] = ...;
		port_printf(out, "var4[%u] = ", (unsigned)stmt->SETRBC.reg_no);
		mes_expression_list_print(stmt->SETRBC.exprs, out);
		port_puts(out, ";\n");
		break;
	case MES_STMT_SETV:
		// var16[v] = ...;
		port_printf(out, "var16[%u] = ", (unsigned)stmt->SETV.var_no);
		mes_expression_list_print(stmt->SETV.exprs, out);
		port_puts(out, ";\n");
		break;
	case MES_STMT_SETRBE:
		// var4[e] = ...;
		port_puts(out, "var4[");
		mes_expression_print(stmt->SETRBE.reg_expr, out);
		port_puts(out, "] = ");
		mes_expression_list_print(stmt->SETRBE.val_exprs, out);
		port_puts(out, ";\n");
		break;
	case MES_STMT_SETAC:
		// var16[v]->byte[e] = ...;
		port_printf(out, "var16[%u]->byte[", (unsigned)stmt->SETAC.var_no);
		mes_expression_print(stmt->SETAC.off_expr, out);
		port_puts(out, "] = ");
		mes_expression_list_print(stmt->SETAC.val_exprs, out);
		port_puts(out, ";\n");
		break;
	case MES_STMT_SETA_AT:
		// var16[v-1]->word[e] = ...;
		// System.var16[e] = ...; // when v = 0
		stmt_seta_at_print(stmt, out);
		break;
	case MES_STMT_SETAD:
		// var32[v]->dword[e] = ...;
		// System.var32[e] = ...; // when v = 0
		stmt_setad_print(stmt, out);
		break;
	case MES_STMT_SETAW:
		// var32[v]->word[e] = ...;
		port_printf(out, "var32[%d]->word[", (int)stmt->SETAW.var_no);
		mes_expression_print(stmt->SETAW.off_expr, out);
		port_puts(out, "] = ");
		mes_expression_list_print(stmt->SETAW.val_exprs, out);
		port_puts(out, ";\n");
		break;
	case MES_STMT_SETAB:
		// var32[v]->byte[e] = ...;
		port_printf(out, "var32[%d]->byte[", (int)stmt->SETAB.var_no);
		mes_expression_print(stmt->SETAB.off_expr, out);
		port_puts(out, "] = ");
		mes_expression_list_print(stmt->SETAB.val_exprs, out);
		port_puts(out, ";\n");
		break;
	case MES_STMT_JZ:
		//WARNING("Encountered JZ as AST statement");
		port_puts(out, "jz ");
		mes_expression_print(stmt->JZ.expr, out);
		port_printf(out, " L_%08x;\n", stmt->JZ.addr);
		break;
	case MES_STMT_JMP:
		port_printf(out, "goto L_%08x;\n", stmt->JMP.addr);
		break;
	case MES_STMT_SYS:
		stmt_sys_print(stmt, out);
		break;
	case MES_STMT_GOTO:
		port_puts(out, "jump");
		mes_parameter_list_print(stmt->GOTO.params, out);
		port_puts(out, ";\n");
		break;
	case MES_STMT_CALL:
		port_puts(out, "call");
		mes_parameter_list_print(stmt->CALL.params, out);
		port_puts(out, ";\n");
		break;
	case MES_STMT_MENUI:
		//WARNING("Encountered MENUI as AST statement");
		port_puts(out, "defmenu");
		mes_parameter_list_print(stmt->MENUI.params, out);
		port_printf(out, " L_%08x;\n", stmt->MENUI.addr);
		break;
	case MES_STMT_PROC:
		port_puts(out, "call");
		mes_parameter_list_print(stmt->PROC.params, out);
		port_puts(out, ";\n");
		break;
	case MES_STMT_UTIL:
		// TODO: interpret first parameter as function name
		port_puts(out, "util");
		mes_parameter_list_print(stmt->UTIL.params, out);
		port_puts(out, ";\n");
		break;
	case MES_STMT_LINE:
		port_printf(out, "line %u;\n", (unsigned)stmt->LINE.arg);
		break;
	case MES_STMT_PROCD:
		//WARNING("Encountered PROCD as AST statement");
		port_puts(out, "defproc ");
		mes_expression_print(stmt->PROCD.no_expr, out);
		port_printf(out, " L_%08x;\n", stmt->PROCD.skip_addr);
		break;
	case MES_STMT_MENUS:
		port_puts(out, "menuexec;\n");
		break;
	case MES_STMT_SETRD:
		port_printf(out, "var32[%d] = ", (unsigned)stmt->SETRD.var_no);
		mes_expression_list_print(stmt->SETRD.val_exprs, out);
		port_puts(out, ";\n");
		break;
	}
}

void mes_statement_print(struct mes_statement *stmt, struct port *out)
{
	_mes_statement_print(stmt, out, 1);
}

static void _mes_statement_list_print(mes_statement_list statements, struct port *out,
		int indent, bool ast)
{
	struct mes_statement *stmt;
	vector_foreach(stmt, statements) {
		_mes_statement_print(stmt, out, indent);
	}
}

void mes_statement_list_print(mes_statement_list statements, struct port *out)
{
	_mes_statement_list_print(statements, out, 1, false);
}

void mes_flat_statement_list_print(mes_statement_list statements, struct port *out)
{
	struct mes_statement *stmt;
	vector_foreach(stmt, statements) {
		if (stmt->is_jump_target)
			port_printf(out, "L_%08x:\n", stmt->address);
		_mes_statement_print(stmt, out, 1);
	}
}

// statements }}}
// blocks {{{

static void mes_block_edge_print(struct mes_basic_block *block, struct port *out, int indent)
{
	struct mes_statement *edge = block->end;
	if (edge->is_jump_target) {
		indent_print(out, indent - 1);
		port_printf(out, "L_%08x:\n", edge->address);
	}
	indent_print(out, indent);

	switch (edge->op) {
	case MES_STMT_JZ:
		port_puts(out, "JZ ");
		mes_expression_print(edge->JZ.expr, out);
		port_printf(out, " L_%08x; // %04d\n", edge->JZ.addr,
				block->jump_target->post);
		break;
	case MES_STMT_JMP:
		port_printf(out, "JMP L_%08x; // %04d\n", edge->JMP.addr,
				block->jump_target->post);
		break;
	case MES_STMT_END:
		port_printf(out, "END;\n");
		break;
	default:
		ERROR("Unexpected statement in block edge: %d", (int)edge->op);
	}
}

static void _mes_block_print(struct mes_block *block, struct port *out, int indent)
{
	indent_print(out, indent);
	port_printf(out, "// -------- %04d --------\n", block->post);
	if (block->type == MES_BLOCK_BASIC) {
		_mes_statement_list_print(block->basic.statements, out, indent, true);
		if (block->basic.end) {
			mes_block_edge_print(&block->basic, out, indent);
		}
		return;
	}

	// header
	indent_print(out, indent);
	if (block->compound.head->op == MES_STMT_MENUI) {
		port_puts(out, "menu[");
		mes_parameter_list_print(block->compound.head->MENUI.params, out);
		port_puts(out, "] = {\n");
	} else if (block->compound.head->op == MES_STMT_PROCD) {
		port_puts(out, "procedure[");
		mes_expression_print(block->compound.head->PROCD.no_expr, out);
		port_puts(out, "] = {\n");
	} else assert(false);

	// body
	struct mes_block *child;
	vector_foreach(child, block->compound.blocks) {
		_mes_block_print(child, out, indent + 1);
	}

	// footer
	indent_print(out, indent);
	port_puts(out, "}; // end of ");
	if (block->compound.head->op == MES_STMT_MENUI) {
		port_puts(out, "menu entry ");
		mes_parameter_list_print(block->compound.head->MENUI.params, out);
	} else if (block->compound.head->op == MES_STMT_PROCD) {
		port_puts(out, "procedure ");
		mes_expression_print(block->compound.head->PROCD.no_expr, out);
	} else assert(false);
	port_puts(out, "\n\n");
}

void mes_block_print(struct mes_block *block, struct port *out)
{
	// handle toplevel as special case
	if (block->type == MES_BLOCK_COMPOUND && !block->compound.head) {
		struct mes_block *child;
		vector_foreach(child, block->compound.blocks) {
			_mes_block_print(child, out, 0);
		}
	} else {
		_mes_block_print(block, out, 0);
	}
}

void mes_block_list_print(mes_block_list blocks, struct port *out)
{
	struct mes_block *block;
	vector_foreach(block, blocks) {
		if (block->type == MES_BLOCK_BASIC)
			_mes_block_print(block, out, 1);
		else
			_mes_block_print(block, out, 0);
	}
}

static void _mes_block_tree_print(struct mes_block *block, struct port *out, int indent)
{
	indent_print(out, indent);

	port_printf(out, "[%d] ", block->post);
	if (block->type == MES_BLOCK_BASIC) {
		port_printf(out, "%u STATEMENTS", (unsigned)vector_length(block->basic.statements));
		if (block->basic.end) {
			if (block->basic.end->op == MES_STMT_JZ) {
				port_puts(out, ", JZ");
			} else if (block->basic.end->op == MES_STMT_JMP) {
				port_puts(out, ", END");
			} else if (block->basic.end->op == MES_STMT_END) {
				port_puts(out, ", END");
			} else {
				port_printf(out, ", %d (BUG)", (int)block->basic.end->op);
			}
		}
		port_putc(out, '\n');
		return;
	}
	if (block->compound.head->op == MES_STMT_MENUI) {
		port_puts(out, "MENU ENTRY ");
		mes_parameter_list_print(block->compound.head->MENUI.params, out);
	} else if (block->compound.head->op == MES_STMT_PROCD) {
		port_puts(out, "PROCEDURE ");
		mes_expression_print(block->compound.head->PROCD.no_expr, out);
	} else assert(false);
	port_putc(out, '\n');

	struct mes_block *child;
	vector_foreach(child, block->compound.blocks) {
		_mes_block_tree_print(child, out, indent + 1);
	}
}

void mes_block_tree_print(mes_block_list blocks, struct port *out)
{
	struct mes_block *block;
	vector_foreach(block, blocks) {
		_mes_block_tree_print(block, out, 0);
	}
}

// blocks }}}
// AST {{{

static void mes_ast_node_print(struct mes_ast *node, struct port *out, int indent);

static void _mes_ast_block_print(mes_ast_block block, struct port *out, int indent)
{
	struct mes_ast *node;
	vector_foreach(node, block) {
		mes_ast_node_print(node, out, indent);
	}
}

void mes_ast_block_print(mes_ast_block block, struct port *out)
{
	_mes_ast_block_print(block, out, 0);
}

static void mes_ast_cond_print(struct mes_ast_if *cond, struct port *out, int indent)
{
	port_puts(out, "if (");
	mes_expression_print(cond->condition, out);
	port_puts(out, ") {\n");
	_mes_ast_block_print(cond->consequent, out, indent + 1);
	if (vector_length(cond->alternative) > 0) {
		indent_print(out, indent);
		struct mes_ast *alt = vector_A(cond->alternative, 0);
		if (vector_length(cond->alternative) == 1 && alt->type == MES_AST_COND) {
			port_puts(out, "} else ");
			mes_ast_cond_print(&alt->cond, out, indent);
			return;
		}
		port_puts(out, "} else {\n");
		_mes_ast_block_print(cond->alternative, out, indent + 1);
	}
	indent_print(out, indent);
	port_puts(out, "}\n");
}

static bool stmt_is_normal_text(struct mes_statement *stmt)
{
	return (stmt->op == MES_STMT_TXT || stmt->op == MES_STMT_STR)
		&& stmt->TXT.terminated && !stmt->TXT.unprefixed;
}

static void mes_ast_statement_list_print(mes_statement_list statements, struct port *out,
		int indent)
{
	string text = NULL;
	for (unsigned i = 0; i < vector_length(statements); i++) {
		struct mes_statement *stmt = vector_A(statements, i);
		struct mes_statement *next = i + 1 < vector_length(statements) ?
			vector_A(statements, i+1) : NULL;
		if (stmt_is_normal_text(stmt)) {
			if (!text)
				text = string_dup(stmt->TXT.text);
			else
				text = string_concat(text, stmt->TXT.text);
			continue;
		}
		if (text && stmt->op == MES_STMT_PROC && next && stmt_is_normal_text(next)) {
			int f = get_int_parameter(stmt->PROC.params, 0);
			if (f >= 0) {
				text = string_concat_fmt(text, "$%i", f);
				continue;
			}
		}
		if (text) {
			indent_print(out, indent);
			port_printf(out, "\"%s\";\n", text);
			string_free(text);
			text = NULL;
		}
		_mes_statement_print(stmt, out, indent);
	}
	if (text) {
		indent_print(out, indent);
		port_printf(out, "\"%s\";\n", text);
		string_free(text);
	}
}

static void mes_ast_node_print(struct mes_ast *node, struct port *out, int indent)
{
	if (node->is_goto_target) {
		indent_print(out, indent - 1);
		port_printf(out, "L_%08x:\n", node->address);
	}
	switch (node->type) {
	case MES_AST_STATEMENTS:
		mes_ast_statement_list_print(node->statements, out, indent);
		break;
	case MES_AST_COND:
		indent_print(out, indent);
		mes_ast_cond_print(&node->cond, out, indent);
		break;
	case MES_AST_LOOP:
		indent_print(out, indent);
		port_puts(out, "while (");
		mes_expression_print(node->loop.condition, out);
		port_puts(out, ") {\n");
		_mes_ast_block_print(node->loop.body, out, indent + 1);
		indent_print(out, indent);
		port_puts(out, "}\n");
		break;
	case MES_AST_PROCEDURE:
		port_putc(out, '\n');
		indent_print(out, indent);
		port_puts(out, "procedure[");
		mes_expression_print(node->proc.num_expr, out);
		port_puts(out, "] = {\n");
		_mes_ast_block_print(node->proc.body, out, indent + 1);
		indent_print(out, indent);
		port_puts(out, "};\n");
		break;
	case MES_AST_MENU_ENTRY:
		indent_print(out, indent);
		port_puts(out, "menu[");
		mes_parameter_list_print(node->menu.params, out);
		port_puts(out, "] = {\n");
		_mes_ast_block_print(node->menu.body, out, indent + 1);
		indent_print(out, indent);
		port_puts(out, "};\n");
		break;
	case MES_AST_CONTINUE:
		indent_print(out, indent);
		port_puts(out, "continue;\n");
		break;
	case MES_AST_BREAK:
		indent_print(out, indent);
		port_puts(out, "break;\n");
		break;
	}
}

void mes_ast_print(struct mes_ast *node, struct port *out)
{
	mes_ast_node_print(node, out, 0);
}

// AST }}}
