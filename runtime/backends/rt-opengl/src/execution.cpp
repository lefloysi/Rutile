#include "execution_internal.hpp"

#include "glad/gl.h"
#include "core.h"
#include "resource/queue.h"

#include <assert.h>

static GLADapiproc rtgl_load_opengl_proc_from_context(void* context, const char* name) {
	return (GLADapiproc)rtgl_load_proc((struct gl_context*)context, name);
}

static bool rtgl_execution_create_best_context(struct rtgl_context* ctx) {
	static const u08 versions[][2] = {
		{ 4, 6 },
		{ 4, 5 },
		{ 4, 4 },
		{ 4, 3 },
		{ 4, 2 },
		{ 4, 1 },
		{ 4, 0 },
	};
	u08 forced_major = 0;
	u08 forced_minor = 0;

	if (rtgl_forced_context_version(&forced_major, &forced_minor)) {
		rtgl_clear_error();
		rtgl_printf("rt-opengl: forcing GL %u.%u context from backend setting\n", forced_major, forced_minor);
		ctx->execution.gl_context = rtgl_create_glcontext(forced_major, forced_minor, ctx->flags.presentation, NULL);
		if (ctx->execution.gl_context) {
			ctx->execution.gl_major = forced_major;
			ctx->execution.gl_minor = forced_minor;
			return true;
		}
		rtgl_throwf(RT_INITIALIZATION_FAILED, "failed to create forced OpenGL %u.%u core context", forced_major, forced_minor);
		return false;
	}

	for (u32 i = 0; i < (u32)(sizeof(versions) / sizeof(versions[0])); i++) {
		rtgl_clear_error();
		ctx->execution.gl_context = rtgl_create_glcontext(versions[i][0], versions[i][1], ctx->flags.presentation, NULL);
		if (ctx->execution.gl_context) {
			ctx->execution.gl_major = versions[i][0];
			ctx->execution.gl_minor = versions[i][1];
			return true;
		}
		rtgl_printf("rt-opengl: GL %u.%u context unavailable, trying lower version\n", versions[i][0], versions[i][1]);
	}
	rtgl_throwf(RT_INITIALIZATION_FAILED, "failed to create an OpenGL 4.0+ core context");
	return false;
}

static void rtgl_execution_detect_capabilities(struct rtgl_context* ctx) {
	ctx->execution.direct_state_access = GLAD_GL_VERSION_4_5 || GLAD_GL_ARB_direct_state_access;
	ctx->execution.texture_storage = GLAD_GL_VERSION_4_2 || GLAD_GL_ARB_texture_storage;
	ctx->execution.separate_shader_objects = GLAD_GL_VERSION_4_1 || GLAD_GL_ARB_separate_shader_objects;
	ctx->execution.shader_storage_buffer = GLAD_GL_VERSION_4_3 || GLAD_GL_ARB_shader_storage_buffer_object;
	ctx->execution.spirv = GLAD_GL_VERSION_4_6 || GLAD_GL_ARB_gl_spirv;
	rtgl_printf(
		"rt-opengl: capabilities dsa=%u texture_storage=%u sso=%u ssbo=%u spirv=%u\n",
		ctx->execution.direct_state_access ? 1u : 0u,
		ctx->execution.texture_storage ? 1u : 0u,
		ctx->execution.separate_shader_objects ? 1u : 0u,
		ctx->execution.shader_storage_buffer ? 1u : 0u,
		ctx->execution.spirv ? 1u : 0u);
}

void rtgl_execution_queue_complete_locked(struct rtgl_queue* queue, u64 value) {
	if (queue->completed_value < value) {
		queue->completed_value = value;
		if (queue->completion_condition) {
			rt_condition_broadcast(queue->completion_condition);
		}
	}
}

static bool rtgl_execution_accept_command(struct rtgl_context* ctx, rtgl_execution_command* command) {
	bool accepted;

	command->next = NULL;
	rt_mutex_lock(ctx->execution.work_lock);
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
	rt_mutex_unlock(ctx->execution.work_lock);
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

	rt_mutex_lock(ctx->execution.work_lock);
	command = ctx->execution.work_first;
	if (command) {
		ctx->execution.work_first = command->next;
		if (!ctx->execution.work_first) {
			ctx->execution.work_last = NULL;
			rt_event_reset(ctx->execution.work_event);
		}
	}
	rt_mutex_unlock(ctx->execution.work_lock);
	return command;
}

static void rtgl_execution_run_commands(struct rtgl_context* ctx) {
	rtgl_execution_command* command;

	while ((command = rtgl_execution_pop_command(ctx)) != NULL) {
		command->run(ctx, command);
		command->finish(command);
	}
}

static unsigned rtgl_execution_thread(void* arg) {
	struct rtgl_context* ctx = (struct rtgl_context*)arg;
	struct rt_event* wait_events[2];

	rtgl_printf("rt-opengl: worker thread starting\n");
	if (!rtgl_execution_create_best_context(ctx)) {
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
	if (!GLAD_GL_VERSION_4_0) {
		rtgl_throwf(RT_INITIALIZATION_FAILED, "OpenGL 4.0 is required");
		rtgl_release_current_context();
		rtgl_destroy_glcontext(ctx->execution.gl_context);
		ctx->execution.gl_context = NULL;
		rt_event_signal(ctx->execution.ready_event);
		return 0;
	}

	rtgl_execution_detect_capabilities(ctx);
	rtgl_printf("rt-opengl: loaded OpenGL %u.%u entry points\n", ctx->execution.gl_major, ctx->execution.gl_minor);
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
	rtgl_release_current_context();
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
	ctx->execution.work_lock = rt_mutex_create();
	ctx->execution.stopping = false;
	if (!ctx->execution.ready_event || !ctx->execution.stop_event || !ctx->execution.work_event || !ctx->execution.work_lock) {
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

	if (ctx->execution.work_lock) {
		rt_mutex_lock(ctx->execution.work_lock);
		ctx->execution.stopping = true;
		rt_mutex_unlock(ctx->execution.work_lock);
	}
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
	if (ctx->execution.work_lock) {
		rt_mutex_destroy(ctx->execution.work_lock);
		ctx->execution.work_lock = NULL;
	}
	ctx->execution.work_first = NULL;
	ctx->execution.work_last = NULL;
	ctx->execution.thread_id = 0;
}

struct gl_context* rtgl_execution_gl_context(struct rtgl_context* ctx) {
	return ctx->execution.gl_context;
}

bool rtgl_execution_is_thread(struct rtgl_context* ctx) {
	return rt_current_thread_id() == ctx->execution.thread_id;
}

void rtgl_execution_lock(struct rtgl_context* ctx) {
	rt_mutex_lock(ctx->execution.work_lock);
}

void rtgl_execution_unlock(struct rtgl_context* ctx) {
	rt_mutex_unlock(ctx->execution.work_lock);
}
