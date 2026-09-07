#ifndef RTVK_PROGRAM_H
#define RTVK_PROGRAM_H

#include "config.h"
#include "resource.h"

#include <volk.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTVK_API rt_program rtProgramCreate(void);
RTVK_API void rtProgramDestroy(rt_program program);

RTVK_API void rtProgramSetLayout(rt_program program, const rt_vertex_layout* layout);
RTVK_API void rtProgramSource(rt_program program, const char* entry_point, const u08* program_bytes, usize program_byte_size);
RTVK_API void rtProgramSetRasterState(rt_program program, enum rt_cull_mode cull_mode, enum rt_front_face front_face, enum rt_fill_mode fill_mode);
RTVK_API void rtProgramSetBlendState(rt_program program, bool enabled, enum rt_blend_factor src_color, enum rt_blend_factor dst_color, enum rt_blend_op color_op, enum rt_blend_factor src_alpha, enum rt_blend_factor dst_alpha, enum rt_blend_op alpha_op);
RTVK_API void rtProgramFinalize(rt_program program);
RTVK_API rt_location rtProgramUniformLocation(rt_program program, const char* name);
RTVK_API rt_location rtProgramInputLocation(rt_program program, const rt_vertex_attribute* attributes, usize attribute_count);
RTVK_API rt_location rtProgramOutputLocation(rt_program program, const char* name);

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rt_location_t {
	u08 address;
};

struct rtvk_program_input_mapping {
	u32 binding;
	u32 shader_location;
};

struct rtvk_program_output_mapping {
	u32 attachment;
	u32 shader_location;
};

typedef enum rtvk_program_descriptor_kind {
	RTVK_DESCRIPTOR_BUFFER,
	RTVK_DESCRIPTOR_STORAGE_BUFFER,
	RTVK_DESCRIPTOR_TEXTURE,
	RTVK_DESCRIPTOR_STORAGE_TEXTURE,
	RTVK_DESCRIPTOR_SAMPLER,
} rtvk_program_descriptor_kind;

struct rtvk_program_descriptor_mapping {
	VkShaderStageFlags stages;
	rtvk_program_descriptor_kind kind;
	u32 binding;
	bool sampled_alias;
	u32 sampled_binding;
};

typedef enum rtvk_program_data_kind {
	RTVK_DATA_UNIFORM,
	RTVK_DATA_STORAGE,
} rtvk_program_data_kind;

struct rtvk_program_data_mapping {
	rtvk_program_data_kind kind;
	u32 binding;
	usize byte_offset;
	usize byte_size;
	usize block_size;
};

struct rtvk_program_shader {
	VkShaderModule vk_shader;
	VkShaderStageFlagBits stage;
	char entry_point[RTVK_MAX_SHADER_UNIFORM_NAME];
};

struct rtvk_graphics_pipeline_variant {
	struct rtvk_graphics_pipeline_variant* next;
	VkPipeline vk_pipeline;
	VkFormat color_formats[RTVK_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS];
	VkFormat depth_format;
	VkFormat stencil_format;
	VkSampleCountFlagBits rasterization_samples;
	u32 color_format_count;
};

struct rtvk_program {
	struct rtvk_resource_base base;
	struct rtvk_program_shader* shaders;
	VkDescriptorSetLayout vk_descriptor_set_layout;
	VkPipelineLayout vk_pipeline_layout;
	VkPipeline vk_compute_pipeline;
	struct rtvk_graphics_pipeline_variant* pipeline_variants;

	char* entry_point;
	u08* program_bytes;

	rt_vertex_layout vertex_layout;
	rt_vertex_input vertex_inputs[RTVK_MAX_VERTEX_ATTRIBUTES];
	rt_vertex_attribute vertex_attributes[RTVK_MAX_VERTEX_ATTRIBUTES];
	struct rt_location_t locations[256];
	char location_names[256][RTVK_MAX_SHADER_UNIFORM_NAME];
	struct rtvk_program_input_mapping input_mappings[256];
	struct rtvk_program_output_mapping output_mappings[256];
	struct rtvk_program_descriptor_mapping descriptor_mappings[256];
	struct rtvk_program_data_mapping data_mappings[256];
	bool location_occupied[256];
	bool input_mapping_occupied[256];
	bool output_mapping_occupied[256];
	bool descriptor_mapping_occupied[256];
	bool data_mapping_occupied[256];
	enum rt_cull_mode cull_mode;
	enum rt_front_face front_face;
	enum rt_fill_mode fill_mode;
	bool blend_enabled;
	enum rt_blend_factor src_color_blend;
	enum rt_blend_factor dst_color_blend;
	enum rt_blend_op color_blend_op;
	enum rt_blend_factor src_alpha_blend;
	enum rt_blend_factor dst_alpha_blend;
	enum rt_blend_op alpha_blend_op;

	usize program_byte_size;
	usize vertex_attribute_count;
	u32 shader_count;
	u32 shader_capacity;
	u32 location_count;
	u32 tessellation_control_points;
	bool compute_program;
};
RTVK_DECLARE_NEW_RESOURCE(program)

VkPipeline rtvk_program_prepare(struct rtvk_context* ctx, struct rtvk_program* program, const VkFormat* color_formats, u32 color_format_count, VkFormat depth_format, VkFormat stencil_format, VkSampleCountFlagBits rasterization_samples);
void rtvk_program_layout(struct rtvk_context* ctx, struct rtvk_program* program, const rt_vertex_layout* layout);
void rtvk_program_source(struct rtvk_context* ctx, struct rtvk_program* program, const char* entry_point, const u08* program_bytes, usize program_byte_size);
void rtvk_program_raster_state(struct rtvk_context* ctx, struct rtvk_program* program, enum rt_cull_mode cull_mode, enum rt_front_face front_face, enum rt_fill_mode fill_mode);
void rtvk_program_blend_state(struct rtvk_context* ctx, struct rtvk_program* program, bool enabled, enum rt_blend_factor src_color, enum rt_blend_factor dst_color, enum rt_blend_op color_op, enum rt_blend_factor src_alpha, enum rt_blend_factor dst_alpha, enum rt_blend_op alpha_op);
void rtvk_program_finalize(struct rtvk_context* ctx, struct rtvk_program* program);
rt_location rtvk_program_uniform_location(struct rtvk_program* program, const char* name);
rt_location rtvk_program_input_location(struct rtvk_program* program, const rt_vertex_attribute* attributes, usize attribute_count);
rt_location rtvk_program_output_location(struct rtvk_program* program, const char* name);
struct rt_location_t* rtvk_program_find_location(struct rtvk_program* program, const char* name);
struct rtvk_program_input_mapping* rtvk_program_input_mapping(struct rtvk_program* program, const struct rt_location_t* location);
struct rtvk_program_output_mapping* rtvk_program_output_mapping(struct rtvk_program* program, const struct rt_location_t* location);
struct rtvk_program_descriptor_mapping* rtvk_program_descriptor_mapping(struct rtvk_program* program, const struct rt_location_t* location);
struct rtvk_program_data_mapping* rtvk_program_data_mapping(struct rtvk_program* program, const struct rt_location_t* location);
struct rtvk_program* rtvk_location_program(const struct rt_location_t* location);
struct rt_location_t* rtvk_program_add_location(struct rtvk_program* program, const char* name);
bool rtvk_program_descriptor_is_first(const struct rtvk_program* program, u32 address);
VkDescriptorType rtvk_program_descriptor_type(rtvk_program_descriptor_kind kind);
void rtvk_program_clear_locations(struct rtvk_program* program);
void rtvk_program_destroy_pipeline(struct rtvk_context* ctx, struct rtvk_program* program);
void rtvk_program_destroy_pipeline_layout(struct rtvk_context* ctx, struct rtvk_program* program);

#endif
