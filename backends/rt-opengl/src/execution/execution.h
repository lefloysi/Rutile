#ifndef RTGL_EXECUTION_H
#define RTGL_EXECUTION_H

#include "platform/context.h"
#include "resource/resource.h"
#include "system/sync.h"

#ifdef __cplusplus
extern "C" {
#endif

struct rtgl_execution_context {
	struct gl_context* gl_context;
	struct rt_thread* thread;
	struct rt_event* ready_event;
	struct rt_event* stop_event;
	struct rt_event* work_event;
	struct rtgl_mutex work_lock;
	struct rtgl_execution_command* work_first;
	struct rtgl_execution_command* work_last;
	struct rtgl_execution_command* deferred_first;
	struct rtgl_execution_command* deferred_last;
	unsigned thread_id;
	bool stopping;
};

bool rtgl_execution_init(struct rtgl_context* ctx);
void rtgl_execution_finish(struct rtgl_context* ctx);
struct gl_context* rtgl_execution_gl_context(struct rtgl_context* ctx);
bool rtgl_execution_is_thread(struct rtgl_context* ctx);
void rtgl_execution_lock(struct rtgl_context* ctx);
void rtgl_execution_unlock(struct rtgl_context* ctx);

void rtgl_execution_buffer_create(struct rtgl_context* ctx, struct rtgl_buffer_storage* storage);
void rtgl_execution_buffer_delete(struct rtgl_context* ctx, struct rtgl_buffer_storage* storage);
void rtgl_execution_buffer_data(struct rtgl_context* ctx, struct rtgl_buffer_storage* storage, usize size, const u08* bytes);
void rtgl_execution_buffer_copy(struct rtgl_context* ctx, struct rtgl_buffer_storage* source, struct rtgl_buffer_storage* target);
void rtgl_execution_buffer_subdata(struct rtgl_context* ctx, struct rtgl_buffer_storage* storage, u64 offset, u64 size, const u08* bytes);
void rtgl_execution_buffer_read(struct rtgl_context* ctx, struct rtgl_buffer_storage* storage, u64 offset, u64 size, u08* bytes);

void rtgl_execution_framebuffer_create(struct rtgl_context* ctx, struct rtgl_framebuffer* framebuffer);
void rtgl_execution_framebuffer_delete(struct rtgl_context* ctx, struct rtgl_framebuffer* framebuffer);
void rtgl_execution_framebuffer_attach_color(struct rtgl_context* ctx, struct rtgl_framebuffer* framebuffer, u32 slot, struct rtgl_texture_view* view);
void rtgl_execution_framebuffer_attach_depth(struct rtgl_context* ctx, struct rtgl_framebuffer* framebuffer, struct rtgl_texture_view* view);
void rtgl_execution_framebuffer_attach_stencil(struct rtgl_context* ctx, struct rtgl_framebuffer* framebuffer, struct rtgl_texture_view* view);

void rtgl_execution_program_finalize(struct rtgl_context* ctx, struct rtgl_program* program);
void rtgl_execution_program_destroy(struct rtgl_context* ctx, struct rtgl_program* program);

void rtgl_execution_texture_create(struct rtgl_context* ctx, struct rtgl_image_base* image);
void rtgl_execution_texture_delete(struct rtgl_context* ctx, struct rtgl_image_base* image);
void rtgl_execution_texture_view_delete_sampler(struct rtgl_context* ctx, struct rtgl_texture_view* view);
void rtgl_execution_texture_data(struct rtgl_context* ctx, struct rtgl_image_base* image, const void* data);
void rtgl_execution_texture_subdata(struct rtgl_context* ctx, struct rtgl_image_base* image, rt_texture_range range, const void* data);
void rtgl_execution_buffer_to_texture(struct rtgl_context* ctx, struct rtgl_buffer_storage* source, usize offset, struct rtgl_image_base* image, rt_texture_range range);
void rtgl_execution_texture_read(struct rtgl_context* ctx, struct rtgl_image_base* image, rt_texture_range range, u08* data, usize data_size);

struct gl_surface* rtgl_execution_glfw_surface_create(struct rtgl_context* ctx, struct GLFWwindow* window);
void rtgl_execution_surface_destroy(struct rtgl_context* ctx, struct gl_surface* surface);

rt_timepoint rtgl_execution_present(struct rtgl_context* ctx, struct rtgl_queue* queue, struct rtgl_swapchain* swapchain, struct rtgl_framebuffer* framebuffer);
void rtgl_execution_queue_complete(struct rtgl_context* ctx, struct rtgl_queue* queue, u64 value);

#ifdef __cplusplus
}
#endif
#endif /* RTGL_EXECUTION_H */
