#ifndef RTOPENGL_CONTEXT_H
#define RTOPENGL_CONTEXT_H
#include "platform/context.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rtgl_context_flags {
	unsigned presentation : 1;
} rtgl_context_flags;

struct rtgl_context {
	struct gl_context* gl_context;
	void* thread_handle;
	void* ready_event;
	void* stop_event;
	unsigned thread_id;
	rtgl_context_flags flags;
};
extern struct rtgl_context* current_context;

struct rtgl_context* rtgl_get_current_context(void);
struct rtgl_context* rtgl_create_context(rtgl_context_flags flags);
void rtgl_context_init(struct rtgl_context* ctx);
void rtgl_context_finish(struct rtgl_context* ctx);
void rtgl_context_destroy(struct rtgl_context* ctx);

#ifdef __cplusplus
}
#endif
#endif /* RTOPENGL_CONTEXT_H */
