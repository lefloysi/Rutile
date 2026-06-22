#ifndef RTGL33_CONTEXT_H
#define RTGL33_CONTEXT_H

#include "types.h"

typedef struct rtgl_context_flags {
	unsigned presentation : 1;
} rtgl_context_flags;

struct rtgl_context {
	rtgl_context_flags flags;
};

extern struct rtgl_context* current_context;

struct rtgl_context* rtgl_get_current_context(void);
struct rtgl_context* rtgl_create_context(rtgl_context_flags flags);
void rtgl_context_init(struct rtgl_context* ctx);
void rtgl_context_finish(struct rtgl_context* ctx);
void rtgl_context_destroy(struct rtgl_context* ctx);

#endif /* RTGL33_CONTEXT_H */
