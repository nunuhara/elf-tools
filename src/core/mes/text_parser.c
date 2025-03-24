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
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "nulib.h"
#include "nulib/hashtable.h"
#include "nulib/string.h"
#include "nulib/vector.h"
#include "nulib/utfsjis.h"
#include "ai5/game.h"

#include "mes.h"

struct mes_encoder {
	struct mes_statement *(*text)(string, bool);
	struct mes_statement *(*line)(void);
	struct mes_statement *(*call)(unsigned);
};

struct mes_statement *ai5_encode_text(string text, bool zenkaku)
{
	return zenkaku ? mes_stmt_txt(text) : mes_stmt_str(text);
}

struct mes_statement *ai5_encode_line(void)
{
	return mes_stmt_line(0);
}

struct mes_statement *ai5_encode_call(unsigned fno)
{
	struct mes_statement *stmt = mes_stmt(MES_STMT_CALL_PROC);
	vector_push(struct mes_parameter, stmt->CALL.params, mes_param_expr(mes_expr_constant(fno)));
	return stmt;
}

static struct mes_encoder ai5_encoder = {
	.text = ai5_encode_text,
	.line = ai5_encode_line,
	.call = ai5_encode_call,
};

struct mes_statement *aiw_encode_text(string text, bool zenkaku)
{
	struct mes_statement *stmt = aiw_mes_stmt(AIW_MES_STMT_TXT);
	if (!zenkaku && string_length(text) % 2 != 0) {
		text = string_grow(text, string_length(text) + 1);
		text[string_length(text) - 1] = '0';
	}
	stmt->TXT.text = text;
	stmt->TXT.terminated = true;
	return stmt;
}

struct mes_statement *aiw_encode_line(void)
{
	return NULL;
}

struct mes_statement *aiw_encode_call(unsigned fno)
{
	struct mes_statement *stmt = stmt = aiw_mes_stmt(AIW_MES_STMT_CALL_PROC);
	vector_push(struct mes_parameter, stmt->CALL.params, mes_param_expr(mes_expr_constant(fno)));
	return stmt;
}

static struct mes_encoder aiw_encoder = {
	.text = aiw_encode_text,
	.line = aiw_encode_line,
	.call = aiw_encode_call,
};

struct mes_text_sub_state {
	unsigned line;
	int columns;
};

static struct mes_encoder *mes_encoder = &ai5_encoder;

#define PARSE_ERROR(state, msg, ...) \
	sys_warning("Parse error: At line %d: " msg "\n", (state)->line + 1, ##__VA_ARGS__)

#define PARSE_WARNING(state, msg, ...) \
	sys_warning("WARNING: At line %d: " msg "\n", (state)->line + 1, ##__VA_ARGS__)

typedef vector_t(char*) line_list;

static bool read_lines(FILE *f, line_list *out)
{
	line_list lines = vector_initializer;

	errno = 0;
	while (true) {
		size_t _n;
		char *line = NULL;
		ssize_t size = getline(&line, &_n, f);
		if (size <= 0) {
			free(line);
			break;
		}

		// remove newline
		if (line[size-1] == '\n') {
			size--;
			line[size] = '\0';
		}

		vector_push(char*, lines, line);
	}

	// handle read error
	if (errno) {
		char *p;
		vector_foreach(p, lines) {
			free(p);
		}
		vector_destroy(lines);
		return false;
	}

	*out = lines;
	return true;
}

static inline bool expect_char(struct mes_text_sub_state *state, char **str, char c)
{
	char *p = *str;
	if (*p != c) {
		PARSE_ERROR(state, "Expected '%c': got '%c'", c, *p);
		return false;
	}
	*str = p + 1;
	return true;
}

static inline bool expect_eol(struct mes_text_sub_state *state, char *str)
{
	if (*str) {
		PARSE_ERROR(state, "Junk at end of header line: \"%s\"", str);
		return false;
	}
	return true;
}

static inline void skip_whitespace(char **str)
{
	char *p = *str;
	while (isspace(*p))
		p++;
	*str = p;
}

static inline bool read_int(struct mes_text_sub_state *state, char **str, int *out)
{
	char *endptr;
	long i = strtol(*str, &endptr, 0);
	if (endptr == *str || (*endptr && !isspace(*endptr))) {
		PARSE_ERROR(state, "Expected integer: \"%s\"", *str);
		return false;
	}
	*out = i;
	*str = endptr;
	return true;
}

static inline bool read_string(struct mes_text_sub_state *state, char **str, string *out)
{
	expect_char(state, str, '"');

	// validate and determine length of string
	char *p = *str;
	while (*p != '"') {
		if (*p == '\0') {
			PARSE_ERROR(state, "Unterminated string");
			return false;
		}
		if (*p == '\\') {
			switch (++(*p)) {
			case 'n':
			case 't':
			case 'r':
			case '"':
			case '$':
				p++;
				break;
			case 'x':
				if (!isxdigit(p[1]) || !isxdigit(p[2])) {
					PARSE_ERROR(state, "Invalid \\x string escape: \"\\x%c%c\"",
							p[1], p[2]);
					return false;
				}
				p += 3;
				break;
			case 'X':
				if (!isxdigit(p[1]) || !isxdigit(p[2]) || !isxdigit(p[3])
						|| !isxdigit(p[4])) {
					PARSE_ERROR(state, "Invalid \\X string escape: \"\\X%c%c%c%c\"",
							p[1], p[2], p[3], p[4]);
					return false;
				}
				p += 5;
				break;
			default:
				PARSE_ERROR(state, "Invalid string escape: \"\\%c\"", *p);
				return false;
			}
		} else {
			p++;
		}
	}

	*out = string_new_len(*str, p - *str);
	*str = p + 1;
	return true;
}

static bool parse_head(struct mes_text_sub_state *state, char *head,
		struct mes_text_substitution *sub)
{
	if (!expect_char(state, &head, '#'))
		return false;

	// '## ...' is a comment
	if (*head == '#')
		return true;

	skip_whitespace(&head);

	// config headers
	if (!strncmp(head, "columns", 7)) {
		head += 7;
		skip_whitespace(&head);
		if (!expect_char(state, &head, '='))
			return false;
		skip_whitespace(&head);
		if (!read_int(state, &head, &state->columns))
			return false;
		if (state->columns < 0) {
			PARSE_ERROR(state, "Invalid columns value: %d", state->columns);
			return false;
		}
		skip_whitespace(&head);
		return expect_eol(state, head);
	}

	// start of substitution header
	if (!read_int(state, &head, &sub->no))
		return false;
	if (sub->no < 0) {
		PARSE_ERROR(state, "Invalid substitution number: %d", sub->no);
		return false;
	}

	skip_whitespace(&head);
	read_string(state, &head, &sub->from);

	skip_whitespace(&head);
	if (!expect_eol(state, head)) {
		string_free(sub->from);
		return false;
	}

	return true;
}

void mes_text_sub_list_free(mes_text_sub_list list)
{
	struct mes_text_substitution *sub;
	vector_foreach_p(sub, list) {
		struct mes_text_line s;
		vector_foreach(s, sub->to) {
			free(s.text);
		}
		vector_destroy(sub->to);
		string_free(sub->from);
	}
	vector_destroy(list);
}

static unsigned strcols(struct mes_text_sub_state *state, const char *str)
{
	unsigned cols = 0;
	while (*str) {
		if (str[0] == '\\') {
			switch (str[1]) {
			case 'n':
			case 't':
			case '$':
			case '\\':
				cols += 1;
				str += 2;
				break;
			case 'x':
				cols += 1;
				str += 4;
				break;
			case 'X':
				cols += 2;
				str += 6;
				break;
			default:
				PARSE_ERROR(state, "invalid escape in string: \"%s\"", str);
			}
		} else {
			cols += utf8_sjis_char_length(str, &str);
		}
	}
	return cols;
}

bool mes_text_parse(FILE *f, mes_text_sub_list *out)
{
	line_list lines;
	if (!read_lines(f, &lines)) {
		WARNING("Read failure: %s", strerror(errno));
		return true;
	}

	struct mes_text_sub_state state = {0};
	mes_text_sub_list subs = vector_initializer;
	while (state.line < vector_length(lines)) {
		// parse substitution headers
		struct mes_text_substitution sub = { .no = -1 };
		while (state.line < vector_length(lines)) {
			// skip empty lines
			while (state.line < vector_length(lines)
					&& vector_A(lines, state.line)[0] == '\0')
				state.line++;
			// parse control line
			if (!parse_head(&state, vector_A(lines, state.line++), &sub))
				goto error;
			// start of substitution
			if (sub.no >= 0)
				break;
		}
		sub.columns = state.columns;

		// read lines until first blank line
		while (state.line < vector_length(lines)) {
			char *str = vector_A(lines, state.line);
			// '## ...' is a comment
			if (str[0] == '#' && str[1] == '#') {
				state.line++;
				continue;
			}
			unsigned cols = strcols(&state, str);
			state.line++;
			if (*str == '\0')
				break;
			struct mes_text_line l = { strdup(str), cols };
			vector_push(struct mes_text_line, sub.to, l);
		}

		vector_push(struct mes_text_substitution, subs, sub);
	}

	char *p;
	vector_foreach(p, lines) {
		free(p);
	}
	vector_destroy(lines);

	*out = subs;
	return true;
error:
	vector_foreach(p, lines) {
		free(p);
	}
	vector_destroy(lines);
	mes_text_sub_list_free(subs);
	return false;
}

struct text_pos {
	string text;
	struct mes_statement *stmt;
	unsigned nr_stmts;
};

typedef vector_t(struct text_pos) text_pos_list;

static void push_text_pos(string text, struct mes_statement *stmt, unsigned nr_stmt,
		void *_data)
{
	text_pos_list *text_locs = _data;
	struct text_pos pos = { string_dup(text), stmt, nr_stmt };
	vector_push(struct text_pos, *text_locs, pos);
}

// Every jump target is stored in a hash table indexed by the original statement address.
// Once the new statement list is prepared, this table is used to update jump addresses.
declare_hashtable_int_type(addr_table, struct mes_statement*);
define_hashtable_int(addr_table, struct mes_statement*);

static void addr_table_add(hashtable_t(addr_table) *table, uint32_t addr,
		struct mes_statement *stmt)
{
	int ret;
	hashtable_iter_t k = hashtable_put(addr_table, table, addr, &ret);
	if (unlikely(ret == HASHTABLE_KEY_PRESENT))
		ERROR("multiple statements with same address");
	hashtable_val(table, k) = stmt;
}

static uint32_t addr_table_get(hashtable_t(addr_table) *table, uint32_t addr)
{
	hashtable_iter_t k = hashtable_get(addr_table, table, addr);
	if (unlikely(k == hashtable_end(table)))
		ERROR("address lookup failed for %08x", (unsigned)addr);
	return hashtable_val(table, k)->address;
}

// update statement address and push to statement list
static void push_stmt(struct mes_statement *stmt, mes_statement_list *mes, uint32_t *mes_addr)
{
	if (!stmt)
		return;
	stmt->address = *mes_addr;
	*mes_addr += mes_statement_size(stmt);
	vector_push(struct mes_statement*, *mes, stmt);
}

static void push_string(const char *text, size_t len, bool zenkaku,
		mes_statement_list *mes, uint32_t *mes_addr)
{
	string str = string_new_len(text, len);
	struct mes_statement *stmt = mes_encoder->text(str, zenkaku);
	push_stmt(stmt, mes, mes_addr);
}

// split text into hankaku/zenkaku parts and push statements to list
bool encode_substitution(struct mes_text_substitution *sub, mes_statement_list *mes,
		uint32_t *mes_addr)
{
	if (vector_length(sub->to) == 0) {
		const char *tmp;
		// TODO: print this only in verbose mode
		//sys_warning("WARNING: no substitution for string %d\n", sub->no);
		push_string(sub->from, string_length(sub->from),
				utf8_sjis_char_length(sub->from, &tmp) == 2,
				mes, mes_addr);
		return false;
	}

	int line_no = 0;
	struct mes_text_line line = vector_A(sub->to, 0);
	const char *p = line.text;
	const char *start = p;
	bool zenkaku = false;
	while (true) {
		// end of line
		if (*p == '\0') {
			if (p > start)
				push_string(start, p - start, zenkaku, mes, mes_addr);
			if (++line_no >= vector_length(sub->to))
				break;
			if (line.columns < sub->columns)
				push_stmt(mes_encoder->line(), mes, mes_addr);
			if (sub->columns && line.columns > sub->columns)
				sys_warning("WARNING: Line # %d exceeds configured columns value\n",
						sub->no);
			line = vector_A(sub->to, line_no);
			p = line.text;
			start = p;
		}
		// embedded procedure call (name function)
		if (p[0] == '$' && p[1] == '(') {
			char *endptr;
			long i = strtol(p+2, &endptr, 0);
			if (*endptr != ')' || i < 0)
				ERROR("Invalid '$' call in string: %s", p);
			if (p > start)
				push_string(start, p - start, zenkaku, mes, mes_addr);
			push_stmt(mes_encoder->call(i), mes, mes_addr);
			p = endptr + 1;
		}
		const char *next;
		bool next_zenkaku;
		if (p[0] == '\\') {
			switch (p[1]) {
			case 'X':
				next_zenkaku = true;
				next = p + 6;
				break;
			case 'x':
				next_zenkaku = false;
				next = p + 4;
				break;
			case 'n':
			case 't':
			case '$':
			case '\\':
				next_zenkaku = false;
				next = p + 2;
				break;
			default:
				ERROR("Invalid escape in string: %s", p);
			}
		} else {
			next_zenkaku = utf8_sjis_char_length(p, &next) == 2;
		}
		if (p > start && zenkaku != next_zenkaku) {
			push_string(start, p - start, zenkaku, mes, mes_addr);
			start = p;
		}
		zenkaku = next_zenkaku;
		p = next;
	}
	return true;
}

static void copy_stmt(mes_statement_list mes, unsigned mes_pos, uint32_t *mes_addr,
		mes_statement_list *mes_out, hashtable_t(addr_table) *table)
{
	assert(mes_pos < vector_length(mes));
	struct mes_statement *stmt = vector_A(mes, mes_pos);
	// add old address to table if the statement is a jump target
	if (stmt->is_jump_target)
		addr_table_add(table, stmt->address, stmt);
	push_stmt(stmt, mes_out, mes_addr);
}

static void ai5_update_addresses(mes_statement_list mes_out, hashtable_t(addr_table) *table)
{
	struct mes_statement *stmt;
	vector_foreach(stmt, mes_out) {
		uint32_t *addr;
		switch (stmt->op) {
		case MES_STMT_JZ: addr = &stmt->JZ.addr; break;
		case MES_STMT_JMP: addr = &stmt->JMP.addr; break;
		case MES_STMT_DEF_MENU: addr = &stmt->DEF_MENU.skip_addr; break;
		case MES_STMT_DEF_PROC: addr = &stmt->DEF_PROC.skip_addr; break;
		default: continue;
		}
		// replace old address with new address
		*addr = addr_table_get(table, *addr);
	}
}

static void aiw_update_addresses(mes_statement_list mes_out, hashtable_t(addr_table) *table)
{
	struct mes_statement *stmt;
	vector_foreach(stmt, mes_out) {
		uint32_t *addr;
		switch (stmt->aiw_op) {
		case AIW_MES_STMT_JZ: addr = &stmt->JZ.addr; break;
		case AIW_MES_STMT_JMP: addr = &stmt->JMP.addr; break;
		case AIW_MES_STMT_DEF_PROC: addr = &stmt->DEF_PROC.skip_addr; break;
		default: continue;
		}
		// replace old address with new address
		*addr = addr_table_get(table, *addr);
	}
}

mes_statement_list mes_substitute_text(mes_statement_list mes, mes_text_sub_list subs_in)
{
	if (game_is_aiwin())
		mes_encoder = &aiw_encoder;
	else
		mes_encoder = &ai5_encoder;

	// create list of text positions
	text_pos_list text_locs = vector_initializer;
	mes_statement_list_foreach_text(mes, -1, push_text_pos, NULL, &text_locs);

	// create corresponding list of text substitutions
	mes_text_sub_list subs = vector_initializer;
	vector_resize(struct mes_text_substitution, subs, vector_length(text_locs));
	memset(subs.a, 0, subs.n * sizeof(struct mes_text_substitution));
	struct mes_text_substitution *sub;
	vector_foreach_p(sub, subs_in) {
		if (sub->no < 0 || sub->no >= vector_length(subs))
			ERROR("Invalid string number in substitution: %d", sub->no);
		assert(sub->no >= 0 && sub->no <= vector_length(subs));
		vector_set(struct mes_text_substitution, subs, sub->no, *sub);
	}

	// hash table associating (original) address to statement object for each jump target
	hashtable_t(addr_table) table = hashtable_initializer(addr_table);

	// create new statement list with text substituted
	unsigned mes_pos = 0;
	uint32_t mes_addr = 0;
	mes_statement_list mes_out = vector_initializer;
	unsigned missing_subs = 0;
	for (int i = 0; i < vector_length(subs); i++) {
		struct mes_text_substitution *sub = &vector_A(subs, i);
		struct text_pos *loc = &vector_A(text_locs, i);
		if (!sub->from)
			continue;
		// copy statements up until the current substitution
		for (; vector_A(mes, mes_pos) != loc->stmt; mes_pos++) {
			copy_stmt(mes, mes_pos, &mes_addr, &mes_out, &table);
		}
		// encode substitution text as statement(s) and push to new list
		size_t n = vector_length(mes_out);
		if (!encode_substitution(sub, &mes_out, &mes_addr))
			missing_subs++;
		if (loc->stmt->is_jump_target) {
			addr_table_add(&table, vector_A(mes, mes_pos)->address, vector_A(mes_out, n));
		}

		// skip original statements
		for (int j = mes_pos; j < mes_pos + loc->nr_stmts; j++) {
			mes_statement_free(vector_A(mes, j));
		}
		mes_pos += loc->nr_stmts;
	}
	// copy remaining statements at end of file
	for (; mes_pos < vector_length(mes); mes_pos++) {
		copy_stmt(mes, mes_pos, &mes_addr, &mes_out, &table);
	}

	// update jump target addresses
	if (game_is_aiwin())
		aiw_update_addresses(mes_out, &table);
	else
		ai5_update_addresses(mes_out, &table);

	struct text_pos *loc;
	vector_foreach_p(loc, text_locs) {
		string_free(loc->text);
	}
	vector_destroy(text_locs);
	vector_destroy(subs);
	vector_destroy(mes);
	hashtable_destroy(addr_table, &table);
	if (missing_subs)
		sys_warning("WARNING: %u lines without substitutions.\n", missing_subs);
	return mes_out;
}
