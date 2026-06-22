#include "context.h"
#include "error.h"

#include <assert.h>
#include <stdlib.h>

struct rtgl_context* current_context = NULL;

struct rtgl_context* rtgl_get_current_context(void) {
	return current_context;
}

struct rtgl_context* rtgl_create_context(rtgl_context_flags flags) {
	struct rtgl_context* result = calloc(1, sizeof(struct rtgl_context));
	RTGL_CHECK_ALLOC(result, sizeof(struct rtgl_context), "GL33 context");
	if (!result) { return NULL; }
	result->flags = flags;
	rtgl_context_init(result);
	if (rtgl_error() != RT_SUCCESS) {
		rtgl_context_destroy(result);
		return NULL;
	}
	return result;
}

void rtgl_context_init(struct rtgl_context* ctx) {
	assert(ctx);

}

void rtgl_context_finish(struct rtgl_context* ctx) {
	assert(ctx);

}

void rtgl_context_destroy(struct rtgl_context* ctx) {
	assert(ctx);

	rtgl_context_finish(ctx);
	free(ctx);
}
