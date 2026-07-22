#include "context.h"
#include "error.h"
#include "resource/queue.h"

#include <stdlib.h>

struct rtgl_context* current_context = NULL;

struct rtgl_context* rtgl_get_current_context(void) {
	return current_context;
}

struct rtgl_context* rtgl_create_context(rtgl_context_flags flags) {
	RTGL_ALLOC(result, struct rtgl_context, 1, "OpenGL context");
	if (!result) {
		return NULL;
	}
	result->flags = flags;
	rtgl_printf("rt-opengl: creating context (presentation=%u)\n", flags.presentation ? 1u : 0u);
	rtgl_context_init(result);
	if (rtgl_error() != RT_SUCCESS) {
		rtgl_context_destroy(result);
		return NULL;
	}
	return result;
}

void rtgl_context_init(struct rtgl_context* ctx) {
	if (!rtgl_execution_init(ctx)) {
		return;
	}
	ctx->queues = calloc(1, sizeof(*ctx->queues));
	RTGL_CHECK_ALLOC(ctx->queues, sizeof(*ctx->queues), "OpenGL queue handles");
	if (!ctx->queues) {
		return;
	}
	ctx->queues[0] = rtgl_queue_create(ctx);
	if (!ctx->queues[0]) {
		return;
	}
	ctx->queue_count = 1;
	rtgl_printf("rt-opengl: context ready\n");
}

void rtgl_context_finish(struct rtgl_context* ctx) {
	rtgl_execution_finish(ctx);
	for (u32 i = 0; i < ctx->queue_count; i++) {
		rtgl_queue_destroy(ctx, ctx->queues[i]);
	}
	free(ctx->queues);
	ctx->queues = NULL;
	ctx->queue_count = 0;
}

void rtgl_context_destroy(struct rtgl_context* ctx) {
	if (!ctx) {
		return;
	}

	rtgl_context_finish(ctx);
	free(ctx);
}

struct rtgl_queue* rtgl_context_graphics_queue(struct rtgl_context* ctx) {
	return ctx->queues[0];
}
