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
#include "ai5/game.h"
#include "mes.h"

// text iterator {{{

static int get_int_parameter(mes_parameter_list params, unsigned i)
{
	if (i >= vector_length(params))
		return -1;
	struct mes_parameter *p = &vector_A(params, i);
	if (p->type != MES_PARAM_EXPRESSION || p->expr->op != MES_EXPR_IMM)
		return -1;
	return p->expr->arg8;
}

static bool stmt_is_normal_text(struct mes_statement *stmt)
{
	if (game_is_aiwin())
		return stmt->aiw_op == AIW_MES_STMT_TXT;
	return (stmt->op == MES_STMT_ZENKAKU || stmt->op == MES_STMT_HANKAKU)
		&& stmt->TXT.terminated && !stmt->TXT.unprefixed;
}

void mes_statement_list_foreach_text(mes_statement_list statements,
		int name_function,
		void(*handle_text)(string text, struct mes_statement*, unsigned, void*),
		void(*handle_statement)(struct mes_statement*, void*),
		void *data)
{
	string text = NULL;
	struct mes_statement *text_stmt = NULL;
	unsigned text_nr_statements = 0;
	for (unsigned i = 0; i < vector_length(statements); i++) {
		struct mes_statement *stmt = vector_A(statements, i);
		struct mes_statement *next = i + 1 < vector_length(statements) ?
			vector_A(statements, i + 1) : NULL;
		if (stmt_is_normal_text(stmt)) {
			if (!text) {
				text = string_dup(stmt->TXT.text);
				text_stmt = stmt;
				text_nr_statements = 1;
			} else if (stmt->is_jump_target) {
				handle_text(text, text_stmt, text_nr_statements, data);
				string_free(text);
				text = string_dup(stmt->TXT.text);
				text_stmt = stmt;
				text_nr_statements = 1;
			} else {
				text = string_concat(text, stmt->TXT.text);
				text_nr_statements++;
			}
			continue;
		}
		if (text && stmt->op == MES_STMT_CALL_PROC && next && stmt_is_normal_text(next)) {
			int f = get_int_parameter(stmt->CALL.params, 0);
			if (f >= 0 && f == name_function) {
				text = string_concat_fmt(text, "$%i", f);
				text_nr_statements++;
				continue;
			}
		}
		if (text) {
			handle_text(text, text_stmt, text_nr_statements, data);
			string_free(text);
			text = NULL;
		}
		if (handle_statement)
			handle_statement(stmt, data);
	}
	if (text) {
		handle_text(text, text_stmt, text_nr_statements, data);
		string_free(text);
	}
}

// text iterator }}}
// blocks {{{

static void indent_print(struct port *out, int indent)
{
	for (int i = 0; i < indent; i++) {
		port_putc(out, '\t');
	}
}

static enum mes_virtual_op vop(struct mes_statement *stmt)
{
	if (game_is_aiwin())
		return mes_aiw_vop(stmt);
	return mes_ai5_vop(stmt);
}

static void mes_block_edge_print(struct mes_basic_block *block, struct port *out, int indent)
{
	struct mes_statement *edge = block->end;
	if (edge->is_jump_target) {
		indent_print(out, indent - 1);
		port_printf(out, "L_%08x:\n", edge->address);
	}
	indent_print(out, indent);

	switch (vop(edge)) {
	case VOP_JZ:
		port_puts(out, "JZ ");
		mes_expression_print(edge->JZ.expr, out);
		port_printf(out, " L_%08x; // %04d\n", edge->JZ.addr,
				block->jump_target->post);
		break;
	case VOP_JMP:
		port_printf(out, "JMP L_%08x; // %04d\n", edge->JMP.addr,
				block->jump_target->post);
		break;
	case VOP_END:
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
		_mes_statement_list_print(block->basic.statements, out, indent);
		if (block->basic.end) {
			mes_block_edge_print(&block->basic, out, indent);
		}
		return;
	}

	enum mes_virtual_op op = vop(block->compound.head);

	// header
	indent_print(out, indent);
	if (op == VOP_DEF_MENU) {
		port_puts(out, "menu[");
		mes_parameter_list_print(block->compound.head->DEF_MENU.params, out);
		port_puts(out, "] = {\n");
	} else if (op == VOP_DEF_PROC) {
		port_puts(out, "procedure[");
		mes_expression_print(block->compound.head->DEF_PROC.no_expr, out);
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
	if (op == VOP_DEF_MENU) {
		port_puts(out, "menu entry ");
		mes_parameter_list_print(block->compound.head->DEF_MENU.params, out);
	} else if (op == VOP_DEF_PROC) {
		port_puts(out, "procedure ");
		mes_expression_print(block->compound.head->DEF_PROC.no_expr, out);
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
			switch (vop(block->basic.end)) {
			case VOP_JZ:  port_puts(out, ", JZ"); break;
			case VOP_JMP: port_puts(out, ", END"); break;
			case VOP_END: port_puts(out, ", END"); break;
			default:
				port_printf(out, ", %d (BUG)", (int)block->basic.end->op);
				break;
			}
		}
		port_putc(out, '\n');
		return;
	}
	switch (vop(block->compound.head)) {
	case VOP_DEF_MENU:
		port_puts(out, "MENU ENTRY ");
		mes_parameter_list_print(block->compound.head->DEF_MENU.params, out);
		break;
	case VOP_DEF_PROC:
		port_puts(out, "PROCEDURE ");
		mes_expression_print(block->compound.head->DEF_PROC.no_expr, out);
		break;
	default:
		assert(false);
	}
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

static void mes_ast_node_print(struct mes_ast *node, int name_function, struct port *out,
		int indent);

static void _mes_ast_block_print(mes_ast_block block, int name_function, struct port *out,
		int indent)
{
	struct mes_ast *node;
	vector_foreach(node, block) {
		mes_ast_node_print(node, name_function, out, indent);
	}
}

void mes_ast_block_print(mes_ast_block block, int name_function, struct port *out)
{
	_mes_ast_block_print(block, name_function, out, 0);
}

static void mes_ast_cond_print(struct mes_ast_if *cond, int name_function, struct port *out, int indent)
{
	port_puts(out, "if (");
	mes_expression_print(cond->condition, out);
	port_puts(out, ") {\n");
	_mes_ast_block_print(cond->consequent, name_function, out, indent + 1);
	if (vector_length(cond->alternative) > 0) {
		indent_print(out, indent);
		struct mes_ast *alt = vector_A(cond->alternative, 0);
		if (vector_length(cond->alternative) == 1 && alt->type == MES_AST_COND) {
			port_puts(out, "} else ");
			mes_ast_cond_print(&alt->cond, name_function, out, indent);
			return;
		}
		port_puts(out, "} else {\n");
		_mes_ast_block_print(cond->alternative, name_function, out, indent + 1);
	}
	indent_print(out, indent);
	port_puts(out, "}\n");
}

struct statement_list_print_data {
	int indent;
	struct port *out;
};

static void mes_ast_statement_list_print_text(string text, struct mes_statement *stmt,
		unsigned nr_stmt, void *_data)
{
	struct statement_list_print_data *data = _data;
	indent_print(data->out, data->indent);
	port_printf(data->out, "\"%s\";\n", text);
}

static void mes_ast_statement_list_print_stmt(struct mes_statement *stmt, void *_data)
{
	struct statement_list_print_data *data = _data;
	if (game_is_aiwin())
		_aiw_mes_statement_print(stmt, data->out, data->indent);
	else
		_mes_statement_print(stmt, data->out, data->indent);
}

static void mes_ast_statement_list_print(mes_statement_list statements, int name_function,
		struct port *out, int indent)
{
	struct statement_list_print_data data = { .indent = indent, .out = out };
	mes_statement_list_foreach_text(statements, -1,
			mes_ast_statement_list_print_text,
			mes_ast_statement_list_print_stmt,
			&data);
}

static void mes_ast_node_print(struct mes_ast *node, int name_function, struct port *out,
		int indent)
{
	if (node->is_goto_target) {
		indent_print(out, indent - 1);
		port_printf(out, "L_%08x:\n", node->address);
	}
	switch (node->type) {
	case MES_AST_STATEMENTS:
		mes_ast_statement_list_print(node->statements, name_function, out, indent);
		break;
	case MES_AST_COND:
		indent_print(out, indent);
		mes_ast_cond_print(&node->cond, name_function, out, indent);
		break;
	case MES_AST_LOOP:
		indent_print(out, indent);
		port_puts(out, "while (");
		mes_expression_print(node->loop.condition, out);
		port_puts(out, ") {\n");
		_mes_ast_block_print(node->loop.body, name_function, out, indent + 1);
		indent_print(out, indent);
		port_puts(out, "}\n");
		break;
	case MES_AST_PROCEDURE:
		port_putc(out, '\n');
		indent_print(out, indent);
		port_puts(out, "procedure[");
		mes_expression_print(node->proc.num_expr, out);
		port_puts(out, "] = {\n");
		_mes_ast_block_print(node->proc.body, name_function, out, indent + 1);
		indent_print(out, indent);
		port_puts(out, "};\n");
		break;
	case MES_AST_MENU_ENTRY:
		indent_print(out, indent);
		port_puts(out, "menu[");
		mes_parameter_list_print(node->menu.params, out);
		port_puts(out, "] = {\n");
		_mes_ast_block_print(node->menu.body, name_function, out, indent + 1);
		indent_print(out, indent);
		port_puts(out, "};\n");
		break;
	case MES_AST_SUB:
		port_putc(out, '\n');
		indent_print(out, indent);
		port_puts(out, "sub[");
		mes_expression_print(node->proc.num_expr, out);
		port_puts(out, "] = {\n");
		_mes_ast_block_print(node->proc.body, name_function, out, indent + 1);
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

void mes_ast_print(struct mes_ast *node, int name_function, struct port *out)
{
	mes_ast_node_print(node, name_function, out, 0);
}

// AST }}}
// {{{ Text

static void _mes_text_print(struct port *out, int i, string text)
{
	port_printf(out, "# %d \"%s\"\n\n", i, text);
}

struct text_print_data {
	int count;
	struct port *out;
};

static void text_print_handle_text(string text, struct mes_statement *stmt,
		unsigned nr_stmt, void *_data)
{
	struct text_print_data *data = _data;
	_mes_text_print(data->out, data->count++, text);
}

void mes_text_print(mes_statement_list statements, struct port *out, int name_function)
{
	struct text_print_data data = { .out = out };
	mes_statement_list_foreach_text(statements, name_function, text_print_handle_text,
			NULL, &data);
}

// }}} Text
