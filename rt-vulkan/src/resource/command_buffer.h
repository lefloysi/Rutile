#ifndef RTVK_COMMAND_BUFFER_H
#define RTVK_COMMAND_BUFFER_H

#include "buffer.h"
#include "compute_program.h"
#include "config.h"
#include "framebuffer.h"
#include "graphics_program.h"
#include "resource.h"
#include "texture.h"

#include <vulkan/vulkan.h>

#define RTVK_INITIAL_DESCRIPTOR_SETS_PER_POOL 64

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTVK_API rt_command_buffer rtCmdCreate(void);
RTVK_API void rtCmdDestroy(rt_command_buffer command_buffer);

RTVK_API void rtCmdBegin(rt_command_buffer command_buffer, rt_queue queue);
RTVK_API void rtCmdBeginRendering(rt_command_buffer command_buffer, rt_framebuffer framebuffer);
RTVK_API void rtCmdClearColor(rt_command_buffer command_buffer, u32 color_index, f32 r, f32 g, f32 b, f32 a);
RTVK_API void rtCmdClearDepth(rt_command_buffer command_buffer, f32 depth);
RTVK_API void rtCmdClearStencil(rt_command_buffer command_buffer, u32 stencil);
RTVK_API void rtCmdEndRendering(rt_command_buffer command_buffer);
RTVK_API void rtCmdEnd(rt_command_buffer command_buffer);

RTVK_API void rtCmdUseGraphicsProgram(rt_command_buffer command_buffer, rt_graphics_program program);
RTVK_API void rtCmdSetScissor(rt_command_buffer command_buffer, u32 x, u32 y, u32 width, u32 height);
RTVK_API void rtCmdUseComputeProgram(rt_command_buffer command_buffer, rt_compute_program program);
RTVK_API void rtCmdUniformBuffer(rt_command_buffer command_buffer, rt_uniform_location location, rt_buffer buffer, u64 offset, u64 size);
RTVK_API void rtCmdUniformTexture(rt_command_buffer command_buffer, rt_uniform_location location, rt_texture_view texture_view);
RTVK_API void rtCmdStorageBuffer(rt_command_buffer command_buffer, u32 binding, rt_buffer buffer, u64 offset, u64 size);
RTVK_API void rtCmdStorageTexture(rt_command_buffer command_buffer, u32 binding, rt_texture_view texture_view);
RTVK_API void rtCmdComputeBarrier(rt_command_buffer command_buffer);

RTVK_API void rtCmdBindVertexBuffer(rt_command_buffer command_buffer, rt_buffer buffer, u64 offset);

RTVK_API void rtCmdDraw(rt_command_buffer command_buffer, u32 vertex_count, u32 first_vertex);
RTVK_API void rtCmdDispatch(rt_command_buffer command_buffer, u32 group_count_x, u32 group_count_y, u32 group_count_z);

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

typedef enum rtvk_uniform_slot_kind {
	RTVK_UNIFORM_SLOT_EMPTY,
	RTVK_UNIFORM_SLOT_BUFFER,
	RTVK_UNIFORM_SLOT_TEXTURE,
	RTVK_UNIFORM_SLOT_STORAGE_BUFFER,
	RTVK_UNIFORM_SLOT_STORAGE_TEXTURE,
} rtvk_uniform_slot_kind;

typedef struct rtvk_uniform_slot {
	rtvk_uniform_slot_kind kind;
	union {
		struct {
			struct rtvk_buffer* node;
			u64 offset;
			u64 size;
		} buffer;
		struct {
			struct rtvk_texture_view* view;
		} texture;
	};
} rtvk_uniform_slot;

typedef struct rtvk_descriptor_pool_node {
	struct rtvk_descriptor_pool_node* next;
	VkDescriptorPool vk_pool;
	u32 max_sets;
	u32 allocated_sets;
	u32 descriptors_per_type;
} rtvk_descriptor_pool_node;

struct rtvk_command_buffer {
	struct rtvk_resource_base base;
	struct rtvk_command_buffer* active;
	struct rtvk_command_buffer* next;

	VkCommandPool vk_command_pool;
	VkCommandBuffer vk_command_buffer;
	VkDescriptorSet bound_descriptor_set;
	rtvk_descriptor_pool_node* descriptor_pools;
	rtvk_descriptor_pool_node* current_descriptor_pool;

	struct rtvk_queue* queue;
	struct rtvk_framebuffer* framebuffer;
	struct rtvk_graphics_program* graphics_program;
	struct rtvk_compute_program* compute_program;
	struct rtvk_buffer* vertex_buffer;
	struct rtvk_buffer* vertex_buffer_node;
	rtvk_uniform_slot* uniform_slots;

	struct rtvk_timepoint pending_timepoint;

	u32 uniform_slot_count;
	u32 family_index;
	bool recording;
	bool uniforms_dirty;
};
RTVK_DECLARE_NEW_RESOURCE(command_buffer)

void rtvk_command_buffer_begin(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer, struct rtvk_queue* queue);
void rtvk_command_buffer_begin_rendering(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer, struct rtvk_framebuffer* framebuffer);
void rtvk_command_buffer_clear_color(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer, u32 color_index, f32 r, f32 g, f32 b, f32 a);
void rtvk_command_buffer_clear_depth(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer, f32 depth);
void rtvk_command_buffer_clear_stencil(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer, u32 stencil);
void rtvk_command_buffer_end_rendering(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer);
void rtvk_command_buffer_end(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer);

void rtvk_command_buffer_use_graphics_program(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer, struct rtvk_graphics_program* program);
void rtvk_command_buffer_set_scissor(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer, u32 x, u32 y, u32 width, u32 height);
void rtvk_command_buffer_use_compute_program(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer, struct rtvk_compute_program* program);
void rtvk_command_buffer_uniform_buffer(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer, struct rtvk_uniform_location* location, struct rtvk_buffer* buffer, u64 offset, u64 size);
void rtvk_command_buffer_uniform_texture(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer, struct rtvk_uniform_location* location, struct rtvk_texture_view* texture_view);
void rtvk_command_buffer_storage_buffer(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer, u32 binding, struct rtvk_buffer* buffer, u64 offset, u64 size);
void rtvk_command_buffer_storage_texture(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer, u32 binding, struct rtvk_texture_view* texture_view);
void rtvk_command_buffer_compute_barrier(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer);

void rtvk_command_buffer_bind_vertex_buffer(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer, struct rtvk_buffer* buffer, u64 offset);

void rtvk_command_buffer_draw(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer, u32 vertex_count, u32 first_vertex);
void rtvk_command_buffer_dispatch(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer, u32 group_count_x, u32 group_count_y, u32 group_count_z);

bool rtvk_command_buffer_has_queue(struct rtvk_command_buffer* command_buffer);
void rtvk_command_buffer_node_retain(struct rtvk_command_buffer* command_buffer);
void rtvk_command_buffer_node_release(struct rtvk_command_buffer* command_buffer);

#endif
