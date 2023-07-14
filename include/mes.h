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

#include "nulib/vector.h"

typedef char *string;

#define MES_ADDRESS_SYNTHETIC 0xFFFFFFFF

/*
 * XXX: These opcode values don't necessarily correspond with those
 *      used in any particular game. They should be considered
 *      virtual opcodes for internal use. When parsing or compiling,
 *      the correct opcode should be looked up in the appropriate
 *      table for the target game.
 */
enum mes_statement_op {
	MES_STMT_END = 0x00,
	MES_STMT_TXT = 0x01,
	MES_STMT_STR = 0x02,
	MES_STMT_SETRBC = 0x03,
	MES_STMT_SETV = 0x04,
	MES_STMT_SETRBE = 0x05,
	MES_STMT_SETAC = 0x06,
	MES_STMT_SETA_AT = 0x07,
	MES_STMT_SETAD = 0x08,
	MES_STMT_SETAW = 0x09,
	MES_STMT_SETAB = 0x0A,
	MES_STMT_JZ = 0x0B,
	MES_STMT_JMP = 0x0C,
	MES_STMT_SYS = 0x0D,
	MES_STMT_GOTO = 0x0E,
	MES_STMT_CALL = 0x0F,
	MES_STMT_MENUI = 0x10,
	MES_STMT_PROC = 0x11,
	MES_STMT_UTIL = 0x12,
	MES_STMT_LINE = 0x13,
	MES_STMT_PROCD = 0x14,
	MES_STMT_MENUS = 0x15,
	MES_STMT_SETRD = 0x16,
};

#define MES_STMT_INVALID 0xFF

enum mes_expression_op {
	MES_EXPR_IMM = 0x00, // not a real op
	MES_EXPR_VAR = 0x80,
	MES_EXPR_ARRAY16_GET16 = 0xA0,
	MES_EXPR_ARRAY16_GET8 = 0xC0,
	MES_EXPR_PLUS = 0xE0,
	MES_EXPR_MINUS = 0xE1,
	MES_EXPR_MUL = 0xE2,
	MES_EXPR_DIV = 0xE3,
	MES_EXPR_MOD = 0xE4,
	MES_EXPR_RAND = 0xE5,
	MES_EXPR_AND = 0xE6,
	MES_EXPR_OR = 0xE7,
	MES_EXPR_BITAND = 0xE8,
	MES_EXPR_BITIOR = 0xE9,
	MES_EXPR_BITXOR = 0xEA,
	MES_EXPR_LT = 0xEB,
	MES_EXPR_GT = 0xEC,
	MES_EXPR_LTE = 0xED,
	MES_EXPR_GTE = 0xEE,
	MES_EXPR_EQ = 0xEF,
	MES_EXPR_NEQ = 0xF0,
	MES_EXPR_IMM16 = 0xF1,
	MES_EXPR_IMM32 = 0xF2,
	MES_EXPR_REG16 = 0xF3, // 16-bit *index*
	MES_EXPR_REG8 = 0xF4, // 8-bit *index*
	MES_EXPR_ARRAY32_GET32 = 0xF5,
	MES_EXPR_ARRAY32_GET16 = 0xF6,
	MES_EXPR_ARRAY32_GET8 = 0xF7,
	MES_EXPR_VAR32 = 0xF8,
	MES_EXPR_END = 0xFF
};

enum mes_parameter_type {
	MES_PARAM_STRING = 1,
	MES_PARAM_EXPRESSION = 2,
};

struct mes_expression {
	enum mes_expression_op op;
	union {
		uint8_t arg8;
		uint16_t arg16;
		uint32_t arg32;
	};
	// unary operand / binary LHS operand
	struct mes_expression *sub_a;
	// binary RHS operand
	struct mes_expression *sub_b;
};

struct mes_parameter {
	enum mes_parameter_type type;
	union {
		// XXX: Actual max size of string parameter is 24, but the VM doesn't
		//      bounds check and the limit is exceeded in some cases (e.g.
		//      Doukyuusei/NAME.MES).
		char str[64];
		struct mes_expression *expr;
	};
};

typedef vector_t(struct mes_expression*) mes_expression_list;
typedef vector_t(struct mes_parameter) mes_parameter_list;

struct mes_statement {
	enum mes_statement_op op;
	uint32_t address;
	uint32_t next_address;
	bool is_jump_target;
	union {
		struct {
			string text;
			bool terminated;
			bool unprefixed;
		} TXT;
		struct {
			uint16_t reg_no;
			mes_expression_list exprs;
		} SETRBC;
		struct {
			uint8_t var_no;
			mes_expression_list exprs;
		} SETV;
		struct {
			struct mes_expression *reg_expr;
			mes_expression_list val_exprs;
		} SETRBE;
		struct {
			uint8_t var_no;
			struct mes_expression *off_expr;
			mes_expression_list val_exprs;
		} SETAC;
		struct {
			uint8_t var_no;
			struct mes_expression *off_expr;
			mes_expression_list val_exprs;
		} SETA_AT;
		struct {
			uint8_t var_no;
			struct mes_expression *off_expr;
			mes_expression_list val_exprs;
		} SETAD;
		struct {
			uint8_t var_no;
			struct mes_expression *off_expr;
			mes_expression_list val_exprs;
		} SETAW;
		struct {
			uint8_t var_no;
			struct mes_expression *off_expr;
			mes_expression_list val_exprs;
		} SETAB;
		struct {
			uint32_t addr;
			struct mes_expression *expr;
		} JZ;
		struct {
			uint32_t addr;
		} JMP;
		struct {
			struct mes_expression *expr;
			mes_parameter_list params;
		} SYS;
		struct {
			mes_parameter_list params;
		} GOTO;
		struct {
			mes_parameter_list params;
		} CALL;
		struct {
			uint32_t addr; // ???
			mes_parameter_list params;
		} MENUI;
		struct {
			mes_parameter_list params;
		} PROC;
		struct {
			mes_parameter_list params;
		} UTIL;
		struct {
			uint8_t arg;
		} LINE;
		struct {
			uint32_t skip_addr;
			struct mes_expression *no_expr;
		} PROCD;
		struct {
			uint8_t var_no;
			mes_expression_list val_exprs;
		} SETRD;
	};
};

typedef vector_t(struct mes_statement*) mes_statement_list;

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

struct port;

bool mes_parse_statements(uint8_t *data, size_t data_size, mes_statement_list *statements);
void mes_expression_free(struct mes_expression *expr);
void mes_parameter_list_free(mes_parameter_list list);
void mes_statement_free(struct mes_statement *stmt);
void mes_statement_list_free(mes_statement_list list);
void mes_block_list_free(mes_block_list list);
void mes_ast_free(struct mes_ast* node);
void mes_ast_block_free(mes_ast_block block);

bool mes_decompile(uint8_t *data, size_t data_size, mes_ast_block *out);
bool mes_decompile_debug(uint8_t *data, size_t data_size, mes_block_list *out);

void mes_expression_print(struct mes_expression *expr, struct port *out);
void mes_expression_list_print(mes_expression_list list, struct port *out);
void mes_parameter_print(struct mes_parameter *param, struct port *out);
void mes_parameter_list_print(mes_parameter_list list, struct port *out);
void mes_statement_print(struct mes_statement *stmt, struct port *out);
void mes_statement_list_print(mes_statement_list statements, struct port *out);
void mes_ast_print(struct mes_ast *node, struct port *out);
void mes_ast_block_print(mes_ast_block block, struct port *out);

void mes_block_list_print(mes_block_list blocks, struct port *out);
void mes_block_tree_print(mes_block_list blocks, struct port *out);

enum game_id;
void mes_set_game(enum game_id id);

#endif // ELF_TOOLS_MES_H
