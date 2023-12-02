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

#include <stdlib.h>

#include "nulib.h"
#include "nulib/buffer.h"
#include "ai5/s4.h"

static void pack_fill_call(struct buffer *out, struct s4_fill_args *fill)
{
	buffer_write_u8(out, S4_DRAW_OP_FILL | (fill->dst.i << 1));
	buffer_write_u8(out, fill->dst.x / 8);
	buffer_write_u16(out, fill->dst.y);
	buffer_write_u8(out, (fill->dst.x + fill->dim.w) / 8 - 1);
	buffer_write_u16(out, (fill->dst.y + fill->dim.h) - 1);
}

static void pack_copy_call(struct buffer *out, enum s4_draw_opcode op,
		struct s4_copy_args *copy)
{
	buffer_write_u8(out, op | copy->dst.i | (copy->src.i << 1));
	buffer_write_u8(out, copy->src.x / 8);
	buffer_write_u16(out, copy->src.y);
	buffer_write_u8(out, (copy->src.x + copy->dim.w) / 8 - 1);
	buffer_write_u16(out, (copy->src.y + copy->dim.h) - 1);
	buffer_write_u8(out, copy->dst.x / 8);
	buffer_write_u16(out, copy->dst.y);
}

static void pack_compose_call(struct buffer *out, struct s4_compose_args *call)
{
	buffer_write_u8(out, S4_DRAW_OP_COMPOSE | call->bg.i | (call->fg.i << 1) | (call->dst.i << 2));
	buffer_write_u8(out, call->fg.x / 8);
	buffer_write_u16(out, call->fg.y);
	buffer_write_u8(out, (call->fg.x + call->dim.w) / 8 - 1);
	buffer_write_u16(out, (call->fg.y + call->dim.h) - 1);
	buffer_write_u8(out, call->bg.x / 8);
	buffer_write_u16(out, call->bg.y);
	buffer_write_u8(out, call->dst.x / 8);
	buffer_write_u16(out, call->dst.y);
}

static void pack_color(struct buffer *out, struct s4_color *color)
{
	buffer_write_u8(out, color->b & 0x0f);
	buffer_write_u8(out, (color->r & 0xf0) | (color->g & 0x0f));
}

static void pack_set_color_call(struct buffer *out, struct s4_set_color_args *call)
{
	buffer_write_u8(out, S4_DRAW_OP_SET_COLOR);
	pack_color(out, &call->color);
}

static void pack_set_palette_call(struct buffer *out, struct s4_set_palette_args *call)
{
	buffer_write_u8(out, S4_DRAW_OP_SET_PALETTE);
	for (int i = 0; i < 16; i++) {
		pack_color(out, &call->colors[i]);
	}
}

static void pack_draw_call(struct buffer *out, struct s4_draw_call *call)
{
	size_t start = out->index;
	switch (call->op) {
	case S4_DRAW_OP_FILL:
		pack_fill_call(out, &call->fill);
		break;
	case S4_DRAW_OP_COPY:
	case S4_DRAW_OP_COPY_MASKED:
	case S4_DRAW_OP_SWAP:
		pack_copy_call(out, call->op, &call->copy);
		break;
	case S4_DRAW_OP_COMPOSE:
		pack_compose_call(out, &call->compose);
		break;
	case S4_DRAW_OP_SET_COLOR:
		pack_set_color_call(out, &call->set_color);
		break;
	case S4_DRAW_OP_SET_PALETTE:
		pack_set_palette_call(out, &call->set_palette);
		break;
	}

	int rem = (start + 33) - out->index;
	for (int i = 0; i < rem; i++) {
		buffer_write_u8(out, 0);
	}
}

static void pack_instruction(struct buffer *out, struct s4_instruction *instr)
{
	if (instr->op == S4_OP_DRAW) {
		buffer_write_u8(out, instr->arg + 20);
		return;
	}

	buffer_write_u8(out, instr->op);
	switch (instr->op) {
	case S4_OP_STALL:
	case S4_OP_LOOP_START:
	case S4_OP_LOOP2_START:
		buffer_write_u8(out, instr->arg);
		break;
	default:
		break;
	}
}

static void pack_stream(struct buffer *out, s4_stream stream)
{
	struct s4_instruction *p;
	vector_foreach_p(p, stream) {
		pack_instruction(out, p);
	}
	buffer_write_u8(out, 0xff);
}

uint8_t *s4_pack(struct s4 *in, size_t *size_out)
{
	struct buffer out;
	buffer_init(&out, NULL, 0);

	buffer_write_u8(&out, S4_MAX_STREAMS);
	buffer_skip(&out, S4_MAX_STREAMS * 2);

	struct s4_draw_call *call;
	vector_foreach_p(call, in->draw_calls) {
		pack_draw_call(&out, call);
	}

	uint16_t stream_addr[S4_MAX_STREAMS];
	for (int i = 0; i < S4_MAX_STREAMS; i++) {
		stream_addr[i] = out.index;
		pack_stream(&out, in->streams[i]);
	}
	*size_out = out.index;

	buffer_seek(&out, 1);
	for (int i = 0; i < S4_MAX_STREAMS; i++) {
		buffer_write_u16(&out, stream_addr[i]);
	}

	return out.buf;
}
