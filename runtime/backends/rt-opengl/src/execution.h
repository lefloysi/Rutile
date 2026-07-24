#ifndef RTGL_EXECUTION_H
#define RTGL_EXECUTION_H

#include "platform/context.h"
#include "resource/resource.h"
#include "sync.h"

#ifdef __cplusplus
extern "C" {
#endif

struct rtgl_execution_context {
	struct gl_context* gl_context;
	u08 gl_major;
	u08 gl_minor;
	bool direct_state_access;
	bool texture_storage;
	bool texture_buffer;
	bool texture_buffer_range;
	bool separate_shader_objects;
	bool shader_storage_buffer;
	bool spirv;
	struct rt_thread* thread;
	struct rt_event* ready_event;
	struct rt_event* stop_event;
	struct rt_event* work_event;
	struct rt_mutex* work_lock;
	struct rtgl_execution_command* work_first;
	struct rtgl_execution_command* work_last;
	unsigned thread_id;
	bool stopping;
};

bool rtgl_execution_init(struct rtgl_context* ctx);
void rtgl_execution_finish(struct rtgl_context* ctx);
struct gl_context* rtgl_execution_gl_context(struct rtgl_context* ctx);
bool rtgl_execution_is_thread(struct rtgl_context* ctx);
void rtgl_execution_lock(struct rtgl_context* ctx);
void rtgl_execution_unlock(struct rtgl_context* ctx);

void rtgl_execution_buffer_create(struct rtgl_context* ctx, struct rtgl_buffer* buffer);
void rtgl_execution_buffer_delete(struct rtgl_context* ctx, struct rtgl_buffer* buffer);
void rtgl_execution_buffer_data(struct rtgl_context* ctx, struct rtgl_buffer* buffer, enum rt_buffer_mode mode, enum rt_buffer_usage usage, u64 size, const u08* bytes);
void rtgl_execution_buffer_subdata(struct rtgl_context* ctx, struct rtgl_buffer* buffer, u64 offset, u64 size, const u08* bytes);
void rtgl_execution_buffer_read(struct rtgl_context* ctx, struct rtgl_buffer* buffer, u64 offset, u64 size, u08* bytes);

void rtgl_execution_framebuffer_create(struct rtgl_context* ctx, struct rtgl_framebuffer* framebuffer);
void rtgl_execution_framebuffer_delete(struct rtgl_context* ctx, struct rtgl_framebuffer* framebuffer);
void rtgl_execution_framebuffer_attach_color(struct rtgl_context* ctx, struct rtgl_framebuffer* framebuffer, u32 slot, struct rtgl_texture_view* view);
void rtgl_execution_framebuffer_attach_depth(struct rtgl_context* ctx, struct rtgl_framebuffer* framebuffer, struct rtgl_texture_view* view);

void rtgl_execution_graphics_program_finalize(struct rtgl_context* ctx, struct rtgl_graphics_program* program);
void rtgl_execution_graphics_program_destroy(struct rtgl_context* ctx, struct rtgl_graphics_program* program);

void rtgl_execution_texture_create(struct rtgl_context* ctx, struct rtgl_image_base* image);
void rtgl_execution_texture_delete(struct rtgl_context* ctx, struct rtgl_image_base* image);
void rtgl_execution_texture_data(struct rtgl_context* ctx, struct rtgl_image_base* image, const void* data);
void rtgl_execution_texture_subdata(struct rtgl_context* ctx, struct rtgl_image_base* image, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, u32 width, u32 height, u32 depth, const void* data);

struct gl_surface* rtgl_execution_glfw_surface_create(struct rtgl_context* ctx, struct GLFWwindow* window);
void rtgl_execution_surface_destroy(struct rtgl_context* ctx, struct gl_surface* surface);

struct rtgl_timepoint rtgl_execution_present(struct rtgl_context* ctx, struct rtgl_queue* queue, struct rtgl_swapchain* swapchain, struct rtgl_framebuffer* framebuffer);
void rtgl_execution_queue_complete(struct rtgl_context* ctx, struct rtgl_queue* queue, u64 value);

#ifdef __cplusplus
}
#endif
#endif /* RTGL_EXECUTION_H */
