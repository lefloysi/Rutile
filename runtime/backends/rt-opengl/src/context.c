#include "context.h"
#include "error.h"
#include "platform/context.h"

#include "glad\gl.h"

#include <assert.h>
#include <process.h>
#include <stdlib.h>
#include <windows.h>

struct rtgl_context* current_context = NULL;

static GLADapiproc rtgl_glad_load_proc(void* userptr, const char* name) {
	struct gl_context* gl_context = (struct gl_context*)userptr;
	return (GLADapiproc)rtgl_load_proc(gl_context, name);
}

static unsigned __stdcall rtgl_context_thread(void* arg) {
	struct rtgl_context* ctx = (struct rtgl_context*)arg;

	rtgl_printf("rt-opengl: worker thread starting\n");
	ctx->gl_context = rtgl_create_glcontext(3, 3, ctx->flags.presentation, NULL);
	if (!ctx->gl_context) {
		SetEvent((HANDLE)ctx->ready_event);
		return 0;
	}

	rtgl_make_glcontext_current(ctx->gl_context, NULL);
	if (!gladLoadGLUserPtr(rtgl_glad_load_proc, ctx->gl_context)) {
		rtgl_release_current_context();
		rtgl_destroy_glcontext(ctx->gl_context);
		ctx->gl_context = NULL;
		SetEvent((HANDLE)ctx->ready_event);
		return 0;
	}

	rtgl_printf("rt-opengl: loaded OpenGL entry points\n");
	rtgl_release_current_context();
	SetEvent((HANDLE)ctx->ready_event);
	WaitForSingleObject((HANDLE)ctx->stop_event, INFINITE);
	rtgl_printf("rt-opengl: worker thread stopping\n");
	rtgl_destroy_glcontext(ctx->gl_context);
	ctx->gl_context = NULL;
	return 0;
}

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
	assert(ctx);
	ctx->ready_event = CreateEventA(NULL, TRUE, FALSE, NULL);
	ctx->stop_event = CreateEventA(NULL, TRUE, FALSE, NULL);
	if (!ctx->ready_event || !ctx->stop_event) {
		rtgl_throwf(RT_PLATFORM_FAILURE, "failed to create OpenGL thread events");
		return;
	}

	ctx->thread_handle = (void*)_beginthreadex(NULL, 0, rtgl_context_thread, ctx, 0, &ctx->thread_id);
	if (!ctx->thread_handle) {
		rtgl_throwf(RT_PLATFORM_FAILURE, "failed to create OpenGL worker thread");
		return;
	}

	WaitForSingleObject((HANDLE)ctx->ready_event, INFINITE);
	if (!ctx->gl_context) {
		rtgl_throwf(RT_INITIALIZATION_FAILED, "failed to create OpenGL platform context");
		return;
	}
	rtgl_printf("rt-opengl: context ready\n");
}

void rtgl_context_finish(struct rtgl_context* ctx) {
	assert(ctx);
	if (ctx->stop_event) {
		SetEvent((HANDLE)ctx->stop_event);
	}
	if (ctx->thread_handle) {
		WaitForSingleObject((HANDLE)ctx->thread_handle, INFINITE);
		CloseHandle((HANDLE)ctx->thread_handle);
		ctx->thread_handle = NULL;
	}
	if (ctx->ready_event) {
		CloseHandle((HANDLE)ctx->ready_event);
		ctx->ready_event = NULL;
	}
	if (ctx->stop_event) {
		CloseHandle((HANDLE)ctx->stop_event);
		ctx->stop_event = NULL;
	}
}

void rtgl_context_destroy(struct rtgl_context* ctx) {
	if (!ctx) {
		return;
	}

	rtgl_context_finish(ctx);
	free(ctx);
}
