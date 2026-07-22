#ifndef RTGL_CONTEXT_H
#define RTGL_CONTEXT_H
#include "execution.h"
#include "platform/context.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rtgl_context_flags {
	unsigned presentation : 1;
} rtgl_context_flags;

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtgl_context {
	struct rtgl_execution_context execution;
	struct rtgl_queue** queues;
	u32 queue_count;
	rtgl_context_flags flags;
};
extern struct rtgl_context* current_context;

struct rtgl_context* rtgl_get_current_context(void);
struct rtgl_context* rtgl_create_context(rtgl_context_flags flags);
void rtgl_context_init(struct rtgl_context* ctx);
void rtgl_context_finish(struct rtgl_context* ctx);
void rtgl_context_destroy(struct rtgl_context* ctx);
struct rtgl_queue* rtgl_context_graphics_queue(struct rtgl_context* ctx);

#ifdef __cplusplus
}
#endif
#endif /* RTGL_CONTEXT_H */
