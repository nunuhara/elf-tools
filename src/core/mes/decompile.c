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
#include "nulib/hashset.h"
#include "nulib/hashtable.h"
#include "nulib/vector.h"
#include "ai5/game.h"
#include "mes.h"

#define DECOMPILER_WARNING(fmt, ...) \
	sys_warning("*WARNING*: " fmt "\n", ##__VA_ARGS__)

enum mes_virtual_op mes_ai5_vop(struct mes_statement *stmt)
{
	switch (stmt->op) {
	case MES_STMT_END: return VOP_END;
	case MES_STMT_JZ: return VOP_JZ;
	case MES_STMT_JMP: return VOP_JMP;
	case MES_STMT_DEF_PROC: return VOP_DEF_PROC;
	case MES_STMT_DEF_MENU: return VOP_DEF_MENU;
	case MES_STMT_DEF_SUB: return VOP_DEF_SUB;
	default: return VOP_OTHER;
	}
}

enum mes_virtual_op mes_aiw_vop(struct mes_statement *stmt)
{
	switch (stmt->aiw_op) {
	case AIW_MES_STMT_END: return VOP_END;
	case AIW_MES_STMT_JZ: return VOP_JZ;
	case AIW_MES_STMT_JMP: return VOP_JMP;
	case AIW_MES_STMT_DEF_PROC: return VOP_DEF_PROC;
	default: return VOP_OTHER;
	}
}

int mes_ai5_vop_to_op(enum mes_virtual_op op)
{
	switch (op) {
	case VOP_END: return MES_STMT_END;
	case VOP_JZ: return MES_STMT_JZ;
	case VOP_JMP: return MES_STMT_JMP;
	case VOP_DEF_PROC: return MES_STMT_DEF_PROC;
	case VOP_DEF_MENU: return MES_STMT_DEF_MENU;
	case VOP_DEF_SUB: return MES_STMT_DEF_SUB;
	default: ERROR("cannot convert virtual op: %d", op);
	}
}

int mes_aiw_vop_to_op(enum mes_virtual_op op)
{
	switch (op) {
	case VOP_END: return AIW_MES_STMT_END;
	case VOP_JZ: return AIW_MES_STMT_JZ;
	case VOP_JMP: return AIW_MES_STMT_JMP;
	case VOP_DEF_PROC: return AIW_MES_STMT_DEF_PROC;
	default: ERROR("cannot convert virtual op: %d", op);
	}
}

static enum mes_virtual_op (*vop)(struct mes_statement*) = mes_ai5_vop;
static int (*vop_to_op)(enum mes_virtual_op) = mes_ai5_vop_to_op;

static void vop_init(void)
{
	if (game_is_aiwin()) {
		vop = mes_aiw_vop;
		vop_to_op = mes_aiw_vop_to_op;
	} else {
		vop = mes_ai5_vop;
		vop_to_op = mes_ai5_vop_to_op;
	}
}

// Phase 1: CFG {{{
// CFG create compound blocks {{{

static struct mes_block *make_basic_block(mes_statement_list statements, struct mes_statement *end)
{
	struct mes_block *block = xcalloc(1, sizeof(struct mes_block));
	block->type = MES_BLOCK_BASIC;
	block->post = -1;
	if (vector_empty(statements)) {
		assert(end != NULL);
		block->address = end->address;
	} else {
		block->address = vector_A(statements, 0)->address;
	}
	block->basic.statements = statements;
	block->basic.end = end;
	return block;
}

static struct mes_block *make_compound_block(struct mes_statement *head)
{
	assert(vop(head) == VOP_DEF_MENU || vop(head) == VOP_DEF_PROC || vop(head) == VOP_DEF_SUB);
	struct mes_block *block = xcalloc(1, sizeof(struct mes_block));
	block->type = MES_BLOCK_COMPOUND;
	block->post = -1;
	block->address = head->address;
	block->compound.head = head;
	vector_init(block->compound.blocks);

	if (vop(head) == VOP_DEF_MENU) {
		block->compound.end_address = head->DEF_MENU.skip_addr - 1;
	} else {
		block->compound.end_address = head->DEF_PROC.skip_addr - 1;
	}

	return block;
}

static void add_child_block(struct mes_block *parent, struct mes_block *child)
{
	assert(parent->type == MES_BLOCK_COMPOUND);
	child->parent = parent;
	vector_push(struct mes_block*, parent->compound.blocks, child);
}

static void push_statements(mes_statement_list *statements, struct mes_block *block)
{
	if (vector_length(*statements) == 0)
		return;
	struct mes_block *stmt_block = make_basic_block(*statements, NULL);
	add_child_block(block, stmt_block);
	vector_init(*statements);
}

/*
 * Group statements belonging to procedures/menu entries into compound blocks.
 * Statement lists are stored in basic block objects, although they are not yet
 * grouped into basic blocks. This is pass 1 of the CFG construction process.
 */
static void cfg_create_compound_blocks(struct mes_block *toplevel, mes_statement_list statements)
{
	if (vector_length(statements) == 0)
		return;
		//return toplevel;

	struct mes_block *stack[512] = { [0] = toplevel };
	int stack_ptr = 1;

	// the current statements block
	mes_statement_list current;
	vector_init(current);

	toplevel->compound.end_address = vector_A(statements, vector_length(statements)-1)->address;
	if (unlikely(vop(vector_A(statements, vector_length(statements)-1)) != VOP_END))
ERROR("mes file is not terminated by END statement");

	struct mes_statement *stmt;
	vector_foreach(stmt, statements) {
		assert(stack_ptr > 0);
		// end of container block: push statements and pop block stack
		if (stmt->address == stack[stack_ptr-1]->compound.end_address) {
			if (unlikely(vop(stmt) != VOP_END))
				ERROR("expected END statement at %08x", stmt->address);
			--stack_ptr;
			vector_push(struct mes_statement*, current, stmt);
			push_statements(&current, stack[stack_ptr]);
		}
		// start of container block: push statements then push new block to stack
		else if (vop(stmt) == VOP_DEF_MENU || vop(stmt) == VOP_DEF_PROC || vop(stmt) == VOP_DEF_SUB) {
			push_statements(&current, stack[stack_ptr-1]);
			struct mes_block *new_block = make_compound_block(stmt);
			add_child_block(stack[stack_ptr-1], new_block);
			stack[stack_ptr++] = new_block;
		}
		// regular statement: add to current statements list
		else {
			vector_push(struct mes_statement*, current, stmt);
		}
	}

	assert(stack_ptr == 0);
	assert(vector_length(current) == 0);

	vector_destroy(statements);
	//return toplevel;
}

// CFG create compound blocks }}}
// CFG create basic blocks {{{

/*
 * Split a statement list into a list of basic blocks (a basic block being a list of
 * statements with no internal control flow). This is pass 2 of the CFG construction
 * process.
 */
static void cfg_statements_to_basic_blocks(mes_statement_list statements, struct mes_block *parent)
{
	mes_statement_list current;
	vector_init(current);

	struct mes_statement *stmt;
	vector_foreach(stmt, statements) {
		if (stmt->is_jump_target && vector_length(current) > 0) {
			// jump target: push current statement list as new basic block
			add_child_block(parent, make_basic_block(current, NULL));
			vector_init(current);
		}
		if (vop(stmt) == VOP_JZ || vop(stmt) == VOP_JMP || vop(stmt) == VOP_END) {
			// control flow: new basic block with statement as outgoing edge
			struct mes_block *new_block = make_basic_block(current, stmt);
			add_child_block(parent, new_block);
			vector_init(current);
		} else {
			// normal statement: push to current list
			vector_push(struct mes_statement*, current, stmt);
		}
	}

	// terminal block
	if (vector_length(current) > 0) {
		add_child_block(parent, make_basic_block(current, NULL));
	}

	vector_destroy(statements);
}

/*
 * Split statement lists into lists of basic blocks. This is pass 2 of the CFG
 * construction process.
 */
static void cfg_create_basic_blocks(struct mes_block *parent)
{
	mes_block_list in = parent->compound.blocks;
	vector_init(parent->compound.blocks);

	struct mes_block *block;
	vector_foreach(block, in) {
		if (block->type == MES_BLOCK_BASIC) {
			cfg_statements_to_basic_blocks(block->basic.statements, parent);
			free(block);
		} else if (block->type == MES_BLOCK_COMPOUND) {
			cfg_create_basic_blocks(block);
			add_child_block(parent, block);
		} else assert(false);
	}

	vector_destroy(in);
}

// CFG create basic blocks }}}
// CFG create edges {{{

declare_hashtable_int_type(block_table, struct mes_block*);
define_hashtable_int(block_table, struct mes_block*);

/*
 * Look up a block by address.
 */
static struct mes_block *block_table_get(hashtable_t(block_table) *table, int addr)
{
	hashtable_iter_t k = hashtable_get(block_table, table, addr);
	if (k == hashtable_end(table))
		ERROR("block address lookup failed for %08x", (unsigned)addr);
	return hashtable_val(table, k);
}

/*
 * Create a hash table associating blocks with their start addresses.
 */
static void init_block_table(mes_block_list blocks, hashtable_t(block_table) *table)
{
	struct mes_block *block;
	vector_foreach(block, blocks) {
		uint32_t addr;
		if (block->type == MES_BLOCK_BASIC) {
			if (vector_length(block->basic.statements) > 0) {
				addr = vector_A(block->basic.statements, 0)->address;
			} else if (block->basic.end) {
				addr = block->basic.end->address;
			} else assert(false);
		} else if (block->type == MES_BLOCK_COMPOUND) {
			addr = block->compound.head->address;
			init_block_table(block->compound.blocks, table);
		} else assert(false);

		int ret;
		hashtable_iter_t k = hashtable_put(block_table, table, addr, &ret);
		if (unlikely(ret == HASHTABLE_KEY_PRESENT))
			ERROR("multiple blocks with same address");
		hashtable_val(table, k) = block;
	}
}

static void cfg_create_edge(struct mes_block *src, struct mes_block *dst)
{
	assert(src); assert(dst);
	vector_push(struct mes_block*, src->succ, dst);
	vector_push(struct mes_block*, dst->pred, src);
}

static void cfg_create_edges(struct mes_compound_block *parent, hashtable_t(block_table) *table)
{
	for (unsigned i = 0; i < vector_length(parent->blocks); i++) {
		struct mes_block *block = vector_A(parent->blocks, i);
		struct mes_block *next = i + 1 < vector_length(parent->blocks) ?
			vector_A(parent->blocks, i + 1) : NULL;
		if (block->type == MES_BLOCK_BASIC) {
			struct mes_statement *end = block->basic.end;
			if (end && vop(end) == VOP_JZ) {
				block->basic.fallthrough = next;
				if (next)
					cfg_create_edge(block, next);
				block->basic.jump_target = block_table_get(table, end->JZ.addr);
				cfg_create_edge(block, block->basic.jump_target);
			} else if (end && vop(end) == VOP_JMP) {
				block->basic.jump_target = block_table_get(table, end->JMP.addr);
				cfg_create_edge(block, block->basic.jump_target);
			} else if (end && vop(end) == VOP_END) {
				// nothing (terminal block)
			} else {
				block->basic.fallthrough = next;
				if (next)
					cfg_create_edge(block, next);
			}
		} else if (block->type == MES_BLOCK_COMPOUND) {
			block->compound.next = next;
			if (next)
				cfg_create_edge(block, next);
			// XXX: We recursively create edges for compound blocks. Note however
			//      that the graph for a compound block *should* be entirely
			//      disconnected from the parent graph we are creating here!
			cfg_create_edges(&block->compound, table);
		} else assert(false);
	}
}

/*
 * Create control-flow edges between blocks. This is pass 3 of the CFG construction
 * process.
 */
static void cfg_create_graph(struct mes_compound_block *toplevel)
{
	hashtable_t(block_table) table = hashtable_initializer(block_table);
	init_block_table(toplevel->blocks, &table);
	cfg_create_edges(toplevel, &table);
	hashtable_destroy(block_table, &table);
}

// CFG create edges }}}
// CFG dominance {{{

static void cfg_postorder(struct mes_block *block, mes_block_list *list)
{
	block->post = 9999; // XXX: to prevent cycles
	struct mes_block *succ;
	vector_foreach(succ, block->succ) {
		if (succ->post >= 0)
			continue;
		cfg_postorder(succ, list);
	}
	block->post = vector_length(*list);
	vector_push(struct mes_block*, *list, block);
}

static int cfg_dom_intersect(int *doms, int b1, int b2)
{
	int finger1 = b1;
	int finger2 = b2;
	while (finger1 != finger2) {
		assert(finger1 >= 0);
		assert(finger2 >= 0);
		while (finger1 < finger2) {
			finger1 = doms[finger1];
		}
		while (finger2 < finger1) {
			finger2 = doms[finger2];
		}
	}
	return finger1;
}

static void cfg_add_to_dominance_frontier(struct mes_block *block, struct mes_block *front)
{
	struct mes_block *b;
	vector_foreach(b, block->dom_front) {
		if (b == front)
			return;
	}
	vector_push(struct mes_block*, block->dom_front, front);
}

static void cfg_dom(struct mes_compound_block *compound)
{
	// create post-order array to simplify traversal
	struct mes_block *start = vector_A(compound->blocks, 0);
	cfg_postorder(start, &compound->post);

	// initialize dominators array
	const unsigned len = vector_length(compound->post);
	int *doms = xmalloc(len * sizeof(int));
	for (unsigned i = 0; i < len; i++) {
		doms[i] = -1;
	}
	doms[start->post] = start->post;

	// compute dominators
	bool changed = true;
	while (changed) {
		changed = false;
		for (unsigned b_i = 0; b_i < len; b_i++) {
			struct mes_block *b = vector_A(compound->post, b_i);
			if (b == start)
				continue;
			assert(vector_length(b->pred) > 0);
			int new_idom = -1;
			struct mes_block *p;
			vector_foreach(p, b->pred) {
				// XXX: dead code
				if (p->post < 0)
					continue;
				if (doms[p->post] == -1)
					continue;
				if (new_idom < 0) {
					new_idom = p->post;
					continue;
				}
				new_idom = cfg_dom_intersect(doms, p->post, new_idom);
				assert(new_idom >= 0);
			}
			if (doms[b->post] != new_idom) {
				doms[b->post] = new_idom;
				changed = true;
			}
		}
	}

	// compute dominance frontiers
	struct mes_block *b;
	vector_foreach(b, compound->post) {
		// XXX: dead code
		if (vector_length(b->pred) < 2)
			continue;
		struct mes_block *p;
		vector_foreach(p, b->pred) {
			if (p->post < 0)
				continue;
			int runner = p->post;
			while (runner != doms[b->post]) {
				cfg_add_to_dominance_frontier(vector_A(compound->post, runner), b);
				runner = doms[runner];
				assert(runner >= 0);
			}
		}
	}

	// analyze dominance relationships of child CFGs
	vector_foreach(b, compound->blocks) {
		// XXX: dead code
		if (b->post < 0)
			continue;
		if (b->type == MES_BLOCK_COMPOUND)
			cfg_dom(&b->compound);
	}

	for (unsigned i = 0; i < len; i++) {
		struct mes_block *dominated = vector_A(compound->post, i);
		for (int j = i; doms[j] != j; j = doms[j]) {
			assert(j >= 0 && j < len);
			struct mes_block *dominator = vector_A(compound->post, j);
			vector_push(struct mes_block*, dominator->dom, dominated);
		}
	}
	free(doms);
}

// CFG dominance }}}
// CFG check {{{

/*
 * Check that a jump statement doesn't escape to an unrelated scope (i.e. into or out of
 * a procedure or menu entry).
 */
static void check_jump(struct mes_statement *stmt, struct mes_block *parent)
{
	uint32_t addr;
	if (vop(stmt) == VOP_JZ) {
		addr = stmt->JZ.addr;
	} else if (vop(stmt) == VOP_JMP) {
		addr = stmt->JMP.addr;
	} else {
		return;
	}

	struct mes_block *block;
	vector_foreach(block, parent->compound.blocks) {
		if (block->type == MES_BLOCK_COMPOUND) {
			// jump to head of compound block is ok
			if (addr == block->compound.head->address)
				return;
			continue;
		}

		// determine start address of block
		uint32_t start_addr;
		if (vector_length(block->basic.statements) > 0) {
			start_addr = vector_A(block->basic.statements, 0)->address;
		} else {
			assert(block->basic.end);
			start_addr = block->basic.end->address;
		}

		// determine end address of block
		uint32_t end_addr;
		if (block->basic.end) {
			end_addr = block->basic.end->address;
		} else {
			unsigned nr_statements = vector_length(block->basic.statements);
			assert(nr_statements > 0);
			end_addr = vector_A(block->basic.statements, nr_statements-1)->next_address;
		}

		// check if jump is between start and end addresses for this block
		if (addr >= start_addr && addr <= end_addr)
			return;
	}
	ERROR("jump escapes local scope at %08x -> %08x", stmt->address, addr);
}

static void check_basic_block(struct mes_block *block, struct mes_block *parent)
{
	if (block->basic.end)
		check_jump(block->basic.end, parent);
}

static void check_block(struct mes_block *block, struct mes_block *parent);

static void check_compound_block(struct mes_block *block)
{
	struct mes_block *child;
	vector_foreach(child, block->compound.blocks) {
		check_block(child, block);
	}
}

static void check_block(struct mes_block *block, struct mes_block *parent)
{
	switch (block->type) {
	case MES_BLOCK_BASIC:
		check_basic_block(block, parent);
		break;
	case MES_BLOCK_COMPOUND:
		check_compound_block(block);
		break;
	}
}

// CFG check }}}

/*
 * Create the control flow graph from the list of statements representing a .mes file
 * (as returned by mes_parse_statements).
 */
static void cfg_create(struct mes_block *toplevel, mes_statement_list statements)
{
	// 1st pass: group procedure and menu entry statements into compound blocks
	cfg_create_compound_blocks(toplevel, statements);
	// 2nd pass: split toplevel/procedure/menu entry statement lists into basic blocks
	cfg_create_basic_blocks(toplevel);
	// 3rd pass: connect blocks by analyzing basic block incoming/outgoing links
	cfg_create_graph(&toplevel->compound);
	// 4th pass: analyze dominance relationships
	cfg_dom(&toplevel->compound);

	// 5th pass: sanity check
	struct mes_block *block;
	vector_foreach(block, toplevel->compound.blocks) {
		check_block(block, toplevel);
	}
}

// Phase 1: CFG }}}
// Phase 2: AST {{{
// AST Create {{{

static struct mes_ast *make_ast_node(enum mes_ast_type type, uint32_t address)
{
	struct mes_ast *node = xcalloc(1, sizeof(struct mes_ast));
	node->type = type;
	node->address = address;
	return node;
}

static bool block_list_contains(mes_block_list list, struct mes_block *end)
{
	struct mes_block *b;
	vector_foreach(b, list) {
		if (b == end)
			return true;
	}
	return false;
}

/*
 * Determine the converge point of the two branches of a JZ statement.
 * We have to subtract the parent block's dominance frontier from the frontiers of both
 * branches to handle the below case properly.
 *
 * while (...) { // parent block
 *     if (...) { // a
 *         if (...) break;
 *         <...>
 *     } else { // b
 *         <...>
 *     }
 *     <...> <- converge point
 * }
 * <...> <- this block must be removed from a->dom_front
 */
static struct mes_block *converge_point(struct mes_block *a, struct mes_block *b, mes_block_list front)
{
	mes_block_list a_front = vector_initializer;
	mes_block_list b_front = vector_initializer;

	struct mes_block *block;
	vector_foreach(block, a->dom_front) {
		if (block->post == a->post || block_list_contains(front, block))
			continue;
		vector_push(struct mes_block*, a_front, block);
	}
	vector_foreach(block, b->dom_front) {
		if (block->post == b->post || block_list_contains(front, block))
			continue;
		vector_push(struct mes_block*, b_front, block);
	}

	struct mes_block *converge = NULL;
	if (vector_empty(a_front) && vector_empty(b_front))
		block = NULL;
	else if (vector_length(a_front) == 1 && vector_length(b_front) < 2)
		converge = vector_A(a_front, 0);
	else if (vector_length(b_front) == 1 && vector_length(a_front) < 2)
		converge = vector_A(b_front, 0);
	else // FIXME?
		ERROR("Failed to find converge point of %d and %d", a->post, b->post);
	vector_destroy(a_front);
	vector_destroy(b_front);
	return converge;
}

static void ast_create_block(mes_ast_block *block, struct mes_block *parent, struct mes_block *head);

static struct mes_block *ast_create_node(mes_ast_block *ast_block, struct mes_block *parent,
		struct mes_block *head, mes_block_list frontier)
{
	if (head->in_ast) {
		ERROR("LOOP at %d", head->post);
	}
	head->in_ast = true;

	// compound block
	if (head->type == MES_BLOCK_COMPOUND) {
		mes_ast_block *body;
		if (vop(head->compound.head) == VOP_DEF_PROC) {
			// procedure definition
			struct mes_ast *node = make_ast_node(MES_AST_PROCEDURE, head->address);
			node->proc.num_expr = head->compound.head->DEF_PROC.no_expr;
			vector_push(struct mes_ast*, *ast_block, node);
			body = &node->proc.body;
		} else if (vop(head->compound.head) == VOP_DEF_SUB) {
			// SUB definition
			struct mes_ast *node = make_ast_node(MES_AST_SUB, head->address);
			node->proc.num_expr = head->compound.head->DEF_PROC.no_expr;
			vector_push(struct mes_ast*, *ast_block, node);
			body = &node->proc.body;
		} else {
			// menu entry definition
			assert(vop(head->compound.head) == VOP_DEF_MENU);
			struct mes_ast *node = make_ast_node(MES_AST_MENU_ENTRY, head->address);
			node->menu.params = head->compound.head->DEF_MENU.params;
			vector_push(struct mes_ast*, *ast_block, node);
			body = &node->menu.body;
		}
		if (vector_length(head->compound.blocks) > 0) {
			ast_create_block(body, head, vector_A(head->compound.blocks, 0));
		}
		free(head->compound.head);
		return head->compound.next;
	}

	// basic block
	struct mes_basic_block *basic = &head->basic;
	if (vector_length(basic->statements) > 0) {
		struct mes_ast *node = make_ast_node(MES_AST_STATEMENTS, head->address);
		node->statements = basic->statements;
		vector_push(struct mes_ast*, *ast_block, node);
	}

	if (!basic->end) {
		// XXX: we insert a synthetic JMP to the fallthrough block so that blocks
		//      can be freely reordered. The simplification phase will typically
		//      remove all of these jumps as redundant.
		if (basic->fallthrough) {
			basic->end = xcalloc(1, sizeof(struct mes_statement));
			basic->end->op = vop_to_op(VOP_JMP);
			basic->end->address = MES_ADDRESS_SYNTHETIC;
			basic->end->JMP.addr = basic->fallthrough->address;
			struct mes_ast *node = make_ast_node(MES_AST_STATEMENTS, basic->end->address);
			vector_push(struct mes_statement*, node->statements, basic->end);
			vector_push(struct mes_ast*, *ast_block, node);
		}
		return basic->fallthrough;
	} else if (vop(basic->end) == VOP_JZ) {
		assert(basic->jump_target && basic->fallthrough);
		if (block_list_contains(head->dom_front, head)) {
			// while loop
			struct mes_ast *node = make_ast_node(MES_AST_LOOP, basic->end->address);
			node->loop.condition = basic->end->JZ.expr;
			free(basic->end);
			vector_push(struct mes_ast*, *ast_block, node);
			ast_create_block(&node->loop.body, parent, basic->fallthrough);
			return basic->jump_target;
		} else {
			// conditional
			struct mes_ast *node = make_ast_node(MES_AST_COND, basic->end->address);
			node->cond.condition = basic->end->JZ.expr;
			free(basic->end);
			vector_push(struct mes_ast*, *ast_block, node);
			// FIXME: should probably insert synthetic JMP here too...
			// empty body, no else clause
			if (basic->jump_target == basic->fallthrough)
				return basic->fallthrough;
			// consequent
			ast_create_block(&node->cond.consequent, parent, basic->fallthrough);
			if (block_list_contains(basic->fallthrough->dom_front, basic->jump_target)
					|| block_list_contains(frontier, basic->jump_target)) {
				// no else clause
				return basic->jump_target;
			}
			// else clause
			ast_create_block(&node->cond.alternative, parent, basic->jump_target);
			return converge_point(basic->fallthrough, basic->jump_target, frontier);
		}
	} else if (vop(basic->end) == VOP_JMP || vop(basic->end) == VOP_END) {
		// goto or return: just put the original statement back into the AST
		// (they will be cleaned up later during simplification)
		struct mes_ast *node = make_ast_node(MES_AST_STATEMENTS, basic->end->address);
		vector_push(struct mes_statement*, node->statements, basic->end);
		vector_push(struct mes_ast*, *ast_block, node);
		// XXX: loose blocks are handled in ast_create_block
		return NULL;
	}
	ERROR("unexpected statement as CFG edge");
}

static void _ast_create_block(mes_ast_block *block, struct mes_block *parent,
		struct mes_block *head, mes_block_list frontier)
{
	do {
		head = ast_create_node(block, parent, head, frontier);
	} while (head && !block_list_contains(frontier, head));
}

static void ast_create_block(mes_ast_block *block, struct mes_block *parent, struct mes_block *head)
{
	_ast_create_block(block, parent, head, head->dom_front);

	// Loop over the blocks dominated by head. If any block hasn't been included into
	// the AST, put it at the end.
	struct mes_block *p;
	vector_foreach(p, head->dom) {
		if (!p->in_ast) {
			_ast_create_block(block, parent, p, head->dom_front);
		}
	}
}

static void ast_create(struct mes_block *cfg_toplevel, mes_ast_block *ast_toplevel)
{
	// XXX: hack so that toplevel head has empty dom_front
	struct mes_block head = {
		.type = MES_BLOCK_BASIC,
		.basic = {
			.statements = vector_initializer,
			.fallthrough = vector_A(cfg_toplevel->compound.blocks, 0)
			//.fallthrough = vector_A(state->cfg_toplevel.compound.blocks, 0)
		}
	};
	ast_create_block(ast_toplevel, cfg_toplevel, &head);
}

// AST Create }}}
// AST Simplify {{{

declare_hashtable_int_type(ast_table, struct mes_ast*);
define_hashtable_int(ast_table, struct mes_ast*);

static void ast_block_simplify(hashtable_t(ast_table) *table, mes_ast_block block,
		struct mes_ast *continuation,
		struct mes_ast *loop_head,
		struct mes_ast *loop_break);

static void ast_simplify_jmp(hashtable_t(ast_table) *table, struct mes_ast *node,
		struct mes_statement *stmt, struct mes_ast *continuation,
		struct mes_ast *loop_head, struct mes_ast *loop_break)
{
	assert(vector_length(node->statements) == 1);
	if (continuation && stmt->JMP.addr == continuation->address) {
		// jump to continuation: eliminate JMP instruction
		vector_length(node->statements)--;
		mes_statement_free(stmt);
	} else if (loop_head && stmt->JMP.addr == loop_head->address) {
		// continue
		mes_statement_list_free(node->statements);
		node->type = MES_AST_CONTINUE;
	} else if (loop_break && stmt->JMP.addr == loop_break->address) {
		// break
		mes_statement_list_free(node->statements);
		node->type = MES_AST_BREAK;
	} else {
		// goto
		hashtable_iter_t k = hashtable_get(ast_table, table, stmt->JMP.addr);
		if (k == hashtable_end(table))
			ERROR("AST node lookup failed for %08x", (unsigned)stmt->JMP.addr);
		hashtable_val(table, k)->is_goto_target = true;
	}
}

static void ast_node_simplify(hashtable_t(ast_table) *table, struct mes_ast *node,
		struct mes_ast *continuation,
		struct mes_ast *loop_head,
		struct mes_ast *loop_break)
{
	struct mes_statement *stmt;
	switch (node->type) {
	case MES_AST_STATEMENTS:
		assert(!vector_empty(node->statements));
		stmt = vector_A(node->statements, vector_length(node->statements) - 1);
		if (vop(stmt) == VOP_JMP) {
			ast_simplify_jmp(table, node, stmt, continuation, loop_head, loop_break);
		} else if (vop(stmt) == VOP_END && !continuation) {
			// return with no continuation: eliminiate END instruction
			vector_length(node->statements)--;
			mes_statement_free(stmt);
		}
		break;
	case MES_AST_COND:
		ast_block_simplify(table, node->cond.consequent, continuation, loop_head, loop_break);
		ast_block_simplify(table, node->cond.alternative, continuation, loop_head, loop_break);
		break;
	case MES_AST_LOOP:
		ast_block_simplify(table, node->loop.body, node, node, continuation);
		break;
	case MES_AST_PROCEDURE:
	case MES_AST_SUB:
		ast_block_simplify(table, node->proc.body, NULL, NULL, NULL);
		break;
	case MES_AST_MENU_ENTRY:
		ast_block_simplify(table, node->menu.body, NULL, NULL, NULL);
		break;
	case MES_AST_CONTINUE:
	case MES_AST_BREAK:
		break;
	}
}

static void ast_block_simplify(hashtable_t(ast_table) *table, mes_ast_block block,
		struct mes_ast *continuation,
		struct mes_ast *loop_head,
		struct mes_ast *loop_break)
{
	for (unsigned i = 0; i < vector_length(block); i++) {
		struct mes_ast *node = vector_A(block, i);
		struct mes_ast *next = i + 1 < vector_length(block)
			? vector_A(block, i+1) : continuation;
		ast_node_simplify(table, node, next, loop_head, loop_break);
	}
}

static void init_ast_table(hashtable_t(ast_table) *table, mes_ast_block block)
{
	struct mes_ast *node;
	vector_foreach(node, block) {
		if (node->address == MES_ADDRESS_SYNTHETIC)
			continue;
		// add node to table
		int ret;
		hashtable_iter_t k = hashtable_put(ast_table, table, node->address, &ret);
		if (unlikely(ret == HASHTABLE_KEY_PRESENT))
			ERROR("multiple AST nodes with same address");
		hashtable_val(table, k) = node;

		switch (node->type) {
		case MES_AST_STATEMENTS:
			break;
		case MES_AST_COND:
			init_ast_table(table, node->cond.consequent);
			init_ast_table(table, node->cond.alternative);
			break;
		case MES_AST_LOOP:
			init_ast_table(table, node->loop.body);
			break;
		case MES_AST_PROCEDURE:
		case MES_AST_SUB:
			init_ast_table(table, node->proc.body);
			break;
		case MES_AST_MENU_ENTRY:
			init_ast_table(table, node->menu.body);
			break;
		case MES_AST_CONTINUE:
		case MES_AST_BREAK:
			break;
		}
	}
}

static void ast_simplify(mes_ast_block toplevel)
{
	hashtable_t(ast_table) table = hashtable_initializer(ast_table);
	init_ast_table(&table, toplevel);
	ast_block_simplify(&table, toplevel, NULL, NULL, NULL);
	hashtable_destroy(ast_table, &table);
}

// AST Simplify }}}
// Phase 2: AST }}}

static void _mes_block_free(struct mes_block *block, bool free_statements)
{
	if (block->post < 0) {
		// XXX: block is dead code
		free_statements = true;
	}

	struct mes_block *b;
	switch (block->type) {
	case MES_BLOCK_BASIC:
		if (free_statements) {
			mes_statement_list_free(block->basic.statements);
			if (block->basic.end)
				mes_statement_free(block->basic.end);
		}
		break;
	case MES_BLOCK_COMPOUND:
		vector_foreach(b, block->compound.blocks) {
			_mes_block_free(b, free_statements);
			free(b);
		}
		vector_destroy(block->compound.blocks);
		vector_destroy(block->compound.post);
		if (free_statements && block->compound.head)
			mes_statement_free(block->compound.head);
		break;
	}
	vector_destroy(block->pred);
	vector_destroy(block->succ);
	vector_destroy(block->dom_front);
	vector_destroy(block->dom);
}

// check for leaked blocks from the CFG->AST transformation
static void leak_check(struct mes_compound_block *block, int indent)
{
	struct mes_block *b;
	vector_foreach(b, block->post) {
		if (!b->in_ast) {
			sys_warning("LEAK: %d", b->post);
			for (struct mes_block *p = b->parent; p; p = p->parent) {
				sys_warning(" <- %d", p->post);
			}
			sys_warning("\n");
		}
		if (b->type == MES_BLOCK_COMPOUND)
			leak_check(&b->compound, indent + 1);
	}
}

bool mes_decompile(uint8_t *data, size_t data_size, mes_ast_block *out)
{
	vop_init();
	struct mes_block cfg_toplevel = { .type = MES_BLOCK_COMPOUND };
	mes_ast_block ast_toplevel = vector_initializer;

	// phase 0: parse
	mes_statement_list statements = vector_initializer;
	if (!(mes_parse_statements(data, data_size, &statements)))
		return false;

	// phase 1: create/analyze control flow graph
	cfg_create(&cfg_toplevel, statements);
	// phase 2: use CFG to reconstruct the AST
	ast_create(&cfg_toplevel, &ast_toplevel);
	// check for leaked blocks
	leak_check(&cfg_toplevel.compound, 0);
	// simplify the created AST
	ast_simplify(ast_toplevel);

	_mes_block_free(&cfg_toplevel, false);

	*out = ast_toplevel;
	return true;
}

void mes_block_free(struct mes_block *block)
{
	switch (block->type) {
	case MES_BLOCK_BASIC:
		mes_statement_list_free(block->basic.statements);
		if (block->basic.end)
			mes_statement_free(block->basic.end);
		break;
	case MES_BLOCK_COMPOUND:
		mes_block_list_free(block->compound.blocks);
		vector_destroy(block->compound.post);
		if (block->compound.head)
			mes_statement_free(block->compound.head);
		break;
	}
	vector_destroy(block->pred);
	vector_destroy(block->succ);
	vector_destroy(block->dom_front);
	vector_destroy(block->dom);
	free(block);
}

void mes_block_list_free(mes_block_list list)
{
	struct mes_block *block;
	vector_foreach(block, list) {
		mes_block_free(block);
	}
	vector_destroy(list);
}

bool mes_decompile_debug(uint8_t *data, size_t data_size, mes_block_list *out)
{
	vop_init();
	struct mes_block cfg_toplevel = { .type = MES_BLOCK_COMPOUND };

	mes_statement_list statements = vector_initializer;
	if (!(mes_parse_statements(data, data_size, &statements)))
		return false;

	cfg_create(&cfg_toplevel, statements);

	vector_destroy(cfg_toplevel.compound.post);
	vector_destroy(cfg_toplevel.pred);
	vector_destroy(cfg_toplevel.succ);
	vector_destroy(cfg_toplevel.dom_front);
	vector_destroy(cfg_toplevel.dom);

	*out = cfg_toplevel.compound.blocks;
	return true;
}

void mes_ast_free(struct mes_ast *node)
{
	switch (node->type) {
	case MES_AST_STATEMENTS:
		mes_statement_list_free(node->statements);
		break;
	case MES_AST_COND:
		mes_expression_free(node->cond.condition);
		mes_ast_block_free(node->cond.consequent);
		mes_ast_block_free(node->cond.alternative);
		break;
	case MES_AST_LOOP:
		mes_expression_free(node->loop.condition);
		mes_ast_block_free(node->loop.body);
		break;
	case MES_AST_PROCEDURE:
	case MES_AST_SUB:
		mes_expression_free(node->proc.num_expr);
		mes_ast_block_free(node->proc.body);
		break;
	case MES_AST_MENU_ENTRY:
		mes_parameter_list_free(node->menu.params);
		mes_ast_block_free(node->menu.body);
		break;
	case MES_AST_CONTINUE:
	case MES_AST_BREAK:
		break;
	}
	free(node);
}

void mes_ast_block_free(mes_ast_block block)
{
	struct mes_ast *node;
	vector_foreach(node, block) {
		mes_ast_free(node);
	}
	vector_destroy(block);
}
