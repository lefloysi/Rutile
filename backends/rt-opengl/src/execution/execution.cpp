#include "execution/internal.hpp"

#include "core.h"
#include "glad/gl.h"
#include "resource/queue.h"

#include <assert.h>

static thread_local rtgl_execution_command* rtgl_current_execution_command = NULL;

void rtgl_execution_defer_current_command(void) {
	if (rtgl_current_execution_command) {
		rtgl_current_execution_command->deferred = true;
	}
}

static void rtgl_debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* user_param) {
	(void)source;
	(void)type;
	(void)length;
	(void)id;
	(void)user_param;
	if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) {
		return;
	}
	rtgl_printf("rt-opengl: GL debug severity=%u: %s\n", (unsigned)severity, message ? message : "(no message)");
}

static GLADapiproc rtgl_load_opengl_proc_from_context(void* context, const char* name) {
	return (GLADapiproc)rtgl_load_proc((struct gl_context*)context, name);
}

static bool rtgl_execution_create_context(struct rtgl_context* ctx) {
	rtgl_printf("rt-opengl: initializing OpenGL 4.6\n");
	ctx->execution.gl_context = rtgl_create_glcontext(4, 6, ctx->flags.presentation, NULL);
	if (!ctx->execution.gl_context) {
		rtgl_throwf(RT_INITIALIZATION_FAILED, "failed to create an OpenGL 4.6 core context");
		return false;
	}
	return true;
}

void rtgl_execution_queue_complete_locked(struct rtgl_queue* queue, u64 value) {
	if (queue->completed_value < value) {
		queue->completed_value = value;
		rtgl_condition_broadcast(&queue->completion_condition);
		rt_event_signal(queue->base.ctx->execution.work_event);
	}
}
static bool rtgl_execution_accept_command(struct rtgl_context* ctx, rtgl_execution_command* command) {
	bool accepted;

	command->next = NULL;
	rtgl_mutex_lock(&ctx->execution.work_lock);
	accepted = !ctx->execution.stopping;
	if (accepted) {
		if (ctx->execution.work_last) {
			ctx->execution.work_last->next = command;
		} else {
			ctx->execution.work_first = command;
		}
		ctx->execution.work_last = command;
		rt_event_signal(ctx->execution.work_event);
	}
	rtgl_mutex_unlock(&ctx->execution.work_lock);
	return accepted;
}

bool rtgl_execution_submit_stack_command(struct rtgl_context* ctx, rtgl_execution_command* command) {
	return rtgl_execution_accept_command(ctx, command);
}

bool rtgl_execution_submit_heap_command(struct rtgl_context* ctx, rtgl_execution_command* command) {
	if (rtgl_execution_accept_command(ctx, command)) {
		return true;
	}
	command->finish(command);
	return false;
}

static rtgl_execution_command* rtgl_execution_pop_command(struct rtgl_context* ctx) {
	rtgl_execution_command* command;

	rtgl_mutex_lock(&ctx->execution.work_lock);
	command = ctx->execution.work_first;
	if (command) {
		ctx->execution.work_first = command->next;
		if (!ctx->execution.work_first) {
			ctx->execution.work_last = NULL;
			if (!ctx->execution.deferred_first) {
				rt_event_reset(ctx->execution.work_event);
			}
		}
	}
	rtgl_mutex_unlock(&ctx->execution.work_lock);
	return command;
}

static void rtgl_execution_defer_command(struct rtgl_context* ctx, rtgl_execution_command* command) {
	rtgl_mutex_lock(&ctx->execution.work_lock);
	command->next = NULL;
	if (ctx->execution.deferred_last) {
		ctx->execution.deferred_last->next = command;
	} else {
		ctx->execution.deferred_first = command;
	}
	ctx->execution.deferred_last = command;
	rtgl_mutex_unlock(&ctx->execution.work_lock);
}

static void rtgl_execution_wake_deferred(struct rtgl_context* ctx) {
	rtgl_mutex_lock(&ctx->execution.work_lock);
	if (ctx->execution.deferred_first) {
		if (ctx->execution.work_last) {
			ctx->execution.work_last->next = ctx->execution.deferred_first;
		} else {
			ctx->execution.work_first = ctx->execution.deferred_first;
		}
		ctx->execution.work_last = ctx->execution.deferred_last;
		ctx->execution.deferred_first = NULL;
		ctx->execution.deferred_last = NULL;
	}
	rtgl_mutex_unlock(&ctx->execution.work_lock);
}

static void rtgl_execution_run_commands(struct rtgl_context* ctx) {
	rtgl_execution_wake_deferred(ctx);
	rtgl_execution_command* command = rtgl_execution_pop_command(ctx);

	if (!command) {
		return;
	}

	rtgl_make_glcontext_current(ctx->execution.gl_context, NULL);
	do {
		command->deferred = false;
		rtgl_current_execution_command = command;
		command->run(ctx, command);
		rtgl_current_execution_command = NULL;
		if (command->deferred) {
			rtgl_execution_defer_command(ctx, command);
		} else {
			command->finish(command);
		}
		command = rtgl_execution_pop_command(ctx);
	} while (command);
	rtgl_release_current_context();
}
static unsigned rtgl_execution_thread(void* arg) {
	struct rtgl_context* ctx = (struct rtgl_context*)arg;
	struct rt_event* wait_events[2];

	rtgl_printf("rt-opengl: worker thread starting\n");
	if (!rtgl_execution_create_context(ctx)) {
		rt_event_signal(ctx->execution.ready_event);
		return 0;
	}

	rtgl_make_glcontext_current(ctx->execution.gl_context, NULL);
	if (!gladLoadGLUserPtr(rtgl_load_opengl_proc_from_context, ctx->execution.gl_context)) {
		rtgl_release_current_context();
		rtgl_destroy_glcontext(ctx->execution.gl_context);
		ctx->execution.gl_context = NULL;
		rt_event_signal(ctx->execution.ready_event);
		return 0;
	}
	if (!GLAD_GL_VERSION_4_6) {
		rtgl_throwf(RT_INITIALIZATION_FAILED, "OpenGL 4.6 core is required");
		rtgl_release_current_context();
		rtgl_destroy_glcontext(ctx->execution.gl_context);
		ctx->execution.gl_context = NULL;
		rt_event_signal(ctx->execution.ready_event);
		return 0;
	}
	if (glDebugMessageCallback) {
		glEnable(GL_DEBUG_OUTPUT);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageCallback(rtgl_debug_callback, ctx);
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, GL_FALSE);
		rtgl_printf("rt-opengl: GL debug callback enabled\n");
	}

	glClipControl(GL_UPPER_LEFT, GL_ZERO_TO_ONE);
	rtgl_printf("rt-opengl: loaded OpenGL 4.6 entry points\n");
	rtgl_release_current_context();
	rt_event_signal(ctx->execution.ready_event);
	wait_events[0] = ctx->execution.stop_event;
	wait_events[1] = ctx->execution.work_event;
	for (;;) {
		u32 wait_result = rt_event_wait_any(wait_events, 2);
		if (wait_result == 0 || wait_result >= 2) {
			break;
		}
		rtgl_execution_run_commands(ctx);
	}
	rtgl_execution_run_commands(ctx);
	rtgl_printf("rt-opengl: worker thread stopping\n");
	rtgl_destroy_glcontext(ctx->execution.gl_context);
	ctx->execution.gl_context = NULL;
	return 0;
}

bool rtgl_execution_init(struct rtgl_context* ctx) {
	assert(ctx);

	ctx->execution.ready_event = rt_event_create(true, false);
	ctx->execution.stop_event = rt_event_create(true, false);
	ctx->execution.work_event = rt_event_create(true, false);
	rtgl_mutex_init(&ctx->execution.work_lock);
	ctx->execution.stopping = false;
	if (!ctx->execution.ready_event || !ctx->execution.stop_event || !ctx->execution.work_event) {
		rtgl_throwf(RT_PLATFORM_FAILURE, "failed to create OpenGL thread synchronization");
		return false;
	}

	ctx->execution.thread = rt_thread_create(rtgl_execution_thread, ctx);
	if (!ctx->execution.thread) {
		rtgl_throwf(RT_PLATFORM_FAILURE, "failed to create OpenGL worker thread");
		return false;
	}
	ctx->execution.thread_id = rt_thread_id(ctx->execution.thread);
	rt_event_wait(ctx->execution.ready_event);
	if (!ctx->execution.gl_context) {
		rtgl_throwf(RT_INITIALIZATION_FAILED, "failed to create OpenGL platform context");
		return false;
	}
	return true;
}

void rtgl_execution_finish(struct rtgl_context* ctx) {
	assert(ctx);

	rtgl_mutex_lock(&ctx->execution.work_lock);
	ctx->execution.stopping = true;
	rtgl_mutex_unlock(&ctx->execution.work_lock);
	if (ctx->execution.stop_event) {
		rt_event_signal(ctx->execution.stop_event);
	}
	if (ctx->execution.thread) {
		rt_thread_join(ctx->execution.thread);
		ctx->execution.thread = NULL;
	}
	if (ctx->execution.ready_event) {
		rt_event_destroy(ctx->execution.ready_event);
		ctx->execution.ready_event = NULL;
	}
	if (ctx->execution.stop_event) {
		rt_event_destroy(ctx->execution.stop_event);
		ctx->execution.stop_event = NULL;
	}
	if (ctx->execution.work_event) {
		rt_event_destroy(ctx->execution.work_event);
		ctx->execution.work_event = NULL;
	}
	rtgl_mutex_finish(&ctx->execution.work_lock);
	ctx->execution.work_first = NULL;
	ctx->execution.work_last = NULL;
	ctx->execution.deferred_first = NULL;
	ctx->execution.deferred_last = NULL;
	ctx->execution.thread_id = 0;
}

struct gl_context* rtgl_execution_gl_context(struct rtgl_context* ctx) {
	return ctx->execution.gl_context;
}

bool rtgl_execution_is_thread(struct rtgl_context* ctx) {
	return rt_current_thread_id() == ctx->execution.thread_id;
}

void rtgl_execution_lock(struct rtgl_context* ctx) {
	rtgl_mutex_lock(&ctx->execution.work_lock);
}

void rtgl_execution_unlock(struct rtgl_context* ctx) {
	rtgl_mutex_unlock(&ctx->execution.work_lock);
}
