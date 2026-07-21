#ifndef RTGL_CONTEXT_H
#define RTGL_CONTEXT_H
#include "platform/context.h"
#include "types.h"

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rtgl_context_flags {
	unsigned presentation : 1;
} rtgl_context_flags;

struct rtgl_exec_job;
typedef void (*rtgl_exec_proc)(struct rtgl_context* ctx, void* data);

struct rtgl_exec_job {
	rtgl_exec_proc proc;
	void* data;
	HANDLE done_event;
	struct rtgl_exec_job* next;
};

struct rtgl_context {
	struct gl_context* gl_context;
#if defined(_WIN32)
	HANDLE thread_handle;
	HANDLE ready_event;
	HANDLE stop_event;
	HANDLE work_event;
	CRITICAL_SECTION work_lock;
#endif
	struct rtgl_exec_job* work_first;
	struct rtgl_exec_job* work_last;
	unsigned thread_id;
	rtgl_context_flags flags;
};
extern struct rtgl_context* current_context;

struct rtgl_context* rtgl_get_current_context(void);
struct rtgl_context* rtgl_create_context(rtgl_context_flags flags);
void rtgl_context_init(struct rtgl_context* ctx);
void rtgl_context_finish(struct rtgl_context* ctx);
void rtgl_context_destroy(struct rtgl_context* ctx);
void rtgl_context_execute(struct rtgl_context* ctx, rtgl_exec_proc proc, void* data);
void rtgl_context_enqueue(struct rtgl_context* ctx, rtgl_exec_proc proc, void* data);

#ifdef __cplusplus
}
#endif
#endif /* RTGL_CONTEXT_H */
