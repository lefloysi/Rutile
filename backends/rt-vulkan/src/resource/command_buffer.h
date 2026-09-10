#ifndef RTVK_COMMAND_BUFFER_H
#define RTVK_COMMAND_BUFFER_H

#include "buffer.h"
#include "config.h"
#include "framebuffer.h"
#include "program.h"
#include "resource.h"
#include "sampler.h"
#include "texture.h"

#include <volk.h>

/*===============================================================================================*/
/*                                                                                                */
/*===============================================================================================*/

RTVK_API rt_command_buffer rtCommandBufferCreate(void);
RTVK_API void rtCommandBufferDestroy(rt_command_buffer command_buffer);
RTVK_API void rtCommandBufferReset(rt_command_buffer command_buffer);
RTVK_API void rtCommandBufferBegin(rt_command_buffer command_buffer);
RTVK_API void rtCommandBufferContinue(rt_command_buffer command_buffer);
RTVK_API void rtCommandBufferContinueRendering(rt_command_buffer command_buffer);
RTVK_API void rtCommandBufferEnd(rt_command_buffer command_buffer);
RTVK_API void rtCmdExecute(rt_command_buffer command_buffer, rt_command_buffer secondary);
RTVK_API void rtCmdBufferData(rt_command_buffer command_buffer, rt_buffer buffer, rt_buffer_range range, const u08* data);
RTVK_API void rtCmdBufferCopy(rt_command_buffer command_buffer, rt_buffer src, rt_buffer_range src_range, rt_buffer dst, rt_buffer_range dst_range);
RTVK_API void rtCmdBufferCopyToTexture(rt_command_buffer command_buffer, rt_buffer src, rt_buffer_range src_range, rt_texture dst, rt_texture_range dst_range);
RTVK_API void rtCmdBufferBarrier(rt_command_buffer command_buffer, rt_buffer buffer, rt_buffer_range range, rt_access src, rt_access dst);
RTVK_API void rtCmdTextureCopy(rt_command_buffer command_buffer, rt_texture src, rt_texture_range src_range, rt_texture dst, rt_texture_range dst_range);
RTVK_API void rtCmdTextureData(rt_command_buffer command_buffer, rt_texture texture, rt_texture_range range, const u08* data);
RTVK_API void rtCmdTextureCopyToBuffer(rt_command_buffer command_buffer, rt_texture src, rt_texture_range src_range, rt_buffer dst, rt_buffer_range dst_range);
RTVK_API void rtCmdTextureBarrier(rt_command_buffer command_buffer, rt_texture texture, rt_texture_range range, rt_access src, rt_access dst);
RTVK_API void rtCmdBeginRendering(rt_command_buffer command_buffer, rt_framebuffer framebuffer);
RTVK_API void rtCmdClearColor(rt_command_buffer command_buffer, rt_location location, f32 r, f32 g, f32 b, f32 a);
RTVK_API void rtCmdClearDepth(rt_command_buffer command_buffer, f32 depth);
RTVK_API void rtCmdClearStencil(rt_command_buffer command_buffer, usize stencil);
RTVK_API void rtCmdClear(rt_command_buffer command_buffer, enum rt_clear_flag attachments);
RTVK_API void rtCmdSetViewport(rt_command_buffer command_buffer, usize x, usize y, usize width, usize height, f32 min_depth, f32 max_depth);
RTVK_API void rtCmdSetScissor(rt_command_buffer command_buffer, usize x, usize y, usize width, usize height);
RTVK_API void rtCmdEndRendering(rt_command_buffer command_buffer);
RTVK_API void rtCmdUseProgram(rt_command_buffer command_buffer, rt_program program);
RTVK_API void rtCmdUniformData(rt_command_buffer command_buffer, rt_location location, const u08* data, usize size);
RTVK_API void rtCmdStorageData(rt_command_buffer command_buffer, rt_location location, const u08* data, usize size);
RTVK_API void rtCmdBindBuffer(rt_command_buffer command_buffer, rt_location location, rt_buffer buffer, rt_buffer_range range);
RTVK_API void rtCmdBindTexture(rt_command_buffer command_buffer, rt_location location, rt_texture_view texture_view);
RTVK_API void rtCmdBindSampler(rt_command_buffer command_buffer, rt_location location, rt_sampler sampler);
RTVK_API void rtCmdVertexBuffer(rt_command_buffer command_buffer, rt_location location, rt_buffer buffer, rt_buffer_range range);
RTVK_API void rtCmdIndexBuffer(rt_command_buffer command_buffer, rt_buffer buffer, rt_buffer_range range, enum rt_index_format format);
RTVK_API void rtCmdDraw(rt_command_buffer command_buffer, usize vertex_count, usize first_vertex);
RTVK_API void rtCmdDrawInstanced(rt_command_buffer command_buffer, usize vertex_count, usize instance_count, usize first_vertex, usize first_instance);
RTVK_API void rtCmdDrawIndexed(rt_command_buffer command_buffer, usize index_count, usize first_index, usize vertex_offset);
RTVK_API void rtCmdDrawIndexedInstanced(rt_command_buffer command_buffer, usize index_count, usize instance_count, usize first_index, usize vertex_offset, usize first_instance);
RTVK_API void rtCmdDispatch(rt_command_buffer command_buffer, usize group_count_x, usize group_count_y, usize group_count_z);

/*===============================================================================================*/
/*                                                                                                */
/*===============================================================================================*/

typedef enum rtvk_command_opcode {
	RTVK_COMMAND_BUFFER_DATA,
	RTVK_COMMAND_BUFFER_COPY,
	RTVK_COMMAND_BUFFER_COPY_TO_TEXTURE,
	RTVK_COMMAND_BUFFER_BARRIER,
	RTVK_COMMAND_TEXTURE_COPY,
	RTVK_COMMAND_TEXTURE_DATA,
	RTVK_COMMAND_TEXTURE_COPY_TO_BUFFER,
	RTVK_COMMAND_TEXTURE_BARRIER,
	RTVK_COMMAND_EXECUTE,
	RTVK_COMMAND_BEGIN_RENDERING,
	RTVK_COMMAND_CLEAR_COLOR,
	RTVK_COMMAND_CLEAR_DEPTH,
	RTVK_COMMAND_CLEAR_STENCIL,
	RTVK_COMMAND_CLEAR,
	RTVK_COMMAND_SET_VIEWPORT,
	RTVK_COMMAND_SET_SCISSOR,
	RTVK_COMMAND_END_RENDERING,
	RTVK_COMMAND_USE_PROGRAM,
	RTVK_COMMAND_UNIFORM_DATA,
	RTVK_COMMAND_STORAGE_DATA,
	RTVK_COMMAND_BIND_BUFFER,
	RTVK_COMMAND_BIND_TEXTURE,
	RTVK_COMMAND_BIND_SAMPLER,
	RTVK_COMMAND_VERTEX_BUFFER,
	RTVK_COMMAND_INDEX_BUFFER,
	RTVK_COMMAND_DRAW,
	RTVK_COMMAND_DRAW_INSTANCED,
	RTVK_COMMAND_DRAW_INDEXED,
	RTVK_COMMAND_DRAW_INDEXED_INSTANCED,
	RTVK_COMMAND_DISPATCH,
} rtvk_command_opcode;

struct rtvk_command_header {
	void* alignment;
	u08 opcode;
};

struct rtvk_ir_buffer_data {
	struct rtvk_buffer* copy_source;
	struct rtvk_buffer* buffer;
	rt_buffer_range range;
	u08* data;
};

struct rtvk_ir_buffer_copy {
	struct rtvk_buffer* copy_source;
	struct rtvk_buffer* src;
	struct rtvk_buffer* dst;
	rt_buffer_range src_range;
	rt_buffer_range dst_range;
};

struct rtvk_ir_buffer_copy_to_texture {
	struct rtvk_buffer* src;
	struct rtvk_texture* dst;
	rt_buffer_range src_range;
	rt_texture_range dst_range;
};

struct rtvk_ir_buffer_barrier {
	struct rtvk_buffer* buffer;
	rt_buffer_range range;
	rt_access src;
	rt_access dst;
};

struct rtvk_ir_texture_copy {
	struct rtvk_texture* copy_source;
	struct rtvk_texture* src;
	struct rtvk_texture* dst;
	rt_texture_range src_range;
	rt_texture_range dst_range;
};

struct rtvk_ir_texture_data {
	struct rtvk_texture* copy_source;
	struct rtvk_texture* texture;
	rt_texture_range range;
	u08* data;
	usize data_size;
};

struct rtvk_ir_texture_copy_to_buffer {
	struct rtvk_texture* src;
	struct rtvk_buffer* copy_source;
	struct rtvk_buffer* dst;
	rt_texture_range src_range;
	rt_buffer_range dst_range;
};

struct rtvk_ir_texture_barrier {
	struct rtvk_texture* texture;
	rt_texture_range range;
	rt_access src;
	rt_access dst;
};

struct rtvk_ir_execute {
	struct rtvk_command_buffer* command_buffer;
};

struct rtvk_ir_framebuffer {
	struct rtvk_framebuffer* framebuffer;
};

struct rtvk_ir_clear_color {
	u32 index;
	f32 r;
	f32 g;
	f32 b;
	f32 a;
};

struct rtvk_ir_clear_depth {
	f32 depth;
};

struct rtvk_ir_clear_stencil {
	u32 stencil;
};

struct rtvk_ir_clear {
	enum rt_clear_flag attachments;
};

struct rtvk_ir_viewport {
	u32 x;
	u32 y;
	u32 width;
	u32 height;
	f32 min_depth;
	f32 max_depth;
};

struct rtvk_ir_scissor {
	u32 x;
	u32 y;
	u32 width;
	u32 height;
};

struct rtvk_ir_program {
	struct rtvk_program* program;
};

struct rtvk_ir_program_data {
	u08 address;
	u08* bytes;
	usize size;
};

struct rtvk_ir_buffer {
	u08 address;
	struct rtvk_buffer* buffer;
	usize offset;
	usize size;
};

struct rtvk_ir_texture {
	u08 address;
	struct rtvk_texture_view* view;
	struct rtvk_image_base* image;
	VkImageView vk_image_view;
	bool owns_image_view;
};

struct rtvk_ir_sampler {
	u08 address;
	struct rtvk_sampler* sampler;
	VkSampler vk_sampler;
};

struct rtvk_ir_vertex_buffer {
	u08 address;
	struct rtvk_buffer* buffer;
	rt_buffer_range range;
};

struct rtvk_ir_index_buffer {
	struct rtvk_buffer* buffer;
	rt_buffer_range range;
	enum rt_index_format format;
};

struct rtvk_ir_draw {
	u32 count;
	u32 first;
};

struct rtvk_ir_draw_instanced {
	u32 count;
	u32 instances;
	u32 first;
	u32 first_instance;
};

struct rtvk_ir_draw_indexed {
	u32 count;
	u32 first;
	i32 vertex_offset;
};

struct rtvk_ir_draw_indexed_instanced {
	u32 count;
	u32 instances;
	u32 first;
	i32 vertex_offset;
	u32 first_instance;
};

struct rtvk_ir_dispatch {
	u32 group_count_x;
	u32 group_count_y;
	u32 group_count_z;
};

struct rtvk_command_buffer {
	struct rtvk_resource_base base;
	u08* ir_data;
	usize ir_size;
	usize ir_capacity;
	bool recording;
	bool executable;
	bool continuation;
	bool rendering_continuation;
	bool rendering;
	bool lowering;
	bool checking_references;
};
RTVK_DECLARE_NEW_RESOURCE(command_buffer)

struct rtvk_lowered_staging_buffer {
	struct rtvk_lowered_staging_buffer* next;
	VkBuffer vk_buffer;
	VmaAllocation vma_allocation;
};

struct rtvk_lowered_resource_job {
	struct rtvk_resource_job base;
	struct rtvk_lowered_resource_job* next;
};

struct rtvk_lowered_image_view {
	VkImageView vk_image_view;
	struct rtvk_lowered_image_view* next;
};

struct rtvk_lowered_descriptor_pool {
	VkDescriptorPool vk_descriptor_pool;
	struct rtvk_lowered_descriptor_pool* next;
};

struct rtvk_lowered_command_buffer {
	struct rtvk_lowered_resource_job* resource_jobs;
	struct rtvk_lowered_staging_buffer* staging_buffers;
	struct rtvk_lowered_staging_buffer* program_data_buffer;
	usize program_data_capacity;
	usize program_data_used;
	usize program_data_alignment;
	struct rtvk_lowered_image_view* image_views;
	VkCommandBuffer vk_command_buffer;
	struct rtvk_lowered_descriptor_pool* descriptor_pools;
	VkCommandPool vk_command_pool;
};

void rtvk_command_buffer_reset(struct rtvk_command_buffer* command_buffer);
void rtvk_command_buffer_begin(struct rtvk_command_buffer* command_buffer);
void rtvk_command_buffer_continue(struct rtvk_command_buffer* command_buffer);
void rtvk_command_buffer_continue_rendering(struct rtvk_command_buffer* command_buffer);
void rtvk_command_buffer_execute(struct rtvk_command_buffer* command_buffer, struct rtvk_command_buffer* secondary);
void rtvk_command_buffer_buffer_data(struct rtvk_command_buffer* command_buffer, struct rtvk_buffer* buffer, rt_buffer_range range, const u08* data);
void rtvk_command_buffer_buffer_copy(struct rtvk_command_buffer* command_buffer, struct rtvk_buffer* src, rt_buffer_range src_range, struct rtvk_buffer* dst, rt_buffer_range dst_range);
void rtvk_command_buffer_buffer_copy_to_texture(struct rtvk_command_buffer* command_buffer, struct rtvk_buffer* src, rt_buffer_range src_range, struct rtvk_texture* dst, rt_texture_range dst_range);
void rtvk_command_buffer_buffer_barrier(struct rtvk_command_buffer* command_buffer, struct rtvk_buffer* buffer, rt_buffer_range range, rt_access src, rt_access dst);
void rtvk_command_buffer_texture_copy(struct rtvk_command_buffer* command_buffer, struct rtvk_texture* src, rt_texture_range src_range, struct rtvk_texture* dst, rt_texture_range dst_range);
void rtvk_command_buffer_texture_data(struct rtvk_command_buffer* command_buffer, struct rtvk_texture* texture, rt_texture_range range, const u08* data);
void rtvk_command_buffer_texture_copy_to_buffer(struct rtvk_command_buffer* command_buffer, struct rtvk_texture* src, rt_texture_range src_range, struct rtvk_buffer* dst, rt_buffer_range dst_range);
void rtvk_command_buffer_texture_barrier(struct rtvk_command_buffer* command_buffer, struct rtvk_texture* texture, rt_texture_range range, rt_access src, rt_access dst);
void rtvk_command_buffer_begin_rendering(struct rtvk_command_buffer* command_buffer, struct rtvk_framebuffer* framebuffer);
void rtvk_command_buffer_clear_color(struct rtvk_command_buffer* command_buffer, u32 index, f32 r, f32 g, f32 b, f32 a);
void rtvk_command_buffer_clear_depth(struct rtvk_command_buffer* command_buffer, f32 depth);
void rtvk_command_buffer_clear_stencil(struct rtvk_command_buffer* command_buffer, u32 stencil);
void rtvk_command_buffer_clear(struct rtvk_command_buffer* command_buffer, enum rt_clear_flag attachments);
void rtvk_command_buffer_set_viewport(struct rtvk_command_buffer* command_buffer, u32 x, u32 y, u32 width, u32 height, f32 min_depth, f32 max_depth);
void rtvk_command_buffer_set_scissor(struct rtvk_command_buffer* command_buffer, u32 x, u32 y, u32 width, u32 height);
void rtvk_command_buffer_end_rendering(struct rtvk_command_buffer* command_buffer);
void rtvk_command_buffer_use_program(struct rtvk_command_buffer* command_buffer, struct rtvk_program* program);
void rtvk_command_buffer_program_data(struct rtvk_command_buffer* command_buffer, rt_location location, const u08* data, usize size, rtvk_program_data_kind expected_kind, rtvk_command_opcode opcode);
void rtvk_command_buffer_bind_buffer(struct rtvk_command_buffer* command_buffer, rt_location location, struct rtvk_buffer* buffer, usize offset, usize size);
void rtvk_command_buffer_bind_texture(struct rtvk_command_buffer* command_buffer, rt_location location, struct rtvk_texture_view* view);
void rtvk_command_buffer_bind_sampler(struct rtvk_command_buffer* command_buffer, rt_location location, struct rtvk_sampler* sampler);
void rtvk_command_buffer_vertex_buffer(struct rtvk_command_buffer* command_buffer, rt_location location, struct rtvk_buffer* buffer, rt_buffer_range range);
void rtvk_command_buffer_index_buffer(struct rtvk_command_buffer* command_buffer, struct rtvk_buffer* buffer, rt_buffer_range range, enum rt_index_format format);
void rtvk_command_buffer_dispatch(struct rtvk_command_buffer* command_buffer, usize group_count_x, usize group_count_y, usize group_count_z);
void rtvk_command_buffer_draw(struct rtvk_command_buffer* command_buffer, u32 count, u32 first);
void rtvk_command_buffer_draw_instanced(struct rtvk_command_buffer* command_buffer, u32 count, u32 instances, u32 first, u32 first_instance);
void rtvk_command_buffer_draw_indexed(struct rtvk_command_buffer* command_buffer, u32 count, u32 first, i32 vertex_offset);
void rtvk_command_buffer_draw_indexed_instanced(struct rtvk_command_buffer* command_buffer, u32 count, u32 instances, u32 first, i32 vertex_offset, u32 first_instance);
void rtvk_command_buffer_end(struct rtvk_command_buffer* command_buffer);
void rtvk_command_buffer_release_resources(struct rtvk_command_buffer* command_buffer);

usize rtvk_command_record_size(rtvk_command_opcode opcode);
void rtvk_command_buffer_lower(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer, struct rtvk_lowered_command_buffer* lowered);
void rtvk_lowered_command_buffer_destroy(struct rtvk_context* ctx, struct rtvk_lowered_command_buffer* lowered);

#endif
