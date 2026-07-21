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

static void rtgl_context_push_job(struct rtgl_context* ctx, struct rtgl_exec_job* job) {
	assert(ctx);
	assert(job);

	job->next = NULL;
	EnterCriticalSection(&ctx->work_lock);
	if (ctx->work_last) {
		ctx->work_last->next = job;
	} else {
		ctx->work_first = job;
	}
	ctx->work_last = job;
	LeaveCriticalSection(&ctx->work_lock);
	SetEvent(ctx->work_event);
}

static struct rtgl_exec_job* rtgl_context_pop_job(struct rtgl_context* ctx) {
	struct rtgl_exec_job* job;

	EnterCriticalSection(&ctx->work_lock);
	job = ctx->work_first;
	if (job) {
		ctx->work_first = job->next;
		if (!ctx->work_first) {
			ctx->work_last = NULL;
		}
	}
	if (!ctx->work_first) {
		ResetEvent(ctx->work_event);
	}
	LeaveCriticalSection(&ctx->work_lock);
	return job;
}

static void rtgl_context_run_jobs(struct rtgl_context* ctx) {
	struct rtgl_exec_job* job;

	while ((job = rtgl_context_pop_job(ctx)) != NULL) {
		job->proc(ctx, job->data);
		if (job->done_event) {
			SetEvent(job->done_event);
		} else {
			free(job);
		}
	}
}

static unsigned __stdcall rtgl_context_thread(void* arg) {
	struct rtgl_context* ctx = (struct rtgl_context*)arg;
	HANDLE wait_handles[2];

	rtgl_printf("rt-opengl: worker thread starting\n");
	ctx->gl_context = rtgl_create_glcontext(4, 6, ctx->flags.presentation, NULL);
	if (!ctx->gl_context) {
		SetEvent(ctx->ready_event);
		return 0;
	}

	rtgl_make_glcontext_current(ctx->gl_context, NULL);
	if (!gladLoadGLUserPtr(rtgl_glad_load_proc, ctx->gl_context)) {
		rtgl_release_current_context();
		rtgl_destroy_glcontext(ctx->gl_context);
		ctx->gl_context = NULL;
		SetEvent(ctx->ready_event);
		return 0;
	}
	if (!GLAD_GL_VERSION_4_6) {
		rtgl_throwf(RT_INITIALIZATION_FAILED, "OpenGL 4.6 is required");
		rtgl_release_current_context();
		rtgl_destroy_glcontext(ctx->gl_context);
		ctx->gl_context = NULL;
		SetEvent(ctx->ready_event);
		return 0;
	}

	rtgl_printf("rt-opengl: loaded OpenGL entry points\n");
	SetEvent(ctx->ready_event);
	wait_handles[0] = ctx->stop_event;
	wait_handles[1] = ctx->work_event;
	for (;;) {
		DWORD wait_result = WaitForMultipleObjects(2, wait_handles, FALSE, INFINITE);
		if (wait_result == WAIT_OBJECT_0) {
			break;
		}
		rtgl_context_run_jobs(ctx);
	}
	rtgl_context_run_jobs(ctx);
	rtgl_release_current_context();
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
	ctx->work_event = CreateEventA(NULL, TRUE, FALSE, NULL);
	InitializeCriticalSection(&ctx->work_lock);
	if (!ctx->ready_event || !ctx->stop_event || !ctx->work_event) {
		rtgl_throwf(RT_PLATFORM_FAILURE, "failed to create OpenGL thread events");
		return;
	}

	ctx->thread_handle = (HANDLE)_beginthreadex(NULL, 0, rtgl_context_thread, ctx, 0, &ctx->thread_id);
	if (!ctx->thread_handle) {
		rtgl_throwf(RT_PLATFORM_FAILURE, "failed to create OpenGL worker thread");
		return;
	}

	WaitForSingleObject(ctx->ready_event, INFINITE);
	if (!ctx->gl_context) {
		rtgl_throwf(RT_INITIALIZATION_FAILED, "failed to create OpenGL platform context");
		return;
	}
	rtgl_printf("rt-opengl: context ready\n");
}

void rtgl_context_finish(struct rtgl_context* ctx) {
	assert(ctx);
	if (ctx->stop_event) {
		SetEvent(ctx->stop_event);
	}
	if (ctx->thread_handle) {
		WaitForSingleObject(ctx->thread_handle, INFINITE);
		CloseHandle(ctx->thread_handle);
		ctx->thread_handle = NULL;
	}
	if (ctx->ready_event) {
		CloseHandle(ctx->ready_event);
		ctx->ready_event = NULL;
	}
	if (ctx->stop_event) {
		CloseHandle(ctx->stop_event);
		ctx->stop_event = NULL;
	}
	if (ctx->work_event) {
		CloseHandle(ctx->work_event);
		ctx->work_event = NULL;
	}
	DeleteCriticalSection(&ctx->work_lock);
}

void rtgl_context_destroy(struct rtgl_context* ctx) {
	if (!ctx) {
		return;
	}

	rtgl_context_finish(ctx);
	free(ctx);
}

void rtgl_context_execute(struct rtgl_context* ctx, rtgl_exec_proc proc, void* data) {
	struct rtgl_exec_job job;

	assert(ctx);
	assert(proc);

	if (GetCurrentThreadId() == ctx->thread_id) {
		proc(ctx, data);
		return;
	}

	job.proc = proc;
	job.data = data;
	job.done_event = CreateEventA(NULL, TRUE, FALSE, NULL);
	job.next = NULL;
	if (!job.done_event) {
		rtgl_throwf(RT_PLATFORM_FAILURE, "failed to create OpenGL execution completion event");
		return;
	}
	rtgl_context_push_job(ctx, &job);
	WaitForSingleObject(job.done_event, INFINITE);
	CloseHandle(job.done_event);
}

void rtgl_context_enqueue(struct rtgl_context* ctx, rtgl_exec_proc proc, void* data) {
	struct rtgl_exec_job* job;

	assert(ctx);
	assert(proc);

	if (GetCurrentThreadId() == ctx->thread_id) {
		proc(ctx, data);
		return;
	}

	job = calloc(1, sizeof(*job));
	RTGL_CHECK_ALLOC(job, sizeof(*job), "OpenGL execution job");
	if (!job) {
		return;
	}
	job->proc = proc;
	job->data = data;
	rtgl_context_push_job(ctx, job);
}
