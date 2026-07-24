#ifndef RTGL_EXECUTION_INTERNAL_HPP
#define RTGL_EXECUTION_INTERNAL_HPP

#include "context.h"
#include "error.h"

#include <new>
#include <utility>

struct rtgl_execution_command {
	rtgl_execution_command* next;
	void (*run)(struct rtgl_context* ctx, rtgl_execution_command* command);
	void (*finish)(rtgl_execution_command* command);

	rtgl_execution_command(void (*next_run)(struct rtgl_context* ctx, rtgl_execution_command* command), void (*next_finish)(rtgl_execution_command* command))
		: next(NULL), run(next_run), finish(next_finish) {}
};

bool rtgl_execution_submit_stack_command(struct rtgl_context* ctx, rtgl_execution_command* command);
bool rtgl_execution_submit_heap_command(struct rtgl_context* ctx, rtgl_execution_command* command);
void rtgl_execution_queue_complete_locked(struct rtgl_queue* queue, u64 value);

template <class F>
struct rtgl_async_command_t final : rtgl_execution_command {
	F fn;

	explicit rtgl_async_command_t(F&& next_fn) : rtgl_execution_command(&run_impl, &finish_impl), fn(std::forward<F>(next_fn)) {}

	static void run_impl(struct rtgl_context* ctx, rtgl_execution_command* command) {
		static_cast<rtgl_async_command_t*>(command)->fn(ctx);
	}

	static void finish_impl(rtgl_execution_command* command) {
		delete static_cast<rtgl_async_command_t*>(command);
	}
};

template <class F>
struct rtgl_sync_command_t final : rtgl_execution_command {
	F fn;
	struct rt_event* done_event;

	rtgl_sync_command_t(F&& next_fn, struct rt_event* next_done_event)
		: rtgl_execution_command(&run_impl, &finish_impl), fn(std::forward<F>(next_fn)), done_event(next_done_event) {}

	static void run_impl(struct rtgl_context* ctx, rtgl_execution_command* command) {
		static_cast<rtgl_sync_command_t*>(command)->fn(ctx);
	}

	static void finish_impl(rtgl_execution_command* command) {
		rt_event_signal(static_cast<rtgl_sync_command_t*>(command)->done_event);
	}
};

template <class F>
bool rtgl_execution_submit_sync(struct rtgl_context* ctx, F&& fn) {
	struct rt_event* done_event;

	if (rtgl_execution_is_thread(ctx)) {
		fn(ctx);
		return true;
	}

	done_event = rt_event_create(true, false);
	if (!done_event) {
		rtgl_throwf(RT_PLATFORM_FAILURE, "failed to create OpenGL execution completion event");
		return false;
	}

	rtgl_sync_command_t<F> command(std::forward<F>(fn), done_event);
	if (!rtgl_execution_submit_stack_command(ctx, &command)) {
		rt_event_destroy(done_event);
		rtgl_throwf(RT_PLATFORM_FAILURE, "OpenGL execution context is shutting down");
		return false;
	}
	rt_event_wait(done_event);
	rt_event_destroy(done_event);
	return true;
}

template <class F>
bool rtgl_execution_submit_async(struct rtgl_context* ctx, F&& fn) {
	rtgl_async_command_t<F>* command;

	if (rtgl_execution_is_thread(ctx)) {
		fn(ctx);
		return true;
	}

	command = new (std::nothrow) rtgl_async_command_t<F>(std::forward<F>(fn));
	if (!command) {
		rtgl_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate OpenGL execution command");
		return false;
	}
	if (!rtgl_execution_submit_heap_command(ctx, command)) {
		return false;
	}
	return true;
}

#endif /* RTGL_EXECUTION_INTERNAL_HPP */
