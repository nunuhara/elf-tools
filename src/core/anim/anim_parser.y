%define api.prefix {anim_script_}
%define parse.error detailed
%define parse.lac full

%{

#include <string.h>
#include <errno.h>

#include "nulib.h"
#include "nulib/file.h"
#include "nulib/vector.h"
#include "anim_parser.tab.h"

int anim_script_lex();
extern FILE *anim_script_in;
extern int anim_script_lineno;

#define PARSE_ERROR(fmt, ...) \
	sys_error("ERROR: At line %d: " fmt "\n", anim_script_lineno, ##__VA_ARGS__)

static struct anim program;

static FILE *open_file(const char *file)
{
	if (!strcmp(file, "-"))
		return stdin;
	return file_open_utf8(file, "rb");
}

struct anim *anim_parse_script(const char *path)
{
	struct anim *anim = NULL;

	anim_script_in = open_file(path);
	if (!anim_script_in) {
		WARNING("Failed to open input file \"%s\": %s", path, strerror(errno));
		return NULL;
	}

	program = (struct anim){0};
	if (anim_script_parse()) {
		WARNING("Failed to parse file: %s", path);
		goto end;
	}

	anim = xmalloc(sizeof(struct anim));
	*anim = program;
end:
	if (anim_script_in != stdin)
		fclose(anim_script_in);
	anim_script_in = NULL;
	return anim;
}

void anim_script_error(const char *s)
{
	PARSE_ERROR("%s", s);
}

static void push_stream(anim_stream stream)
{
	for (int i = 0; i < ANIM_MAX_STREAMS; i++) {
		if (vector_length(program.streams[i]) == 0) {
			program.streams[i] = stream;
			return;
		}
	}
	ERROR("Too many streams");
}

static bool anim_target_eq(struct anim_target *a, struct anim_target *b)
{
	return a->i == b->i && a->x == b->x && a->y == b->y;
}

static bool anim_size_eq(struct anim_size *a, struct anim_size *b)
{
	return a->w == b->w && a->h == b->h;
}

static bool anim_color_eq(struct anim_color *a, struct anim_color *b)
{
	return a->r == b->r && a->g == b->g && a->b == b->b;
}

static bool draw_call_eq(struct anim_draw_call *a, struct anim_draw_call *b)
{
	if (a->op != b->op)
		return false;
	switch (a->op) {
	case ANIM_DRAW_OP_FILL:
		return anim_target_eq(&a->fill.dst, &b->fill.dst)
				&& anim_size_eq(&a->fill.dim, &b->fill.dim);
	case ANIM_DRAW_OP_COPY:
	case ANIM_DRAW_OP_COPY_MASKED:
	case ANIM_DRAW_OP_SWAP:
	case ANIM_DRAW_OP_0x60_COPY_MASKED:
	case ANIM_DRAW_OP_0x62:
	case ANIM_DRAW_OP_0x63_COPY_MASKED_WITH_XOFFSET:
	case ANIM_DRAW_OP_0x66:
		return anim_target_eq(&a->copy.src, &b->copy.src)
				&& anim_target_eq(&a->copy.dst, &b->copy.dst)
				&& anim_size_eq(&a->copy.dim, &b->copy.dim);
	case ANIM_DRAW_OP_COMPOSE_WITH_OFFSET:
	case ANIM_DRAW_OP_COMPOSE:
	case ANIM_DRAW_OP_0x61_COMPOSE:
	case ANIM_DRAW_OP_0x64_COMPOSE_MASKED:
	case ANIM_DRAW_OP_0x65_COMPOSE:
		return anim_target_eq(&a->compose.fg, &b->compose.fg)
				&& anim_target_eq(&a->compose.bg, &b->compose.bg)
				&& anim_target_eq(&a->compose.dst, &b->compose.dst)
				&& anim_size_eq(&a->compose.dim, &b->compose.dim);
	case ANIM_DRAW_OP_SET_COLOR:
		return a->set_color.i == b->set_color.i
				&& anim_color_eq(&a->set_color.color, &b->set_color.color);
	case ANIM_DRAW_OP_SET_PALETTE:
		for (int i = 0; i < 16; i++) {
			if (!anim_color_eq(&a->set_palette.colors[i], &b->set_palette.colors[i]))
				return false;
		}
		return true;
	}
	return false;
}

static unsigned push_draw_call(struct anim_draw_call *call)
{
	for (int i = 0; i < vector_length(program.draw_calls); i++) {
		if (draw_call_eq(call, &vector_A(program.draw_calls, i)))
			return i;
	}
	if (vector_length(program.draw_calls) + 20 >= 256)
		PARSE_ERROR("Too many draw calls");
	unsigned no = vector_length(program.draw_calls);
	vector_push(struct anim_draw_call, program.draw_calls, *call);
	return no;
}

static anim_stream push_instruction(anim_stream stream, struct anim_instruction instruction)
{
	vector_push(struct anim_instruction, stream, instruction);
	return stream;
}

static struct anim_instruction make_instruction(enum anim_opcode op, uint8_t arg)
{
	return (struct anim_instruction) { .op = op, .arg = arg };
}

static struct anim_instruction make_draw_call(struct anim_draw_call *call)
{
	uint8_t no = push_draw_call(call);
	return make_instruction(ANIM_OP_DRAW, no);
}

static struct anim_instruction make_fill(struct anim_target target, struct anim_size size)
{
	struct anim_draw_call call = {
		.op = ANIM_DRAW_OP_FILL,
		.fill = {
			.dst = target,
			.dim = size
		}
	};
	return make_draw_call(&call);
}

static struct anim_instruction make_copy(enum anim_draw_opcode op, struct anim_target src,
			struct anim_target dst, struct anim_size size)
{
	struct anim_draw_call call = {
		.op = op,
		.copy = {
			.src = src,
			.dst = dst,
			.dim = size
		}
	};
	return make_draw_call(&call);
}

static struct anim_instruction make_compose(struct anim_target bg, struct anim_target fg,
			struct anim_target dst, struct anim_size size)
{
	struct anim_draw_call call = {
		.op = ANIM_DRAW_OP_COMPOSE,
		.compose = {
			.fg = fg,
			.bg = bg,
			.dst = dst,
			.dim = size
		}
	};
	return make_draw_call(&call);
}

static struct anim_instruction make_set_color(uint8_t i, struct anim_color color)
{
	if (i != (color.b & 0xf)) {
		WARNING("blue value %u will be clobbered by index %u", color.b, i);
		color.b = i | (i << 4);
	}
	struct anim_draw_call call = {
		.op = ANIM_DRAW_OP_SET_COLOR,
		.set_color = {
			.i = i,
			.color = color
		}
	};
	return make_draw_call(&call);
}

static struct anim_instruction make_set_palette(struct anim_color c0, struct anim_color c1,
			struct anim_color c2, struct anim_color c3, struct anim_color c4,
			struct anim_color c5, struct anim_color c6, struct anim_color c7,
			struct anim_color c8, struct anim_color c9, struct anim_color c10,
			struct anim_color c11, struct anim_color c12, struct anim_color c13,
			struct anim_color c14, struct anim_color c15)
{
	struct anim_draw_call call = {
		.op = ANIM_DRAW_OP_SET_PALETTE,
		.set_palette = {
			.colors = {
				[0] = c0, [1] = c1, [2] = c2, [3] = c3, [4] = c4, [5] = c5,
				[6] = c6, [7] = c7, [8] = c8, [9] = c9, [10] = c10,
				[11] = c11, [12] = c12, [13] = c13, [14] = c14, [15] = c15
			}
		}
	};
	return make_draw_call(&call);
}

static long parse_int(string str)
{
	char *endptr;
	long i = strtol(str, &endptr, 0);
	if (*endptr != '\0')
		PARSE_ERROR("invalid integer constant: %s", str);
	return i;
}

static unsigned parse_uX(string str, unsigned limit)
{
	long i = parse_int(str);
	if (i < 0 || i > limit)
		PARSE_ERROR("value out of range: %s (valid range is [0, %u])", str, limit);
	string_free(str);
	return i;
}

static uint8_t parse_u1(string str)
{
	return parse_uX(str, 1);
}

static uint8_t parse_u4(string str)
{
	return parse_uX(str, 15);
}

static uint8_t parse_u8(string str)
{
	return parse_uX(str, 255);
}

static uint16_t parse_u16(string str)
{
	return parse_uX(str, 65535);
}

static uint16_t parse_x_dim(string str)
{
	uint16_t n = parse_uX(str, 2040);
	if (n & 7) {
		WARNING("X dimension will be truncated to multiple of 8: %s", str);
	}
	return n;
}

static struct anim_target make_target(string i, string x, string y)
{
	return (struct anim_target) {
		.i = parse_u1(i),
		.x = parse_x_dim(x),
		.y = parse_u16(y)
	};
}

static struct anim_size make_size(string w, string h)
{
	return (struct anim_size) {
		.w = parse_x_dim(w),
		.h = parse_u16(h)
	};
}

static struct anim_color make_color(string r, string g, string b)
{
	struct anim_color c = {
		.r = parse_u4(r),
		.g = parse_u4(g),
		.b = parse_u4(b)
	};
	c.r |= c.r << 4;
	c.g |= c.g << 4;
	c.b |= c.b << 4;
	return c;
}

%}

%union {
	int token;
	string string;
	anim_stream stream;
	struct anim_instruction instruction;
	enum anim_draw_opcode draw_op;
	struct anim_target target;
	struct anim_size size;
	struct anim_color color;
}

%code requires {
	#include "nulib/string.h"
	#include "ai5/anim.h"
}

%token	<string>	I_CONSTANT
%token	<token>		ARROW STREAM NOOP CHECK_STOP STALL RESET HALT LOOP_START LOOP_END
%token	<token>		LOOP2_START LOOP2_END COPY COPY_MASKED SWAP SET_COLOR COMPOSE FILL
%token	<token>		SET_PALETTE INVALID_TOKEN

%type	<stream>	stream instructions
%type	<instruction>	instruction
%type	<draw_op>	copy_fun
%type	<target>	target
%type	<size>		size
%type	<color>		color

%start streams

%%

streams
	: stream { push_stream($1); }
	| streams stream { push_stream($2); }
	;

stream
	: STREAM ':' instructions { $$ = $3; }
	;

instructions
	: instruction { $$ = push_instruction((anim_stream)vector_initializer, $1); }
	| instructions instruction { $$ = push_instruction($1, $2); }
	;

instruction
	: NOOP ';'
	{ $$ = make_instruction(ANIM_OP_NOOP, 0); }
	| CHECK_STOP ';'
	{ $$ = make_instruction(ANIM_OP_CHECK_STOP, 0); }
	| STALL I_CONSTANT ';'
	{ $$ = make_instruction(ANIM_OP_STALL, parse_u8($2)); }
	| RESET ';'
	{ $$ = make_instruction(ANIM_OP_RESET, 0); }
	| HALT ';'
	{ $$ = make_instruction(ANIM_OP_HALT, 0); }
	| LOOP_START I_CONSTANT ';'
	{ $$ = make_instruction(ANIM_OP_LOOP_START, parse_u8($2)); }
	| LOOP_END ';'
	{ $$ = make_instruction(ANIM_OP_LOOP_END, 0); }
	| LOOP2_START I_CONSTANT ';'
	{ $$ = make_instruction(ANIM_OP_LOOP2_START, parse_u8($2)); }
	| LOOP2_END ';'
	{ $$ = make_instruction(ANIM_OP_LOOP2_END, 0); }
	| FILL target '@' size ';'
	{ $$ = make_fill($2, $4); }
	| copy_fun target ARROW target '@' size ';'
	{ $$ = make_copy($1, $2, $4, $6); }
	| COMPOSE target '+' target ARROW target '@' size
	{ $$ = make_compose($2, $4, $6, $8); }
	| SET_COLOR I_CONSTANT ARROW color ';'
	{ $$ = make_set_color(parse_u4($2), $4); }
	| SET_PALETTE color color color color color color color color color color color color color color color color ';'
	{ $$ = make_set_palette($2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17); }
	;

copy_fun
	: COPY { $$ = ANIM_DRAW_OP_COPY; }
	| COPY_MASKED { $$ = ANIM_DRAW_OP_COPY_MASKED; }
	| SWAP { $$ = ANIM_DRAW_OP_SWAP; }
	;

target
	: I_CONSTANT '(' I_CONSTANT ',' I_CONSTANT ')' { $$ = make_target($1, $3, $5); }
	;

size
	: '(' I_CONSTANT ',' I_CONSTANT ')' { $$ = make_size($2, $4); }
	;

color
	: '(' I_CONSTANT ',' I_CONSTANT ',' I_CONSTANT ')' { $$ = make_color($2, $4, $6); }
	;
