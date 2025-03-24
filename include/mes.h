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

#ifndef ELF_TOOLS_MES_H
#define ELF_TOOLS_MES_H

#include <stdint.h>
#include <stdio.h>

#include "ai5/mes.h"

struct port;

enum mes_block_type {
	MES_BLOCK_BASIC,
	MES_BLOCK_COMPOUND,
};

struct mes_block;
typedef vector_t(struct mes_block*) mes_block_list;

// statement list with no internal control-flow
struct mes_basic_block {
	mes_statement_list statements;
	struct mes_statement *end;

	// target of JZ/JMP (otherwise NULL)
	struct mes_block *jump_target;
	// fallthrough block (NULL for JMP or terminal block)
	struct mes_block *fallthrough;
};

// procedure or menu entry
struct mes_compound_block {
	struct mes_statement *head;
	uint32_t end_address;
	mes_block_list blocks;

	struct mes_block *next;
	mes_block_list post;
};

struct mes_block {
	// type: basic (statements list) or compound (toplevel, procedure, or menu entry)
	enum mes_block_type type;
	// CFG predecessors
	mes_block_list pred;
	// CFG successors
	mes_block_list succ;
	// domination frontier
	mes_block_list dom_front;
	// dominated blocks
	mes_block_list dom;
	// parent block (compound)
	struct mes_block *parent;
	// post-order number
	int post;
	uint32_t address;
	bool in_ast;
	union {
		struct mes_basic_block basic;
		struct mes_compound_block compound;
	};
};

enum mes_ast_type {
	// list of statements
	MES_AST_STATEMENTS,
	// if (...) { ... } else { ... }
	MES_AST_COND,
	// while (...) { ... }
	MES_AST_LOOP,
	// procedure <n> { ... }
	MES_AST_PROCEDURE,
	// menu <n> { ... }
	MES_AST_MENU_ENTRY,
	// continue
	MES_AST_CONTINUE,
	// break
	MES_AST_BREAK,
};

typedef vector_t(struct mes_ast*) mes_ast_block;

struct mes_ast_if {
	struct mes_expression *condition;
	mes_ast_block consequent;
	mes_ast_block alternative;
};

struct mes_ast_while {
	struct mes_expression *condition;
	mes_ast_block body;
};

struct mes_ast_procedure {
	struct mes_expression *num_expr;
	mes_ast_block body;
};

struct mes_ast_menu_entry {
	mes_parameter_list params;
	mes_ast_block body;
};

struct mes_ast {
	enum mes_ast_type type;
	uint32_t address;
	bool is_goto_target;
	union {
		mes_statement_list statements;
		struct mes_ast_if cond;
		struct mes_ast_while loop;
		struct mes_ast_procedure proc;
		struct mes_ast_menu_entry menu;
	};
};

// ctor.c
struct mes_statement *mes_stmt(enum mes_statement_op op);
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
struct mes_statement *mes_stmt_jz(struct mes_expression *cond);
struct mes_statement *mes_stmt_jmp(void);
struct mes_statement *mes_stmt_sys(struct mes_expression *expr, mes_parameter_list params);
struct mes_statement *mes_stmt_goto(mes_parameter_list params);
struct mes_statement *mes_stmt_call(mes_parameter_list params);
struct mes_statement *mes_stmt_proc(mes_parameter_list params);
struct mes_statement *mes_stmt_menui(mes_parameter_list params);
struct mes_statement *mes_stmt_util(mes_parameter_list params);
struct mes_statement *mes_stmt_line(uint8_t arg);
struct mes_statement *mes_stmt_procd(struct mes_expression *expr);
struct mes_statement *mes_stmt_menus(void);
struct mes_expression *mes_expr(enum mes_expression_op op);
struct mes_expression *mes_expr_constant(long i);
struct mes_expression *mes_expr_var4(struct mes_expression *index);
struct mes_expression *mes_expr_var16(uint8_t no);
struct mes_expression *mes_expr_var32(uint8_t no);
struct mes_expression *mes_expr_system_var16(struct mes_expression *index);
struct mes_expression *mes_expr_system_var32(struct mes_expression *index);
struct mes_expression *mes_expr_array_index(enum mes_expression_op op, uint8_t var_no,
		struct mes_expression *index);
struct mes_expression *mes_expr_random(struct mes_expression *limit);
struct mes_expression *mes_binary_expr(enum mes_expression_op op, struct mes_expression *a,
		struct mes_expression *b);
struct mes_parameter mes_param_str(string text);
struct mes_parameter mes_param_expr(struct mes_expression *expr);

struct mes_statement *aiw_mes_stmt(enum aiw_mes_statement_op op);
struct mes_statement *aiw_mes_stmt_end(void);
struct mes_statement *aiw_mes_stmt_set_flag(struct mes_expression *var, mes_expression_list vals);
struct mes_statement *aiw_mes_stmt_set_var16(struct mes_expression *var, mes_expression_list vals);
struct mes_statement *aiw_mes_stmt_set_sysvar(struct mes_expression *var, mes_expression_list vals);
struct mes_statement *aiw_mes_stmt_set_var32(uint8_t no, struct mes_expression *val);
struct mes_statement *aiw_mes_stmt_ptr_set8(uint8_t no, struct mes_expression *off_expr,
		mes_expression_list vals);
struct mes_statement *aiw_mes_stmt_ptr_set16(uint8_t no, struct mes_expression *off_expr,
		mes_expression_list vals);
struct mes_statement *aiw_mes_stmt_jz(struct mes_expression *cond);
struct mes_statement *aiw_mes_stmt_jmp(void);
struct mes_statement *_aiw_mes_stmt_call(enum aiw_mes_statement_op op, mes_parameter_list params);
struct mes_statement *aiw_mes_stmt_defproc(struct mes_expression *no_expr);
struct mes_statement *aiw_mes_stmt_menuexec(mes_expression_list exprs);
struct aiw_mes_menu_case aiw_mes_menu_case(struct mes_expression *expr, mes_statement_list body);
struct mes_statement *aiw_mes_stmt_defmenu(struct mes_expression *expr, aiw_menu_table cases);
struct mes_statement *aiw_mes_stmt_0x35(uint16_t a, uint16_t b);
struct mes_statement *aiw_mes_stmt_0x37(uint32_t i);
struct mes_statement *aiw_mes_stmt_0xfe(void);
struct mes_expression *aiw_mes_expr(enum aiw_mes_expression_op op);
struct mes_expression *aiw_mes_expr_var4(struct mes_expression *index);
struct mes_expression *aiw_mes_expr_var16(struct mes_expression *index);
struct mes_expression *aiw_mes_expr_sysvar(struct mes_expression *index);
struct mes_expression *aiw_mes_expr_var32(uint8_t no);
struct mes_expression *aiw_mes_expr_ptr_get8(uint8_t no, struct mes_expression *off_expr);
struct mes_expression *aiw_mes_expr_random(uint16_t limit);

void mes_block_list_free(mes_block_list list);
void mes_ast_free(struct mes_ast* node);
void mes_ast_block_free(mes_ast_block block);

bool mes_decompile(uint8_t *data, size_t data_size, mes_ast_block *out);
bool mes_decompile_debug(uint8_t *data, size_t data_size, mes_block_list *out);

void mes_ast_print(struct mes_ast *node, int name_function, struct port *out);
void mes_ast_block_print(mes_ast_block block, int name_function, struct port *out);
void mes_text_print(mes_statement_list statements, struct port *out, int name_function);

void mes_block_list_print(mes_block_list blocks, struct port *out);
void mes_block_tree_print(mes_block_list blocks, struct port *out);

uint32_t mes_statement_size(struct mes_statement *stmt);
uint32_t aiw_mes_statement_size(struct mes_statement *stmt);

mes_statement_list mes_flat_parse(const char *path);
uint8_t *mes_pack(mes_statement_list stmts, size_t *size_out);

void mes_statement_list_foreach_text(mes_statement_list statements,
		int name_function,
		void(*handle_text)(string text, struct mes_statement*, unsigned, void*),
		void(*handle_statement)(struct mes_statement*, void*),
		void *data);

struct mes_text_line {
	char *text;
	unsigned columns;
};

struct mes_text_substitution {
	int no;
	int columns;
	string from;
	vector_t(struct mes_text_line) to;
};

typedef vector_t(struct mes_text_substitution) mes_text_sub_list;

bool mes_text_parse(FILE *f, mes_text_sub_list *out);
void mes_text_sub_list_free(mes_text_sub_list list);
mes_statement_list mes_substitute_text(mes_statement_list mes, mes_text_sub_list subs_in);

#endif // ELF_TOOLS_MES_H
