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
#include "ai5/anim.h"

static enum anim_s4_draw_opcode anim_to_s4_draw_opcode(enum anim_draw_opcode op)
{
	switch (op) {
	case ANIM_DRAW_OP_COPY: return ANIM_S4_DRAW_OP_COPY;
	case ANIM_DRAW_OP_COPY_MASKED: return ANIM_S4_DRAW_OP_COPY_MASKED;
	case ANIM_DRAW_OP_SWAP: return ANIM_S4_DRAW_OP_SWAP;
	case ANIM_DRAW_OP_SET_COLOR: return ANIM_S4_DRAW_OP_SET_COLOR;
	case ANIM_DRAW_OP_COMPOSE: return ANIM_S4_DRAW_OP_COMPOSE;
	case ANIM_DRAW_OP_FILL: return ANIM_S4_DRAW_OP_FILL;
	case ANIM_DRAW_OP_SET_PALETTE: return ANIM_S4_DRAW_OP_SET_PALETTE;
	case ANIM_DRAW_OP_COMPOSE_WITH_OFFSET:
	case ANIM_DRAW_OP_0x60_COPY_MASKED:
	case ANIM_DRAW_OP_0x61_COMPOSE:
	case ANIM_DRAW_OP_0x62:
	case ANIM_DRAW_OP_0x63_COPY_MASKED_WITH_XOFFSET:
	case ANIM_DRAW_OP_0x64_COMPOSE_MASKED:
	case ANIM_DRAW_OP_0x65_COMPOSE:
	case ANIM_DRAW_OP_0x66:
		break;
	}
	ERROR("Invalid draw call opcode: %u", op);
}

static void pack_s4_fill_call(struct buffer *out, struct anim_fill_args *fill)
{
	buffer_write_u8(out, ANIM_S4_DRAW_OP_FILL | (fill->dst.i << 1));
	buffer_write_u8(out, fill->dst.x / 8);
	buffer_write_u16(out, fill->dst.y);
	buffer_write_u8(out, (fill->dst.x + fill->dim.w) / 8 - 1);
	buffer_write_u16(out, (fill->dst.y + fill->dim.h) - 1);
}

static void pack_s4_copy_call(struct buffer *out, enum anim_draw_opcode op,
		struct anim_copy_args *copy)
{
	enum anim_s4_draw_opcode s4_op = anim_to_s4_draw_opcode(op);
	buffer_write_u8(out, s4_op | copy->dst.i | (copy->src.i << 1));
	buffer_write_u8(out, copy->src.x / 8);
	buffer_write_u16(out, copy->src.y);
	buffer_write_u8(out, (copy->src.x + copy->dim.w) / 8 - 1);
	buffer_write_u16(out, (copy->src.y + copy->dim.h) - 1);
	buffer_write_u8(out, copy->dst.x / 8);
	buffer_write_u16(out, copy->dst.y);
}

static void pack_s4_compose_call(struct buffer *out, struct anim_compose_args *call)
{
	buffer_write_u8(out, ANIM_S4_DRAW_OP_COMPOSE | call->bg.i | (call->fg.i << 1) | (call->dst.i << 2));
	buffer_write_u8(out, call->fg.x / 8);
	buffer_write_u16(out, call->fg.y);
	buffer_write_u8(out, (call->fg.x + call->dim.w) / 8 - 1);
	buffer_write_u16(out, (call->fg.y + call->dim.h) - 1);
	buffer_write_u8(out, call->bg.x / 8);
	buffer_write_u16(out, call->bg.y);
	buffer_write_u8(out, call->dst.x / 8);
	buffer_write_u16(out, call->dst.y);
}

static void pack_s4_color(struct buffer *out, struct anim_color *color)
{
	buffer_write_u8(out, color->b & 0x0f);
	buffer_write_u8(out, (color->r & 0xf0) | (color->g & 0x0f));
}

static void pack_s4_set_color_call(struct buffer *out, struct anim_set_color_args *call)
{
	buffer_write_u8(out, ANIM_S4_DRAW_OP_SET_COLOR);
	pack_s4_color(out, &call->color);
}

static void pack_s4_set_palette_call(struct buffer *out, struct anim_set_palette_args *call)
{
	buffer_write_u8(out, ANIM_S4_DRAW_OP_SET_PALETTE);
	for (int i = 0; i < 16; i++) {
		pack_s4_color(out, &call->colors[i]);
	}
}

static void pack_s4_draw_call(struct buffer *out, struct anim_draw_call *call)
{
	size_t start = out->index;
	switch (call->op) {
	case ANIM_DRAW_OP_FILL:
		pack_s4_fill_call(out, &call->fill);
		break;
	case ANIM_DRAW_OP_COPY:
	case ANIM_DRAW_OP_COPY_MASKED:
	case ANIM_DRAW_OP_SWAP:
		pack_s4_copy_call(out, call->op, &call->copy);
		break;
	case ANIM_DRAW_OP_COMPOSE:
		pack_s4_compose_call(out, &call->compose);
		break;
	case ANIM_DRAW_OP_SET_COLOR:
		pack_s4_set_color_call(out, &call->set_color);
		break;
	case ANIM_DRAW_OP_SET_PALETTE:
		pack_s4_set_palette_call(out, &call->set_palette);
		break;
	case ANIM_DRAW_OP_COMPOSE_WITH_OFFSET:
	case ANIM_DRAW_OP_0x60_COPY_MASKED:
	case ANIM_DRAW_OP_0x61_COMPOSE:
	case ANIM_DRAW_OP_0x62:
	case ANIM_DRAW_OP_0x63_COPY_MASKED_WITH_XOFFSET:
	case ANIM_DRAW_OP_0x64_COMPOSE_MASKED:
	case ANIM_DRAW_OP_0x65_COMPOSE:
	case ANIM_DRAW_OP_0x66:
		break;
	}

	int rem = (start + anim_draw_call_size) - out->index;
	for (int i = 0; i < rem; i++) {
		buffer_write_u8(out, 0);
	}
}

static void pack_a_copy_args(struct buffer *out, struct anim_copy_args *copy)
{
	buffer_write_u16(out, copy->src.x);
	buffer_write_u16(out, copy->src.y);
	buffer_write_u16(out, copy->dim.w);
	buffer_write_u16(out, copy->dim.h);
	buffer_write_u16(out, copy->dst.x);
	buffer_write_u16(out, copy->dst.y);
}

static void pack_a_compose_args(struct buffer *out, struct anim_compose_args *call)
{
	buffer_write_u16(out, call->fg.x);
	buffer_write_u16(out, call->fg.y);
	buffer_write_u16(out, call->dim.w);
	buffer_write_u16(out, call->dim.h);
	buffer_write_u16(out, call->bg.x);
	buffer_write_u16(out, call->bg.y);
	if (call->bg.x != call->dst.x || call->bg.y != call->dst.y)
		WARNING("Compose call has different coordinate for BG and DST areas");
}

static uint8_t a8_draw_call_opcode(struct anim_draw_call *call)
{
	switch (call->op) {
	case ANIM_DRAW_OP_COPY: return 0x10 | call->copy.dst.i | (call->copy.src.i << 1);
	case ANIM_DRAW_OP_COPY_MASKED: return 0x20 | call->copy.dst.i | (call->copy.src.i << 1);
	case ANIM_DRAW_OP_SWAP: return 0x30 | call->copy.dst.i | (call->copy.src.i << 1);
	case ANIM_DRAW_OP_FILL: return 0x60 | (call->copy.src.i << 1);
	default:
		ERROR("Invalid draw call: %u", call->op);
	}
}

static void pack_a8_draw_call(struct buffer *out, struct anim_draw_call *call)
{
	size_t start = out->index;
	buffer_write_u8(out, a8_draw_call_opcode(call));
	switch (call->op) {
	case ANIM_DRAW_OP_COPY:
	case ANIM_DRAW_OP_COPY_MASKED:
	case ANIM_DRAW_OP_SWAP:
		pack_a_copy_args(out, &call->copy);
		break;
	case ANIM_DRAW_OP_FILL:
		buffer_write_u16(out, call->fill.dst.x);
		buffer_write_u16(out, call->fill.dst.y);
		buffer_write_u16(out, call->fill.dim.w);
		buffer_write_u16(out, call->fill.dim.h);
		break;
	default:
		ERROR("Invalid draw call: %u", call->op);
	}

	int rem = (start + anim_draw_call_size) - out->index;
	for (int i = 0; i < rem; i++) {
		buffer_write_u8(out, 0);
	}
}

static uint8_t a_draw_call_opcode(struct anim_draw_call *call)
{
	switch (call->op) {
	case ANIM_DRAW_OP_COPY:
		return ANIM_A_DRAW_OP_COPY | call->copy.dst.i | (call->copy.src.i << 1);
	case ANIM_DRAW_OP_COPY_MASKED:
		return ANIM_A_DRAW_OP_COPY_MASKED | call->copy.dst.i | (call->copy.src.i << 1);
	case ANIM_DRAW_OP_SWAP:
		return ANIM_A_DRAW_OP_SWAP | call->copy.dst.i | (call->copy.src.i << 1);
	case ANIM_DRAW_OP_COMPOSE:
		return ANIM_A_DRAW_OP_COMPOSE | call->compose.bg.i | (call->compose.fg.i << 1) | (call->compose.dst.i << 2);
	case ANIM_DRAW_OP_COMPOSE_WITH_OFFSET:
		return 0x30 | call->compose.bg.i | (call->compose.fg.i << 1) | (call->compose.dst.i << 2);
	case ANIM_DRAW_OP_0x60_COPY_MASKED:
		return 0x60;
	case ANIM_DRAW_OP_0x61_COMPOSE:
		return 0x61;
	case ANIM_DRAW_OP_0x62:
		return 0x62;
	case ANIM_DRAW_OP_0x63_COPY_MASKED_WITH_XOFFSET:
		return 0x63;
	case ANIM_DRAW_OP_0x64_COMPOSE_MASKED:
		return 0x64;
	case ANIM_DRAW_OP_0x65_COMPOSE:
		return 0x65;
	case ANIM_DRAW_OP_0x66:
		return 0x66;
	default:
		ERROR("Invalid draw call: %u", call->op);
	}
}

static void pack_a_draw_call(struct buffer *out, struct anim_draw_call *call)
{
	struct anim_copy_args copy_args;
	size_t start = out->index;
	buffer_write_u8(out, a_draw_call_opcode(call));
	switch (call->op) {
	case ANIM_DRAW_OP_COPY:
	case ANIM_DRAW_OP_COPY_MASKED:
	case ANIM_DRAW_OP_SWAP:
	case ANIM_DRAW_OP_0x62:
	case ANIM_DRAW_OP_0x66:
		pack_a_copy_args(out, &call->copy);
		break;
	case ANIM_DRAW_OP_COMPOSE:
	case ANIM_DRAW_OP_COMPOSE_WITH_OFFSET:
	case ANIM_DRAW_OP_0x64_COMPOSE_MASKED:
	case ANIM_DRAW_OP_0x65_COMPOSE:
		pack_a_compose_args(out, &call->compose);
		break;
	case ANIM_DRAW_OP_0x60_COPY_MASKED:
		copy_args = call->copy;
		copy_args.dst.y += 8;
		pack_a_copy_args(out, &copy_args);
		break;
	case ANIM_DRAW_OP_0x61_COMPOSE:
		buffer_write_u16(out, call->compose.fg.x);
		buffer_write_u16(out, call->compose.fg.y);
		buffer_write_u16(out, call->compose.dim.w);
		buffer_write_u16(out, call->compose.dim.h);
		buffer_write_u16(out, call->compose.dst.x);
		buffer_write_u16(out, call->compose.dst.y);
		break;
	case ANIM_DRAW_OP_0x63_COPY_MASKED_WITH_XOFFSET:
		copy_args = call->copy;
		copy_args.dst.y -= 20;
		pack_a_copy_args(out, &copy_args);
		break;
	default:
		ERROR("Invalid draw call: %u", call->op);
	}

	int rem = (start + anim_draw_call_size) - out->index;
	for (int i = 0; i < rem; i++) {
		buffer_write_u8(out, 0);
	}
}

static void pack_s4_instruction(struct buffer *out, struct anim_instruction *instr)
{
	if (instr->op == ANIM_OP_DRAW) {
		buffer_write_u8(out, instr->arg + 20);
		return;
	}

	buffer_write_u8(out, instr->op);
	switch (instr->op) {
	case ANIM_OP_STALL:
	case ANIM_OP_LOOP_START:
	case ANIM_OP_LOOP2_START:
		buffer_write_u8(out, instr->arg);
		break;
	case ANIM_OP_LOAD_PALETTE:
		buffer_write_u16(out, instr->arg);
		break;
	default:
		break;
	}
}

static void pack_a_instruction(struct buffer *out, struct anim_instruction *instr)
{
	if (instr->op == ANIM_OP_DRAW) {
		buffer_write_u16(out, instr->arg + 20);
		return;
	}

	buffer_write_u16(out, instr->op);
	switch (instr->op) {
	case ANIM_OP_STALL:
	case ANIM_OP_LOOP_START:
	case ANIM_OP_LOOP2_START:
		buffer_write_u16(out, instr->arg);
		break;
	case ANIM_OP_LOAD_PALETTE:
		buffer_write_u16(out, instr->arg);
		break;
	default:
		break;
	}
}

static void pack_s4_stream(struct buffer *out, anim_stream stream)
{
	struct anim_instruction *p;
	vector_foreach_p(p, stream) {
		pack_s4_instruction(out, p);
	}
	buffer_write_u8(out, 0xff);
}

static int get_palette_no(anim_palette_list palettes, uint16_t id)
{
	int i = 0;
	struct anim_palette *p;
	vector_foreach_p(p, palettes) {
		if (p->addr == id)
			return i;
		i++;
	}
	return -1;
}

static void pack_a8_stream(struct buffer *out, anim_stream stream, anim_palette_list palettes,
		uint16_t *pal_addr)
{
	struct anim_instruction *p;
	vector_foreach_p(p, stream) {
		if (p->op == ANIM_OP_LOAD_PALETTE) {
			int no = get_palette_no(palettes, p->arg);
			if (no < 0)
				ERROR("Invalid palette index: %u", p->arg);
			p->arg = pal_addr[no];
		}
		pack_s4_instruction(out, p);
	}
}

static void pack_a_stream(struct buffer *out, anim_stream stream)
{
	struct anim_instruction *p;
	vector_foreach_p(p, stream) {
		pack_a_instruction(out, p);
	}
	buffer_write_u16(out, 0xffff);
}

static void anim_pack_s4(struct anim *in, struct buffer *out)
{
	buffer_write_u8(out, 10);
	buffer_seek(out, 1 + 10 * 2);

	struct anim_draw_call *call;
	vector_foreach_p(call, in->draw_calls) {
		pack_s4_draw_call(out, call);
	}

	uint16_t stream_addr[10];
	for (int i = 0; i < 10; i++) {
		stream_addr[i] = out->index;
		pack_s4_stream(out, in->streams[i]);
	}

	size_t end = out->index;
	buffer_seek(out, 1);
	for (int i = 0; i < 10; i++) {
		buffer_write_u16(out, stream_addr[i]);
	}
	buffer_seek(out, end);
}

static void anim_pack_a8(struct anim *in, struct buffer *out)
{
	buffer_write_u8(out, vector_length(in->draw_calls) + vector_length(in->palettes));
	buffer_seek(out, 1 + 10 * 2);

	struct anim_draw_call *call;
	vector_foreach_p(call, in->draw_calls) {
		pack_a8_draw_call(out, call);
	}

	uint16_t *pal_addr = xcalloc(vector_length(in->palettes), sizeof(uint16_t));

	int no = 0;
	struct anim_palette *pal;
	vector_foreach_p(pal, in->palettes) {
		pal_addr[no++] = out->index;
		for (int i = 0; i < 256; i++) {
			buffer_write_u8(out, pal->colors[i].r);
			buffer_write_u8(out, pal->colors[i].g);
			buffer_write_u8(out, pal->colors[i].b);
		}
	}

	uint16_t stream_addr[10];
	for (int i = 0; i < 10; i++) {
		stream_addr[i] = out->index;
		pack_a8_stream(out, in->streams[i], in->palettes, pal_addr);
	}

	size_t end = out->index;
	buffer_seek(out, 1);
	for (int i = 0; i < 10; i++) {
		buffer_write_u16(out, stream_addr[i]);
	}
	buffer_seek(out, end);
	free(pal_addr);
}

static void anim_pack_a(struct anim *in, struct buffer *out)
{
	buffer_write_u16(out, vector_length(in->draw_calls));
	buffer_seek(out, 2 + 100 * 4);

	struct anim_draw_call *call;
	vector_foreach_p(call, in->draw_calls) {
		pack_a_draw_call(out, call);
	}

	uint32_t stream_addr[100];
	for (int i = 0; i < 100; i++) {
		stream_addr[i] = out->index;
		pack_a_stream(out, in->streams[i]);
	}

	size_t end = out->index;
	buffer_seek(out, 2);
	for (int i = 0; i < 100; i++) {
		buffer_write_u32(out, stream_addr[i]);
	}
	buffer_seek(out, end);
}

uint8_t *anim_pack(struct anim *in, size_t *size_out)
{
	struct buffer out;
	buffer_init(&out, NULL, 0);

	if (anim_type == ANIM_S4)
		anim_pack_s4(in, &out);
	else if (anim_type == ANIM_A8)
		anim_pack_a8(in, &out);
	else
		anim_pack_a(in, &out);
	*size_out = out.index;
	return out.buf;
}
