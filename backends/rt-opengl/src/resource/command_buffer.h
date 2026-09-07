#ifndef RTGL_COMMAND_BUFFER_H
#define RTGL_COMMAND_BUFFER_H

#include "buffer.h"
#include "framebuffer.h"
#include "program.h"
#include "resource.h"
#include "sampler.h"
#include "texture.h"

RTGL_EXTERN_C_ENTER

RTGL_API rt_command_buffer rtCommandBufferCreate(void);
RTGL_API void rtCommandBufferDestroy(rt_command_buffer command_buffer);
RTGL_API void rtCommandBufferReset(rt_command_buffer command_buffer);
RTGL_API void rtCommandBufferBegin(rt_command_buffer command_buffer);
RTGL_API void rtCommandBufferContinue(rt_command_buffer command_buffer);
RTGL_API void rtCommandBufferContinueRendering(rt_command_buffer command_buffer);
RTGL_API void rtCommandBufferEnd(rt_command_buffer command_buffer);
RTGL_API void rtCmdExecute(rt_command_buffer command_buffer, rt_command_buffer secondary);
RTGL_API void rtCmdBeginRendering(rt_command_buffer command_buffer, rt_framebuffer framebuffer);
RTGL_API void rtCmdClearColor(rt_command_buffer command_buffer, rt_location location, f32 r, f32 g, f32 b, f32 a);
RTGL_API void rtCmdClearDepth(rt_command_buffer command_buffer, f32 depth);
RTGL_API void rtCmdClearStencil(rt_command_buffer command_buffer, usize stencil);
RTGL_API void rtCmdClear(rt_command_buffer command_buffer, enum rt_clear_flag attachments);
RTGL_API void rtCmdSetViewport(rt_command_buffer command_buffer, usize x, usize y, usize width, usize height, f32 min_depth, f32 max_depth);
RTGL_API void rtCmdUseProgram(rt_command_buffer command_buffer, rt_program program);
RTGL_API void rtCmdUniformData(rt_command_buffer command_buffer, rt_location location, const u08* data, usize size);
RTGL_API void rtCmdStorageData(rt_command_buffer command_buffer, rt_location location, const u08* data, usize size);
RTGL_API void rtCmdSetScissor(rt_command_buffer command_buffer, usize x, usize y, usize width, usize height);
RTGL_API void rtCmdBindBuffer(rt_command_buffer command_buffer, rt_location location, rt_buffer buffer, rt_buffer_range range);
RTGL_API void rtCmdBindTexture(rt_command_buffer command_buffer, rt_location location, rt_texture_view texture_view);
RTGL_API void rtCmdBindSampler(rt_command_buffer command_buffer, rt_location location, rt_sampler sampler);
RTGL_API void rtCmdVertexBuffer(rt_command_buffer command_buffer, rt_location location, rt_buffer buffer, rt_buffer_range range);
RTGL_API void rtCmdIndexBuffer(rt_command_buffer command_buffer, rt_buffer buffer, rt_buffer_range range, enum rt_index_format format);
RTGL_API void rtCmdDraw(rt_command_buffer command_buffer, usize vertex_count, usize first_vertex);
RTGL_API void rtCmdDrawInstanced(rt_command_buffer command_buffer, usize vertex_count, usize instance_count, usize first_vertex, usize first_instance);
RTGL_API void rtCmdDrawIndexed(rt_command_buffer command_buffer, usize index_count, usize first_index, usize vertex_offset);
RTGL_API void rtCmdDrawIndexedInstanced(rt_command_buffer command_buffer, usize index_count, usize instance_count, usize first_index, usize vertex_offset, usize first_instance);
RTGL_API void rtCmdDispatch(rt_command_buffer command_buffer, usize group_count_x, usize group_count_y, usize group_count_z);
RTGL_API void rtCmdEndRendering(rt_command_buffer command_buffer);
RTGL_API void rtCmdBufferData(rt_command_buffer command_buffer, rt_buffer buffer, rt_buffer_range range, const u08* data);
RTGL_API void rtCmdBufferCopy(rt_command_buffer command_buffer, rt_buffer src, rt_buffer_range src_range, rt_buffer dst, rt_buffer_range dst_range);
RTGL_API void rtCmdBufferCopyToTexture(rt_command_buffer command_buffer, rt_buffer src, rt_buffer_range src_range, rt_texture dst, rt_texture_range dst_range);
RTGL_API void rtCmdBufferBarrier(rt_command_buffer command_buffer, rt_buffer buffer, rt_buffer_range range, rt_access src, rt_access dst);
RTGL_API void rtCmdTextureCopy(rt_command_buffer command_buffer, rt_texture src, rt_texture_range src_range, rt_texture dst, rt_texture_range dst_range);
RTGL_API void rtCmdTextureData(rt_command_buffer command_buffer, rt_texture texture, rt_texture_range range, const u08* data);
RTGL_API void rtCmdTextureCopyToBuffer(rt_command_buffer command_buffer, rt_texture src, rt_texture_range src_range, rt_buffer dst, rt_buffer_range dst_range);
RTGL_API void rtCmdTextureBarrier(rt_command_buffer command_buffer, rt_texture texture, rt_texture_range range, rt_access src, rt_access dst);

RTGL_EXTERN_C_EXIT

typedef enum rtgl_recorded_command_kind {
	RTGL_RECORDED_COMMAND_BEGIN_RENDERING,
	RTGL_RECORDED_COMMAND_CLEAR_COLOR,
	RTGL_RECORDED_COMMAND_CLEAR_DEPTH,
	RTGL_RECORDED_COMMAND_CLEAR_STENCIL,
	RTGL_RECORDED_COMMAND_CLEAR,
	RTGL_RECORDED_COMMAND_SET_VIEWPORT,
	RTGL_RECORDED_COMMAND_USE_PROGRAM,
	RTGL_RECORDED_COMMAND_UNIFORM_DATA,
	RTGL_RECORDED_COMMAND_STORAGE_DATA,
	RTGL_RECORDED_COMMAND_SET_SCISSOR,
	RTGL_RECORDED_COMMAND_BIND_BUFFER,
	RTGL_RECORDED_COMMAND_BIND_TEXTURE,
	RTGL_RECORDED_COMMAND_BIND_SAMPLER,
	RTGL_RECORDED_COMMAND_VERTEX_BUFFER,
	RTGL_RECORDED_COMMAND_INDEX_BUFFER,
	RTGL_RECORDED_COMMAND_DRAW,
	RTGL_RECORDED_COMMAND_DRAW_INSTANCED,
	RTGL_RECORDED_COMMAND_DRAW_INDEXED,
	RTGL_RECORDED_COMMAND_DRAW_INDEXED_INSTANCED,
	RTGL_RECORDED_COMMAND_DISPATCH,
	RTGL_RECORDED_COMMAND_END_RENDERING,
	RTGL_RECORDED_COMMAND_BUFFER_DATA,
	RTGL_RECORDED_COMMAND_BUFFER_COPY,
	RTGL_RECORDED_COMMAND_BUFFER_COPY_TO_TEXTURE,
	RTGL_RECORDED_COMMAND_BUFFER_BARRIER,
	RTGL_RECORDED_COMMAND_TEXTURE_DATA,
	RTGL_RECORDED_COMMAND_TEXTURE_COPY,
	RTGL_RECORDED_COMMAND_TEXTURE_COPY_TO_BUFFER,
	RTGL_RECORDED_COMMAND_TEXTURE_BARRIER,
} rtgl_recorded_command_kind;
typedef struct rtgl_recorded_command {
	rtgl_recorded_command_kind kind;
	u32 size;
	union {
		struct {
			struct rtgl_framebuffer* framebuffer;
			struct rtgl_image_base* color_images[RTGL_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS];
			struct rtgl_image_base* color_copy_sources[RTGL_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS];
			struct rtgl_image_base* depth_image;
			struct rtgl_image_base* depth_copy_source;
			struct rtgl_image_base* stencil_image;
			struct rtgl_image_base* stencil_copy_source;
		} begin_rendering;
		struct {
			enum rt_clear_flag attachments;
			f32 colors[RTGL_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS][4];
			f32 depth;
			usize stencil;
		} clear;
		struct {
			usize x;
			usize y;
			usize width;
			usize height;
			f32 min_depth;
			f32 max_depth;
		} set_viewport;
		struct {
			struct rtgl_program* program;
		} use_program;
		struct {
			u08 address;
			u08* bytes;
			usize size;
		} program_data;
		struct {
			usize x;
			usize y;
			usize width;
			usize height;
		} set_scissor;
		struct {
			u08 address;
			struct rtgl_buffer_storage* storage;
			u64 offset;
			u64 size;
		} bind_buffer;
		struct {
			u08 address;
			struct rtgl_texture_view* texture_view;
			struct rtgl_image_base* image;
			enum rt_filter mag_filter;
			enum rt_filter min_filter;
			enum rt_mip_filter mip_filter;
			enum rt_address_mode address_u;
			enum rt_address_mode address_v;
			enum rt_address_mode address_w;
			u32 max_anisotropy;
			f32 min_lod;
			f32 max_lod;
			f32 lod_bias;
		} bind_texture;
		struct {
			u08 address;
			enum rt_filter mag_filter;
			enum rt_filter min_filter;
			enum rt_mip_filter mip_filter;
			enum rt_address_mode address_u;
			enum rt_address_mode address_v;
			enum rt_address_mode address_w;
			u32 max_anisotropy;
			f32 min_lod;
			f32 max_lod;
			f32 lod_bias;
		} bind_sampler;
		struct {
			u08 address;
			struct rtgl_buffer_storage* storage;
			u64 offset;
			u64 size;
		} vertex_buffer;
		struct {
			struct rtgl_buffer_storage* storage;
			u64 offset;
			u64 size;
			enum rt_index_format format;
		} index_buffer;
		struct {
			usize vertex_count;
			usize first_vertex;
		} draw;
		struct {
			usize vertex_count;
			usize instance_count;
			usize first_vertex;
			usize first_instance;
		} draw_instanced;
		struct {
			usize index_count;
			usize first_index;
			usize vertex_offset;
		} draw_indexed;
		struct {
			usize index_count;
			usize instance_count;
			usize first_index;
			usize vertex_offset;
			usize first_instance;
		} draw_indexed_instanced;
		struct {
			usize group_count_x;
			usize group_count_y;
			usize group_count_z;
		} dispatch;
		struct {
			struct rtgl_buffer_storage* storage;
			struct rtgl_buffer_storage* copy_source;
			rt_buffer_range range;
			u08* data;
		} buffer_data;
		struct {
			struct rtgl_buffer_storage* src;
			rt_buffer_range src_range;
			struct rtgl_buffer_storage* dst;
			struct rtgl_buffer_storage* dst_copy_source;
			rt_buffer_range dst_range;
		} buffer_copy;
		struct {
			struct rtgl_buffer_storage* src;
			rt_buffer_range src_range;
			struct rtgl_image_base* dst;
			struct rtgl_image_base* copy_source;
			rt_texture_range dst_range;
		} buffer_copy_to_texture;
		struct {
			rt_access src;
			rt_access dst;
		} barrier;
		struct {
			struct rtgl_image_base* image;
			struct rtgl_image_base* copy_source;
			rt_texture_range range;
			u08* data;
		} texture_data;
		struct {
			struct rtgl_image_base* src;
			rt_texture_range src_range;
			struct rtgl_image_base* dst;
			struct rtgl_image_base* dst_copy_source;
			rt_texture_range dst_range;
		} texture_copy;
		struct {
			struct rtgl_image_base* src;
			rt_texture_range src_range;
			struct rtgl_buffer_storage* dst;
			struct rtgl_buffer_storage* dst_copy_source;
			rt_buffer_range dst_range;
		} texture_copy_to_buffer;
	} data;
} rtgl_recorded_command;

struct rtgl_command_buffer {
	struct rtgl_resource_base base;
	rtgl_recorded_command* commands;
	u32 command_count;
	u32 command_capacity;
	bool recording;
	bool executable;
	bool continuation;
	bool rendering_continuation;
	bool rendering;
	f32 clear_colors[RTGL_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS][4];
	f32 clear_depth;
	usize clear_stencil;
};

RTGL_EXTERN_C_ENTER
RTGL_DECLARE_NEW_RESOURCE(command_buffer)
rtgl_recorded_command* rtgl_command_buffer_append(struct rtgl_command_buffer* command_buffer);
void rtgl_command_buffer_release_command(struct rtgl_command_buffer* command_buffer, rtgl_recorded_command* command);
void rtgl_command_buffer_clear_commands(struct rtgl_command_buffer* command_buffer);
rt_timepoint rtgl_command_buffer_submit(struct rtgl_context* ctx, struct rtgl_queue* queue, struct rtgl_command_buffer* command_buffer);
void rtgl_command_buffer_execute(struct rtgl_context* ctx, struct rtgl_command_buffer* command_buffer, struct rtgl_queue* queue, u64 complete_value);
void rtgl_command_buffer_program_data(struct rtgl_command_buffer* command_buffer, struct rt_location_t* location, const u08* data, usize size, rtgl_location_kind expected_kind, rtgl_recorded_command_kind command_kind);

RTGL_EXTERN_C_EXIT

#endif /* RTGL_COMMAND_BUFFER_H */
