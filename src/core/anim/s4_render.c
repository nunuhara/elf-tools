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

#include "nulib.h"
#include "ai5/cg.h"
#include "ai5/s4.h"

#define MSF_GIF_IMPL
#include "msf_gif.h"

struct stream_state {
	bool halted;
	bool dirty;
	unsigned ip;
	unsigned stalling;
	unsigned loop_start;
	unsigned loop_count;
	unsigned loop2_start;
	unsigned loop2_count;
};

static void fill_clip(struct s4_target *dst, struct s4_size *dim, struct cg_metrics *s_dst)
{
	if (unlikely(dst->x < 0)) {
		dim->w += dst->x;
		dst->x = 0;
	}
	if (unlikely(dst->y < 0)) {
		dim->h += dst->y;
		dst->y = 0;
	}
	if (unlikely(dst->x + dim->w > s_dst->w)) {
		dim->w = s_dst->w - dst->x;
	}
	if (unlikely(dst->y + dim->h > s_dst->h)) {
		dim->h = s_dst->h - dst->y;
	}
}

static void copy_clip(struct s4_target *src, struct s4_target *dst, struct s4_size *dim,
		struct cg_metrics *s_src, struct cg_metrics *s_dst)
{
	// XXX: apply offset from CG
	src->x -= s_src->x;
	src->y -= s_src->y;
	dst->x -= s_dst->x;
	dst->y -= s_dst->y;

	if (unlikely(src->x < 0)) {
		dim->w += src->x;
		dst->x -= src->x;
		src->x = 0;
	}
	if (unlikely(src->y < 0)) {
		dim->h += src->y;
		dst->y -= src->y;
		src->y = 0;
	}
	if (unlikely(dst->x < 0)) {
		dim->w += dst->x;
		src->x -= dst->x;
		dst->x = 0;
	}
	if (unlikely(dst->y < 0)) {
		dim->h += dst->y;
		src->y -= dst->y;
		dst->y = 0;
	}
	if (dim->w < 0 || dim->h < 0)
		return;
	if (unlikely(src->x + dim->w > s_src->w)) {
		dim->w = s_src->w - src->x;
	}
	if (unlikely(src->y + dim->h > s_src->h)) {
		dim->h = s_src->h - src->y;
	}
	if (unlikely(dst->x + dim->w > s_dst->w)) {
		dim->w = s_dst->w - dst->x;
	}
	if (unlikely(dst->y + dim->h > s_dst->h)) {
		dim->h = s_dst->h - dst->y;
	}
}

static void draw_call_clip(struct s4_draw_call *call, struct cg *src, struct cg *dst)
{
#define TARGET(t) ((t.i) ? &src->metrics : &dst->metrics)
	switch (call->op) {
	case S4_DRAW_OP_FILL:
		fill_clip(&call->fill.dst, &call->fill.dim, TARGET(call->fill.dst));
		break;
	case S4_DRAW_OP_COPY:
	case S4_DRAW_OP_COPY_MASKED:
	case S4_DRAW_OP_SWAP:
		copy_clip(&call->copy.src, &call->copy.dst, &call->copy.dim,
				TARGET(call->copy.src), TARGET(call->copy.dst));
		break;
	case S4_DRAW_OP_COMPOSE:
		// FIXME: this is not quite correct, but it shouldn't matter since
		//        clipping is an error condition anyways
		copy_clip(&call->compose.bg, &call->compose.dst, &call->compose.dim,
				TARGET(call->compose.bg), TARGET(call->compose.dst));
		copy_clip(&call->compose.fg, &call->compose.dst, &call->compose.dim,
				TARGET(call->compose.fg), TARGET(call->compose.dst));
		break;
	case S4_DRAW_OP_SET_COLOR:
	case S4_DRAW_OP_SET_PALETTE:
		break;
	}
#undef TARGET
}

static uint8_t *px_offset(struct cg *cg, unsigned x, unsigned y)
{
	return cg->pixels + y * cg->metrics.w + x;
}

static void stream_render_fill(struct s4_fill_args *call, struct cg *dst)
{
	if (call->dim.w < 1 || call->dim.h < 1)
		return;
	for (int row = 0; row < call->dim.h; row++) {
		uint8_t *p = px_offset(dst, call->dst.x, call->dst.y + row);
		memset(p, 8, call->dim.w);
	}
}

static void stream_render_copy(struct s4_copy_args *call, struct cg *src, struct cg *dst)
{
	if (call->dim.w < 1 || call->dim.h < 1)
		return;
	for (int row = 0; row < call->dim.h; row++) {
		uint8_t *src_p = px_offset(src, call->src.x, call->src.y + row);
		uint8_t *dst_p = px_offset(dst, call->dst.x, call->dst.y + row);
		memcpy(dst_p, src_p, call->dim.w);
	}
}

static void stream_render_copy_masked(struct s4_copy_args *call, struct cg *src,
		struct cg *dst)
{
	if (call->dim.w < 1 || call->dim.h < 1)
		return;
	for (int row = 0; row < call->dim.h; row++) {
		uint8_t *src_p = px_offset(src, call->src.x, call->src.y + row);
		uint8_t *dst_p = px_offset(dst, call->dst.x, call->dst.y + row);
		for (int col = 0; col < call->dim.w; col++, src_p++, dst_p++) {
			// XXX: we assume mask color is 8
			if (*src_p != 8)
				*dst_p = *src_p;
		}
		memcpy(dst_p, src_p, call->dim.w);
	}
}

static void stream_render_swap(struct s4_copy_args *call, struct cg *src, struct cg *dst)
{
	if (call->dim.w < 1 || call->dim.h < 1)
		return;
	for (int row = 0; row < call->dim.h; row++) {
		uint8_t *src_p = px_offset(src, call->src.x, call->src.y + row);
		uint8_t *dst_p = px_offset(dst, call->dst.x, call->dst.y + row);
		for (int col = 0; col < call->dim.w; col++, src_p++, dst_p++) {
			uint8_t tmp = *dst_p;
			*dst_p = *src_p;
			*src_p = tmp;
		}
	}
}

static void stream_render_compose(struct s4_compose_args *call, struct cg *fg, struct cg *bg,
		struct cg *dst)
{
	if (call->dim.w < 1 || call->dim.h < 1)
		return;
	for (int row = 0; row < call->dim.h; row++) {
		uint8_t *fg_p = px_offset(fg, call->fg.x, call->fg.y + row);
		uint8_t *bg_p = px_offset(bg, call->bg.x, call->bg.y + row);
		uint8_t *dst_p = px_offset(dst, call->dst.x, call->dst.y + row);
		for (int col = 0; col < call->dim.w; call++, fg_p++, bg_p++, dst_p++) {
			// XXX: we assume mask color is 8
			*dst_p = *fg_p == 8 ? *bg_p : *fg_p;
		}
	}
}

static void stream_render_set_color(struct s4_set_color_args *call, struct cg *src,
		struct cg *dst)
{
	uint8_t *src_c = src->palette + call->i * 4;
	uint8_t *dst_c = dst->palette + call->i * 4;
	src_c[0] = dst_c[0] = call->color.b;
	src_c[1] = dst_c[1] = call->color.g;
	src_c[2] = dst_c[2] = call->color.r;
	src_c[3] = dst_c[3] = 0;
}

static void stream_render_set_palette(struct s4_set_palette_args *call, struct cg *src,
		struct cg *dst)
{
	uint8_t *src_c = src->palette;
	uint8_t *dst_c = dst->palette;
	for (int i = 0; i < 16; i++, src_c += 4, dst_c += 4) {
		src_c[0] = dst_c[0] = call->colors[i].b;
		src_c[1] = dst_c[1] = call->colors[i].g;
		src_c[2] = dst_c[2] = call->colors[i].r;
		src_c[3] = dst_c[3] = 0;
	}
}

static void stream_render_draw(struct s4_draw_call *call, struct cg *src, struct cg *dst)
{
#define TARGET(t) ((t).i ? src : dst)
	switch (call->op) {
	case S4_DRAW_OP_FILL:
		stream_render_fill(&call->fill, TARGET(call->fill.dst));
		break;
	case S4_DRAW_OP_COPY:
		stream_render_copy(&call->copy, TARGET(call->copy.src),
				TARGET(call->copy.dst));
		break;
	case S4_DRAW_OP_COPY_MASKED:
		stream_render_copy_masked(&call->copy, TARGET(call->copy.src),
				TARGET(call->copy.dst));
		break;
	case S4_DRAW_OP_SWAP:
		stream_render_swap(&call->copy, TARGET(call->copy.src),
				TARGET(call->copy.dst));
		break;
	case S4_DRAW_OP_COMPOSE:
		stream_render_compose(&call->compose, TARGET(call->compose.fg),
				TARGET(call->compose.bg), TARGET(call->compose.dst));
		break;
	case S4_DRAW_OP_SET_COLOR:
		stream_render_set_color(&call->set_color, src, dst);
		break;
	case S4_DRAW_OP_SET_PALETTE:
		stream_render_set_palette(&call->set_palette, src, dst);
		break;
	}
#undef TARGET
}

static bool stream_render(struct s4 *anim, unsigned stream, struct stream_state *state,
		struct cg *src, struct cg *dst)
{
	if (state->stalling) {
		state->stalling--;
		return true;
	}
	if (state->ip >= vector_length(anim->streams[stream])) {
		state->halted = true;
		return true;
	}

	struct s4_instruction instr = vector_A(anim->streams[stream], state->ip);
	switch (instr.op) {
	case S4_OP_DRAW:
		stream_render_draw(&vector_A(anim->draw_calls, instr.arg), src, dst);
		state->ip++;
		state->dirty = true;
		return false;
	case S4_OP_NOOP:
	case S4_OP_CHECK_STOP:
		state->ip++;
		break;
	case S4_OP_STALL:
		state->stalling = instr.arg;
		state->ip++;
		break;
	case S4_OP_RESET:
		// XXX: no point looping for offline render
		state->halted = true;
		//state->ip = 0;
		break;
	case S4_OP_HALT:
		state->halted = true;
		break;
	case S4_OP_LOOP_START:
		state->loop_start = state->ip + 1;
		state->loop_count = instr.arg;
		state->ip++;
		break;
	case S4_OP_LOOP_END:
		if (state->loop_count && --state->loop_count) {
			state->ip = state->loop_start;
		} else {
			state->ip++;
		}
		break;
	case S4_OP_LOOP2_START:
		state->loop2_start = state->ip + 1;
		state->loop2_count = instr.arg;
		state->ip++;
		break;
	case S4_OP_LOOP2_END:
		if (state->loop2_count && --state->loop2_count) {
			state->ip = state->loop2_start;
		} else {
			state->ip++;
		}
		break;
	}
	return false;
}

static struct cg *make_blank_cg(struct cg *src)
{
	struct cg *dst = xcalloc(1, sizeof(struct cg));
	dst->metrics.x = 0;
	dst->metrics.y = 0;
	dst->metrics.w = 640;
	dst->metrics.h = 400;
	dst->pixels = xcalloc(1, 640 * 400);
	dst->palette = xmalloc(256 * 4);
	memcpy(dst->palette, src->palette, 256 * 4);
	return dst;
}

struct s4_frame {
	struct cg *cg;
	unsigned nr_frames;
};

struct s4_frame *s4_render_frames(struct s4 *anim, struct cg *src, struct cg *dst,
		unsigned max_frames)
{
	bool dst_needs_free = false;

	if (!src->palette || (dst && !dst->palette)) {
		WARNING("Only indexed CGs are supported for S4 animations");
		return NULL;
	}

	// create empty CG if `dst` not provided
	if (!dst) {
		dst = make_blank_cg(src);
		dst_needs_free = true;
	}

	// ensure all draw operations are clipped to src/dst size
	struct s4_draw_call *call;
	vector_foreach_p(call, anim->draw_calls) {
		draw_call_clip(call, src, dst);
	}

	// halt all empty streams
	struct stream_state state[S4_MAX_STREAMS] = {0};
	for (int i = 0; i < S4_MAX_STREAMS; i++) {
		if (vector_length(anim->streams[i]) == 0)
			state[i].halted = true;
	}

	struct s4_frame *frames = xcalloc(max_frames, sizeof(struct s4_frame));
	frames[0].cg = cg_depalettize_copy(dst);

	for (unsigned frame = 0; frame < max_frames;) {
		bool halted = true;
		bool flush = false;
		for (int stream = 0; stream < S4_MAX_STREAMS; stream++) {
			if (!state[stream].halted) {
				halted = false;
				if (stream_render(anim, stream, &state[stream], src, dst)
						&& state[stream].dirty) {
					flush = true;
					state[stream].dirty = false;
				}
			}
		}
		if (halted)
			break;
		if (flush) {
			frame++;
			if (frame >= max_frames)
				break;
			frames[frame].cg = cg_depalettize_copy(dst);
			frames[frame].nr_frames = 1;
		} else {
			frames[frame].nr_frames++;
		}
	}

	if (dst_needs_free)
		cg_free(dst);

	return frames;
}

uint8_t *s4_render_gif(struct s4 *anim, struct cg *src, struct cg *dst,
		unsigned max_frames, size_t *size_out)
{
	struct s4_frame *frames = s4_render_frames(anim, src, dst, max_frames);
	if (!frames)
		return NULL;

	MsfGifState gif_state = {};
	msf_gif_begin(&gif_state, dst->metrics.w, dst->metrics.h);
	// XXX: frames[0].nr_frames can be 0 if the first instruction is a draw call
	for (int i = frames[0].nr_frames ? 0 : 1; i < max_frames && frames[i].cg; i++) {
		// XXX: we want 16ms frame time, but gif only supports centiseconds
		unsigned t = (frames[i].nr_frames * 16) / 10;
		if (!msf_gif_frame(&gif_state, frames[i].cg->pixels, t, 16, dst->metrics.w * 4))
			ERROR("msf_gif_frame");
	}
	for (int i = 0; i < max_frames && frames[i].cg; i++) {
		cg_free(frames[i].cg);
	}
	free(frames);

	MsfGifResult gif = msf_gif_end(&gif_state);
	*size_out = gif.dataSize;
	return gif.data;
}
