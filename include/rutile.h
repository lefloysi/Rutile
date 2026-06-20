#ifndef RUTILE_H
#define RUTILE_H

/*
 * rutile.h - the loader.
 */

#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <stddef.h>
#include <stdint.h>

#define RT_NULL_HANDLE NULL

#if !defined(RT_BUILD_DLL)
#define RT_EXPORT
#elif defined(_WIN32)
#define RT_EXPORT __declspec(dllexport)
#else
#define RT_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void* rt_proc_t;

typedef struct rt_proc_chain {
	rt_proc_t (*get_proc)(const struct rt_proc_chain* chain, const char* name);
} rt_proc_chain;

typedef const char* (*PFN_rtLayerGetName)(void);
typedef void (*PFN_rtLayerSetNext)(rt_proc_chain next);

#ifndef RUTILE_LOADER_ONLY

typedef uint8_t u08;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef size_t usize;
typedef uintptr_t uptr;

typedef int8_t i08;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;

enum rt_error { RT_ERROR__RESERVED = 0x7fffffff };
#define RT_SUCCESS ((enum rt_error)0)
#define RT_OUT_OF_HOST_MEMORY ((enum rt_error)1)
#define RT_OUT_OF_DEVICE_MEMORY ((enum rt_error)2)
#define RT_IMPROPER_USAGE ((enum rt_error)3)
#define RT_PLATFORM_FAILURE ((enum rt_error)4)
#define RT_DEVICE_LOST ((enum rt_error)5)
#define RT_ALREADY_INITIALIZED ((enum rt_error)6)
#define RT_UNSUPPORTED_PLATFORM ((enum rt_error)7)
#define RT_NO_BACKEND ((enum rt_error)8)
#define RT_UNSUPPORTED_FEATURE ((enum rt_error)9)
#define RT_INITIALIZATION_FAILED ((enum rt_error)10)
#define RT_LAYER_NOT_PRESENT ((enum rt_error)11)
#define RT_EXTENSION_NOT_PRESENT ((enum rt_error)12)
#define RT_INCOMPATIBLE_DRIVER ((enum rt_error)13)
#define RT_SHADER_COMPILATION_FAILED ((enum rt_error)14)
#define RT_SHADER_LINK_FAILED ((enum rt_error)15)

enum rt_format { RT_FORMAT__RESERVED = 0x7fffffff };
#define RT_FORMAT_UNKNOWN ((enum rt_format)0)
#define RT_R8_UNORM ((enum rt_format)1)
#define RT_RG8_UNORM ((enum rt_format)2)
#define RT_RGB8_UNORM ((enum rt_format)3)
#define RT_RGBA8_UNORM ((enum rt_format)4)
#define RT_R16_UNORM ((enum rt_format)5)
#define RT_RG16_UNORM ((enum rt_format)6)
#define RT_RGB16_UNORM ((enum rt_format)7)
#define RT_RGBA16_UNORM ((enum rt_format)8)
#define RT_R16_SFLOAT ((enum rt_format)9)
#define RT_RG16_SFLOAT ((enum rt_format)10)
#define RT_RGB16_SFLOAT ((enum rt_format)11)
#define RT_RGBA16_SFLOAT ((enum rt_format)12)
#define RT_R32_SFLOAT ((enum rt_format)13)
#define RT_RG32_SFLOAT ((enum rt_format)14)
#define RT_RGB32_SFLOAT ((enum rt_format)15)
#define RT_RGBA32_SFLOAT ((enum rt_format)16)
#define RT_R8_SINT ((enum rt_format)17)
#define RT_RG8_SINT ((enum rt_format)18)
#define RT_RGB8_SINT ((enum rt_format)19)
#define RT_RGBA8_SINT ((enum rt_format)20)
#define RT_R16_SINT ((enum rt_format)21)
#define RT_RG16_SINT ((enum rt_format)22)
#define RT_RGB16_SINT ((enum rt_format)23)
#define RT_RGBA16_SINT ((enum rt_format)24)
#define RT_R32_SINT ((enum rt_format)25)
#define RT_RG32_SINT ((enum rt_format)26)
#define RT_RGB32_SINT ((enum rt_format)27)
#define RT_RGBA32_SINT ((enum rt_format)28)
#define RT_R8_UINT ((enum rt_format)29)
#define RT_RG8_UINT ((enum rt_format)30)
#define RT_RGB8_UINT ((enum rt_format)31)
#define RT_RGBA8_UINT ((enum rt_format)32)
#define RT_R16_UINT ((enum rt_format)33)
#define RT_RG16_UINT ((enum rt_format)34)
#define RT_RGB16_UINT ((enum rt_format)35)
#define RT_RGBA16_UINT ((enum rt_format)36)
#define RT_R32_UINT ((enum rt_format)37)
#define RT_RG32_UINT ((enum rt_format)38)
#define RT_RGB32_UINT ((enum rt_format)39)
#define RT_RGBA32_UINT ((enum rt_format)40)
#define RT_D16_UNORM ((enum rt_format)41)
#define RT_D32_SFLOAT ((enum rt_format)42)
#define RT_S8_UINT ((enum rt_format)43)
#define RT_D24_UNORM_S8_UINT ((enum rt_format)44)
#define RT_D32_SFLOAT_S8_UINT ((enum rt_format)45)

enum rt_format_usage { RT_FORMAT_USAGE__RESERVED = 0x7fffffff };
#define RT_FORMAT_USAGE_NONE ((enum rt_format_usage)0x00)
#define RT_FORMAT_USAGE_SAMPLED ((enum rt_format_usage)0x01)
#define RT_FORMAT_USAGE_COLOR_ATTACHMENT ((enum rt_format_usage)0x02)
#define RT_FORMAT_USAGE_DEPTH_ATTACHMENT ((enum rt_format_usage)0x04)
#define RT_FORMAT_USAGE_STORAGE ((enum rt_format_usage)0x08)
#define RT_FORMAT_USAGE_TRANSFER_SRC ((enum rt_format_usage)0x10)
#define RT_FORMAT_USAGE_TRANSFER_DST ((enum rt_format_usage)0x20)

#define RT_FEATURE_PRESENTATION "RT_FEATURE_PRESENTATION"
#define RT_FEATURE_SWAPCHAIN_DEPTH_VIEW "RT_FEATURE_SWAPCHAIN_DEPTH_VIEW"

enum rt_buffer_mode { RT_BUFFER_MODE__RESERVED = 0x7fffffff };
#define RT_BUFFER_STATIC ((enum rt_buffer_mode)1)
#define RT_BUFFER_DYNAMIC ((enum rt_buffer_mode)2)

enum rt_buffer_usage { RT_BUFFER_USAGE__RESERVED = 0x7fffffff };
#define RT_BUFFER_USAGE_NONE ((enum rt_buffer_usage)0x00)
#define RT_BUFFER_USAGE_STAGING ((enum rt_buffer_usage)0x01)
#define RT_BUFFER_USAGE_VERTEX ((enum rt_buffer_usage)0x02)
#define RT_BUFFER_USAGE_INDEX ((enum rt_buffer_usage)0x04)
#define RT_BUFFER_USAGE_UNIFORM ((enum rt_buffer_usage)0x08)
#define RT_BUFFER_USAGE_STORAGE ((enum rt_buffer_usage)0x10)
#define RT_BUFFER_USAGE_TRANSFER_SRC ((enum rt_buffer_usage)0x20)
#define RT_BUFFER_USAGE_TRANSFER_DST ((enum rt_buffer_usage)0x40)

enum rt_texture_type { RT_TEXTURE_TYPE__RESERVED = 0x7fffffff };
#define RT_TEXTURE_UNKNOWN ((enum rt_texture_type)0)
#define RT_TEXTURE_1D ((enum rt_texture_type)1)
#define RT_TEXTURE_2D ((enum rt_texture_type)2)
#define RT_TEXTURE_3D ((enum rt_texture_type)3)
#define RT_TEXTURE_1D_ARRAY ((enum rt_texture_type)4)
#define RT_TEXTURE_2D_ARRAY ((enum rt_texture_type)5)

enum rt_filter { RT_FILTER__RESERVED = 0x7fffffff };
#define RT_FILTER_NEAREST ((enum rt_filter)1)
#define RT_FILTER_LINEAR ((enum rt_filter)2)

enum rt_mip_filter { RT_MIP_FILTER__RESERVED = 0x7fffffff };
#define RT_MIP_FILTER_NONE ((enum rt_mip_filter)0)
#define RT_MIP_FILTER_NEAREST ((enum rt_mip_filter)1)
#define RT_MIP_FILTER_LINEAR ((enum rt_mip_filter)2)

enum rt_address_mode { RT_ADDRESS_MODE__RESERVED = 0x7fffffff };
#define RT_ADDRESS_CLAMP ((enum rt_address_mode)1)
#define RT_ADDRESS_REPEAT ((enum rt_address_mode)2)
#define RT_ADDRESS_MIRROR ((enum rt_address_mode)3)

enum rt_queue_capability { RT_QUEUE_CAPABILITY__RESERVED = 0x7fffffff };
#define RT_QUEUE_TRANSFER ((enum rt_queue_capability)1)
#define RT_QUEUE_COMPUTE ((enum rt_queue_capability)2)
#define RT_QUEUE_GRAPHICS ((enum rt_queue_capability)3)

enum rt_index_type { RT_INDEX_TYPE__RESERVED = 0x7fffffff };
#define RT_INDEX_U16 ((enum rt_index_type)1)
#define RT_INDEX_U32 ((enum rt_index_type)2)

enum rt_cull_mode { RT_CULL_MODE__RESERVED = 0x7fffffff };
#define RT_CULL_NONE ((enum rt_cull_mode)0)
#define RT_CULL_FRONT ((enum rt_cull_mode)1)
#define RT_CULL_BACK ((enum rt_cull_mode)2)

enum rt_front_face { RT_FRONT_FACE__RESERVED = 0x7fffffff };
#define RT_FRONT_FACE_CCW ((enum rt_front_face)0)
#define RT_FRONT_FACE_CW ((enum rt_front_face)1)

enum rt_fill_mode { RT_FILL_MODE__RESERVED = 0x7fffffff };
#define RT_FILL_SOLID ((enum rt_fill_mode)0)
#define RT_FILL_WIREFRAME ((enum rt_fill_mode)1)

enum rt_compare_op { RT_COMPARE_OP__RESERVED = 0x7fffffff };
#define RT_COMPARE_NEVER ((enum rt_compare_op)0)
#define RT_COMPARE_LESS ((enum rt_compare_op)1)
#define RT_COMPARE_EQUAL ((enum rt_compare_op)2)
#define RT_COMPARE_LESS_EQUAL ((enum rt_compare_op)3)
#define RT_COMPARE_GREATER ((enum rt_compare_op)4)
#define RT_COMPARE_NOT_EQUAL ((enum rt_compare_op)5)
#define RT_COMPARE_GREATER_EQUAL ((enum rt_compare_op)6)
#define RT_COMPARE_ALWAYS ((enum rt_compare_op)7)

enum rt_blend_factor { RT_BLEND_FACTOR__RESERVED = 0x7fffffff };
#define RT_BLEND_ZERO ((enum rt_blend_factor)0)
#define RT_BLEND_ONE ((enum rt_blend_factor)1)
#define RT_BLEND_SRC_COLOR ((enum rt_blend_factor)2)
#define RT_BLEND_ONE_MINUS_SRC_COLOR ((enum rt_blend_factor)3)
#define RT_BLEND_DST_COLOR ((enum rt_blend_factor)4)
#define RT_BLEND_ONE_MINUS_DST_COLOR ((enum rt_blend_factor)5)
#define RT_BLEND_SRC_ALPHA ((enum rt_blend_factor)6)
#define RT_BLEND_ONE_MINUS_SRC_ALPHA ((enum rt_blend_factor)7)
#define RT_BLEND_DST_ALPHA ((enum rt_blend_factor)8)
#define RT_BLEND_ONE_MINUS_DST_ALPHA ((enum rt_blend_factor)9)

enum rt_blend_op { RT_BLEND_OP__RESERVED = 0x7fffffff };
#define RT_BLEND_OP_ADD ((enum rt_blend_op)0)
#define RT_BLEND_OP_SUBTRACT ((enum rt_blend_op)1)
#define RT_BLEND_OP_REVERSE_SUBTRACT ((enum rt_blend_op)2)
#define RT_BLEND_OP_MIN ((enum rt_blend_op)3)
#define RT_BLEND_OP_MAX ((enum rt_blend_op)4)

typedef struct rt_vertex_attribute {
	u32 location;
	u32 offset;
	enum rt_format format;
} rt_vertex_attribute;

typedef struct rt_vertex_layout {
	u32 stride;
	const rt_vertex_attribute* attributes;
	u32 attribute_count;
} rt_vertex_layout;

typedef struct rt_texture_t* rt_texture;
typedef struct rt_texture_view_t* rt_texture_view;
typedef struct rt_buffer_t* rt_buffer;
typedef struct rt_graphics_program_t* rt_graphics_program;
typedef struct rt_uniform_location_t* rt_uniform_location;
typedef struct rt_command_buffer_t* rt_command_buffer;
typedef struct rt_framebuffer_t* rt_framebuffer;
typedef struct rt_queue_t* rt_queue;

typedef struct rt_timepoint {
	rt_queue queue;
	u64 value;
} rt_timepoint;

typedef struct rt_extent_3d {
	u32 width;
	u32 height;
	u32 depth;
} rt_extent_3d;

/*!
** @brief Load an implementation and optional layers.
**
** @param backend_name Implementation name or library path.
** @param layer_names Optional ordered layer names.
** @param layer_count Number of entries in @p layer_names.
** @return RT_SUCCESS on success, otherwise the loader failure code.
**
** @note At most one implementation is loaded at a time.
** @note Core API calls may be made after rtLoaded returns true.
** @note Optional modules are loaded by their own extension headers.
*/
enum rt_error rtLoad(const char* backend_name, const char* const* layer_names, u32 layer_count);

/*!
** @brief Unload the current implementation and layers.
**
** @note Core API calls are unavailable after unload.
** @note Handles created before unload become invalid.
** @note It is valid to call rtUnload when nothing is loaded.
*/
void rtUnload(void);

/*!
** @brief Return whether Rutile currently has a loaded implementation.
**
** @return true when core API calls are available.
**
** @note A true result means core wrappers may be called.
** @note A false result means no implementation functions are available.
*/
bool rtLoaded(void);

/*!
** @brief Resolve a named API function.
**
** @param name Null-terminated procedure name.
** @return Function pointer, or NULL when the name is not available.
**
** @note Names are stable extension points.
** @note Optional functions may be absent.
*/
rt_proc_t rtGetProc(const char* name);

typedef void (*PFN_rtInit)(const char* const* features, u32 feature_count);
typedef void (*PFN_rtExit)(void);
typedef void (*PFN_rtOutput)(const char* message, void* user_data);
typedef void (*PFN_rtSetOutput)(PFN_rtOutput output, void* user_data);
typedef enum rt_error (*PFN_rtError)(void);
typedef const char* (*PFN_rtErrorMessage)(void);
typedef void (*PFN_rtClearError)(void);
typedef const char* (*PFN_rtGetName)(void);
typedef enum rt_format_usage (*PFN_rtQueryFormatCapabilities)(enum rt_format format);

typedef rt_buffer (*PFN_rtBufferCreate)(void);
typedef void (*PFN_rtBufferDestroy)(rt_buffer buffer);
typedef rt_timepoint (*PFN_rtBufferData)(rt_buffer buffer, enum rt_buffer_mode mode, enum rt_buffer_usage usage, u64 size, const void* data);
typedef rt_timepoint (*PFN_rtBufferSubdata)(rt_buffer buffer, u64 offset, u64 size, const void* data);
typedef void (*PFN_rtBufferRead)(rt_buffer buffer, u64 offset, u64 size, void* data);
typedef void* (*PFN_rtBufferMap)(rt_buffer buffer, u64 offset, u64 size);
typedef void (*PFN_rtBufferUnmap)(rt_buffer buffer);

typedef rt_texture (*PFN_rtTextureCreate)(void);
typedef void (*PFN_rtTextureDestroy)(rt_texture texture);
typedef rt_texture_view (*PFN_rtTextureViewCreate)(rt_texture texture);
typedef void (*PFN_rtTextureViewDestroy)(rt_texture_view texture_view);
typedef void (*PFN_rtTextureViewFilter)(rt_texture_view texture_view, enum rt_filter mag_filter, enum rt_filter min_filter, enum rt_mip_filter mip_filter);
typedef void (*PFN_rtTextureViewAddress)(rt_texture_view texture_view, enum rt_address_mode address_u, enum rt_address_mode address_v, enum rt_address_mode address_w);
typedef void (*PFN_rtTextureViewAnisotropy)(rt_texture_view texture_view, u32 max_anisotropy);
typedef void (*PFN_rtTextureViewLod)(rt_texture_view texture_view, f32 min_lod, f32 max_lod, f32 lod_bias);

typedef rt_timepoint (*PFN_rtTextureCopy)(rt_queue queue, rt_texture src_texture, u32 src_mip, rt_texture dst_texture, u32 dst_mip);
typedef rt_timepoint (*PFN_rtTextureData)(rt_queue queue, rt_texture texture, enum rt_texture_type type, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, enum rt_format format, const void* data);
typedef rt_timepoint (*PFN_rtTextureSubcopy)(rt_queue queue, rt_texture src_texture, u32 src_mip, u32 src_x, u32 src_y, u32 src_z, rt_texture dst_texture, u32 dst_mip, u32 dst_x, u32 dst_y, u32 dst_z, u32 width, u32 height, u32 depth);
typedef rt_timepoint (*PFN_rtTextureSubdata)(rt_queue queue, rt_texture texture, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, u32 width, u32 height, u32 depth, const void* data);
typedef rt_timepoint (*PFN_rtTextureViewCopyToBuffer)(rt_queue queue, rt_texture_view texture_view, rt_buffer buffer);
typedef rt_extent_3d (*PFN_rtTextureViewExtent)(rt_texture_view texture_view);
typedef rt_framebuffer (*PFN_rtFramebufferCreate)(void);
typedef void (*PFN_rtFramebufferDestroy)(rt_framebuffer framebuffer);
typedef rt_texture_view (*PFN_rtFramebufferColorView)(rt_framebuffer framebuffer, u32 slot);
typedef void (*PFN_rtFramebufferSetColorView)(rt_framebuffer framebuffer, u32 slot, rt_texture_view view);
typedef void (*PFN_rtFramebufferDepthView)(rt_framebuffer framebuffer, rt_texture_view view);

typedef rt_graphics_program (*PFN_rtGraphicsProgramCreate)(void);
typedef void (*PFN_rtGraphicsProgramDestroy)(rt_graphics_program program);
typedef void (*PFN_rtGraphicsProgramVertexLayout)(rt_graphics_program program, const rt_vertex_layout* layout);
typedef void (*PFN_rtGraphicsProgramVertexShader)(rt_graphics_program program, u64 size, const void* data);
typedef void (*PFN_rtGraphicsProgramFragmentShader)(rt_graphics_program program, u64 size, const void* data);
typedef void (*PFN_rtGraphicsProgramRasterState)(rt_graphics_program program, enum rt_cull_mode cull_mode, enum rt_front_face front_face, enum rt_fill_mode fill_mode);
typedef void (*PFN_rtGraphicsProgramBlendState)(rt_graphics_program program, bool enabled, enum rt_blend_factor src_color, enum rt_blend_factor dst_color, enum rt_blend_op color_op, enum rt_blend_factor src_alpha, enum rt_blend_factor dst_alpha, enum rt_blend_op alpha_op);
typedef void (*PFN_rtGraphicsProgramLink)(rt_graphics_program program);
typedef rt_uniform_location (*PFN_rtGraphicsProgramUniformLocation)(rt_graphics_program program, const char* name);

typedef rt_command_buffer (*PFN_rtCmdCreate)(void);
typedef void (*PFN_rtCmdDestroy)(rt_command_buffer command_buffer);
typedef void (*PFN_rtCmdBegin)(rt_command_buffer command_buffer, rt_queue queue);
typedef void (*PFN_rtCmdBeginRendering)(rt_command_buffer command_buffer, rt_framebuffer framebuffer);
typedef void (*PFN_rtCmdClearColor)(rt_command_buffer command_buffer, u32 color_index, f32 r, f32 g, f32 b, f32 a);
typedef void (*PFN_rtCmdClearDepth)(rt_command_buffer command_buffer, f32 depth);
typedef void (*PFN_rtCmdClearStencil)(rt_command_buffer command_buffer, u32 stencil);
typedef void (*PFN_rtCmdUseGraphicsProgram)(rt_command_buffer command_buffer, rt_graphics_program program);
typedef void (*PFN_rtCmdSetScissor)(rt_command_buffer command_buffer, u32 x, u32 y, u32 width, u32 height);
typedef void (*PFN_rtCmdUniformBuffer)(rt_command_buffer command_buffer, rt_uniform_location location, rt_buffer buffer, u64 offset, u64 size);
typedef void (*PFN_rtCmdUniformTexture)(rt_command_buffer command_buffer, rt_uniform_location location, rt_texture_view texture_view);
typedef void (*PFN_rtCmdBindVertexBuffer)(rt_command_buffer command_buffer, rt_buffer buffer, u64 offset);
typedef void (*PFN_rtCmdDraw)(rt_command_buffer command_buffer, u32 vertex_count, u32 first_vertex);
typedef void (*PFN_rtCmdEndRendering)(rt_command_buffer command_buffer);
typedef void (*PFN_rtCmdEnd)(rt_command_buffer command_buffer);

typedef rt_queue (*PFN_rtQueueQuery)(enum rt_queue_capability capability);
typedef void (*PFN_rtQueueWait)(rt_queue queue, rt_timepoint timepoint);
typedef rt_timepoint (*PFN_rtQueueSubmit)(rt_queue queue, rt_command_buffer command_buffer);
typedef rt_timepoint (*PFN_rtQueueFlush)(rt_queue queue);
typedef void (*PFN_rtTimepointWait)(rt_timepoint timepoint);
typedef bool (*PFN_rtTimepointReached)(rt_timepoint timepoint);

extern PFN_rtInit rt_rtInit;
extern PFN_rtExit rt_rtExit;
extern PFN_rtSetOutput rt_rtSetOutput;
extern PFN_rtError rt_rtError;
extern PFN_rtErrorMessage rt_rtErrorMessage;
extern PFN_rtClearError rt_rtClearError;
extern PFN_rtGetName rt_rtGetName;
extern PFN_rtQueryFormatCapabilities rt_rtQueryFormatCapabilities;

extern PFN_rtBufferCreate rt_rtBufferCreate;
extern PFN_rtBufferDestroy rt_rtBufferDestroy;
extern PFN_rtBufferData rt_rtBufferData;
extern PFN_rtBufferSubdata rt_rtBufferSubdata;
extern PFN_rtBufferRead rt_rtBufferRead;
extern PFN_rtBufferMap rt_rtBufferMap;
extern PFN_rtBufferUnmap rt_rtBufferUnmap;

extern PFN_rtTextureCreate rt_rtTextureCreate;
extern PFN_rtTextureDestroy rt_rtTextureDestroy;
extern PFN_rtTextureViewCreate rt_rtTextureViewCreate;
extern PFN_rtTextureViewDestroy rt_rtTextureViewDestroy;
extern PFN_rtTextureViewFilter rt_rtTextureViewFilter;
extern PFN_rtTextureViewAddress rt_rtTextureViewAddress;
extern PFN_rtTextureViewAnisotropy rt_rtTextureViewAnisotropy;
extern PFN_rtTextureViewLod rt_rtTextureViewLod;

extern PFN_rtTextureCopy rt_rtTextureCopy;
extern PFN_rtTextureData rt_rtTextureData;
extern PFN_rtTextureSubcopy rt_rtTextureSubcopy;
extern PFN_rtTextureSubdata rt_rtTextureSubdata;
extern PFN_rtTextureViewCopyToBuffer rt_rtTextureViewCopyToBuffer;
extern PFN_rtTextureViewExtent rt_rtTextureViewExtent;
extern PFN_rtFramebufferCreate rt_rtFramebufferCreate;
extern PFN_rtFramebufferDestroy rt_rtFramebufferDestroy;
extern PFN_rtFramebufferColorView rt_rtFramebufferColorView;
extern PFN_rtFramebufferSetColorView rt_rtFramebufferSetColorView;
extern PFN_rtFramebufferDepthView rt_rtFramebufferDepthView;

extern PFN_rtGraphicsProgramCreate rt_rtGraphicsProgramCreate;
extern PFN_rtGraphicsProgramDestroy rt_rtGraphicsProgramDestroy;
extern PFN_rtGraphicsProgramVertexLayout rt_rtGraphicsProgramVertexLayout;
extern PFN_rtGraphicsProgramVertexShader rt_rtGraphicsProgramVertexShader;
extern PFN_rtGraphicsProgramFragmentShader rt_rtGraphicsProgramFragmentShader;
extern PFN_rtGraphicsProgramRasterState rt_rtGraphicsProgramRasterState;
extern PFN_rtGraphicsProgramBlendState rt_rtGraphicsProgramBlendState;
extern PFN_rtGraphicsProgramLink rt_rtGraphicsProgramLink;
extern PFN_rtGraphicsProgramUniformLocation rt_rtGraphicsProgramUniformLocation;

extern PFN_rtCmdCreate rt_rtCmdCreate;
extern PFN_rtCmdDestroy rt_rtCmdDestroy;
extern PFN_rtCmdBegin rt_rtCmdBegin;
extern PFN_rtCmdBeginRendering rt_rtCmdBeginRendering;
extern PFN_rtCmdClearColor rt_rtCmdClearColor;
extern PFN_rtCmdClearDepth rt_rtCmdClearDepth;
extern PFN_rtCmdClearStencil rt_rtCmdClearStencil;
extern PFN_rtCmdUseGraphicsProgram rt_rtCmdUseGraphicsProgram;
extern PFN_rtCmdSetScissor rt_rtCmdSetScissor;
extern PFN_rtCmdUniformBuffer rt_rtCmdUniformBuffer;
extern PFN_rtCmdUniformTexture rt_rtCmdUniformTexture;
extern PFN_rtCmdBindVertexBuffer rt_rtCmdBindVertexBuffer;
extern PFN_rtCmdDraw rt_rtCmdDraw;
extern PFN_rtCmdEndRendering rt_rtCmdEndRendering;
extern PFN_rtCmdEnd rt_rtCmdEnd;

extern PFN_rtQueueQuery rt_rtQueueQuery;
extern PFN_rtQueueWait rt_rtQueueWait;
extern PFN_rtQueueSubmit rt_rtQueueSubmit;
extern PFN_rtQueueFlush rt_rtQueueFlush;
extern PFN_rtTimepointWait rt_rtTimepointWait;
extern PFN_rtTimepointReached rt_rtTimepointReached;

#ifndef RT_NO_API_WRAPPERS
/*!
** @brief Initialize Rutile.
**
** @param features Optional array of feature name strings.
** @param feature_count Number of entries in @p features.
**
** @note rtLoad must succeed before this function is called.
** @note Resources may be created after successful initialization.
*/
static inline void rtInit(const char* const* features, u32 feature_count) {
	rt_rtInit(features, feature_count);
}

/*!
** @brief Shut down Rutile.
**
** @note Public objects should be destroyed before rtExit.
** @note Handles from the previous initialization are invalid after rtExit.
*/
static inline void rtExit(void) {
	rt_rtExit();
}

/*!
** @brief Set the output callback.
**
** @param output Callback receiving output messages.
** @param user_data Opaque pointer passed back to @p output.
**
** @note Passing NULL restores the default output behavior.
** @note Rutile never owns @p user_data.
*/
static inline void rtSetOutput(PFN_rtOutput output, void* user_data) {
	rt_rtSetOutput(output, user_data);
}

#ifndef RUTILE_IMPL
/*!
** @brief Return the current error code.
**
** @return Current error code.
**
** @note RT_SUCCESS means no error is currently recorded.
** @note Reading the error does not clear it.
*/
static inline enum rt_error rtError(void) {
	return rt_rtError();
}

/*!
** @brief Return the current error message.
**
** @return Null-terminated message owned by Rutile.
**
** @note The returned pointer remains owned by Rutile.
** @note The message may be empty when the error code is sufficient.
*/
static inline const char* rtErrorMessage(void) {
	return rt_rtErrorMessage();
}

/*!
** @brief Clear the current error state.
**
** @note After this call, rtError returns RT_SUCCESS until another error is recorded.
*/
static inline void rtClearError(void) {
	rt_rtClearError();
}
#endif

/*!
** @brief Return the loaded implementation name.
**
** @return Static name string.
**
** @note The returned pointer is valid until unload.
*/
static inline const char* rtGetName(void) {
	return rt_rtGetName();
}

/*!
** @brief Query supported usage flags for a format.
**
** @param format Format to query.
** @return Bitset of supported format usages.
**
** @note Unknown or unsupported formats may return RT_FORMAT_USAGE_NONE.
*/
static inline enum rt_format_usage rtQueryFormatCapabilities(enum rt_format format) {
	return rt_rtQueryFormatCapabilities(format);
}

/*!
** @brief Create an unconfigured buffer handle.
**
** @return New buffer handle.
**
** @note Storage is defined later by rtBufferData.
** @note The handle remains valid until rtBufferDestroy is called.
*/
static inline rt_buffer rtBufferCreate(void) {
	return rt_rtBufferCreate();
}

/*!
** @brief Retire a buffer handle.
**
** @param buffer Buffer to destroy.
**
** @note Destruction ends the public lifetime of the buffer immediately.
** @note Work already submitted may continue to reference the zombie buffer.
*/
static inline void rtBufferDestroy(rt_buffer buffer) {
	rt_rtBufferDestroy(buffer);
}

/*!
** @brief Define buffer storage and upload initial data.
**
** @param buffer Buffer to configure.
** @param mode Storage/update mode for this buffer definition.
** @param usage Bitset of RT_BUFFER_USAGE_* flags for this buffer definition.
** @param size Size in bytes of the buffer storage.
** @param data Optional source data copied into the buffer.
** @return Queue timepoint for GPU upload work, or a completed/null timepoint for CPU copies.
**
** @note This configures an existing buffer; it does not create one.
** @note This call defines how the buffer memory is created.
** @note The backend selects any queue needed for GPU uploads.
*/
static inline rt_timepoint rtBufferData(rt_buffer buffer, enum rt_buffer_mode mode, enum rt_buffer_usage usage, u64 size, const void* data) {
	return rt_rtBufferData(buffer, mode, usage, size, data);
}

/*!
** @brief Upload data into an existing buffer range.
**
** @param buffer Destination buffer.
** @param offset Byte offset into @p buffer.
** @param size Number of bytes to upload.
** @param data Source data copied into the buffer range.
** @return Queue timepoint for GPU upload work, or a completed/null timepoint for CPU copies.
**
** @note The buffer storage must already exist.
** @note The upload range must fit within the buffer.
** @note The backend selects any queue needed for GPU uploads.
*/
static inline rt_timepoint rtBufferSubdata(rt_buffer buffer, u64 offset, u64 size, const void* data) {
	return rt_rtBufferSubdata(buffer, offset, size, data);
}

/*!
** @brief Copy bytes out of a CPU-visible buffer.
**
** @param buffer Source buffer.
** @param offset Byte offset into @p buffer.
** @param size Number of bytes to copy.
** @param data Destination memory.
*/
static inline void rtBufferRead(rt_buffer buffer, u64 offset, u64 size, void* data) {
	rt_rtBufferRead(buffer, offset, size, data);
}

/*!
** @brief Map buffer storage for direct CPU access.
**
** @param buffer Buffer to map.
** @param offset Byte offset into @p buffer.
** @param size Number of bytes to map.
** @return CPU pointer to the requested byte range, or NULL on failure.
*/
static inline void* rtBufferMap(rt_buffer buffer, u64 offset, u64 size) {
	return rt_rtBufferMap(buffer, offset, size);
}

/*!
** @brief Unmap a previously mapped buffer.
**
** @param buffer Buffer to unmap.
*/
static inline void rtBufferUnmap(rt_buffer buffer) {
	rt_rtBufferUnmap(buffer);
}

/*!
** @brief Create an unconfigured texture handle.
**
** @return New texture handle.
**
** @note Creation takes no configuration.
** @note The handle remains valid until rtTextureDestroy is called.
*/
static inline rt_texture rtTextureCreate(void) {
	return rt_rtTextureCreate();
}

/*!
** @brief Retire a texture handle.
**
** @param texture Texture to destroy.
**
** @note Destruction ends the public lifetime of the texture immediately.
** @note Work already submitted may continue to reference the zombie texture.
*/
static inline void rtTextureDestroy(rt_texture texture) {
	rt_rtTextureDestroy(texture);
}

/*!
** @brief Create a default view of a texture.
**
** @param texture Texture to view.
** @return Texture view handle, or NULL on failure.
**
** @note The view keeps the texture alive for the duration of the view.
*/
static inline rt_texture_view rtTextureViewCreate(rt_texture texture) {
	return rt_rtTextureViewCreate(texture);
}

/*!
** @brief Retire a texture view handle.
**
** @param texture_view Texture view to destroy.
*/
static inline void rtTextureViewDestroy(rt_texture_view texture_view) {
	rt_rtTextureViewDestroy(texture_view);
}

/*!
** @brief Set the filtering state used when sampling through a texture view.
**
** @param texture_view Texture view to update.
** @param mag_filter Magnification filter.
** @param min_filter Minification filter.
** @param mip_filter Mip filter.
*/
static inline void rtTextureViewFilter(rt_texture_view texture_view, enum rt_filter mag_filter, enum rt_filter min_filter, enum rt_mip_filter mip_filter) {
	rt_rtTextureViewFilter(texture_view, mag_filter, min_filter, mip_filter);
}

/*!
** @brief Set the address modes used when sampling through a texture view.
**
** @param texture_view Texture view to update.
** @param address_u Address mode for the U axis.
** @param address_v Address mode for the V axis.
** @param address_w Address mode for the W axis.
*/
static inline void rtTextureViewAddress(rt_texture_view texture_view, enum rt_address_mode address_u, enum rt_address_mode address_v, enum rt_address_mode address_w) {
	rt_rtTextureViewAddress(texture_view, address_u, address_v, address_w);
}

/*!
** @brief Set the anisotropy limit used when sampling through a texture view.
**
** @param texture_view Texture view to update.
** @param max_anisotropy Maximum anisotropy. Zero maps to the backend default.
*/
static inline void rtTextureViewAnisotropy(rt_texture_view texture_view, u32 max_anisotropy) {
	rt_rtTextureViewAnisotropy(texture_view, max_anisotropy);
}

/*!
** @brief Set the LOD state used when sampling through a texture view.
**
** @param texture_view Texture view to update.
** @param min_lod Minimum LOD.
** @param max_lod Maximum LOD.
** @param lod_bias LOD bias.
*/
static inline void rtTextureViewLod(rt_texture_view texture_view, f32 min_lod, f32 max_lod, f32 lod_bias) {
	rt_rtTextureViewLod(texture_view, min_lod, max_lod, lod_bias);
}

/*!
** @brief Copy one complete texture mip into another texture mip.
**
** @param queue Queue used for the copy operation.
** @param src_texture Source texture.
** @param src_mip Source mip level.
** @param dst_texture Destination texture.
** @param dst_mip Destination mip level.
** @return Timepoint for the copy operation.
**
** @note Source and destination storage must already exist.
*/
static inline rt_timepoint rtTextureCopy(rt_queue queue, rt_texture src_texture, u32 src_mip, rt_texture dst_texture, u32 dst_mip) {
	return rt_rtTextureCopy(queue, src_texture, src_mip, dst_texture, dst_mip);
}

/*!
** @brief Define texture storage and upload initial texture data.
**
** @param queue Queue used for the upload operation.
** @param texture Texture to configure.
** @param type Texture dimensionality.
** @param mip Mip level receiving data.
** @param offset_x X offset of the uploaded region.
** @param offset_y Y offset of the uploaded region.
** @param offset_z Z offset of the uploaded region.
** @param format Texture format.
** @param data Optional source texel data.
** @return Timepoint for the upload operation.
**
** @note This configures an existing texture; it does not create one.
*/
static inline rt_timepoint rtTextureData(rt_queue queue, rt_texture texture, enum rt_texture_type type, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, enum rt_format format, const void* data) {
	return rt_rtTextureData(queue, texture, type, mip, offset_x, offset_y, offset_z, format, data);
}

/*!
** @brief Copy a texture region into another texture region.
**
** @param queue Queue used for the copy operation.
** @param src_texture Source texture.
** @param src_mip Source mip level.
** @param src_x Source X offset.
** @param src_y Source Y offset.
** @param src_z Source Z offset.
** @param dst_texture Destination texture.
** @param dst_mip Destination mip level.
** @param dst_x Destination X offset.
** @param dst_y Destination Y offset.
** @param dst_z Destination Z offset.
** @param width Region width in texels.
** @param height Region height in texels.
** @param depth Region depth in texels.
** @return Timepoint for the copy operation.
**
** @note Source and destination storage must already exist.
** @note Source and destination regions must fit within their textures.
*/
static inline rt_timepoint rtTextureSubcopy(rt_queue queue, rt_texture src_texture, u32 src_mip, u32 src_x, u32 src_y, u32 src_z, rt_texture dst_texture, u32 dst_mip, u32 dst_x, u32 dst_y, u32 dst_z, u32 width, u32 height, u32 depth) {
	return rt_rtTextureSubcopy(queue, src_texture, src_mip, src_x, src_y, src_z, dst_texture, dst_mip, dst_x, dst_y, dst_z, width, height, depth);
}

/*!
** @brief Upload data into an existing texture region.
**
** @param queue Queue used for the upload operation.
** @param texture Destination texture.
** @param mip Destination mip level.
** @param offset_x Destination X offset.
** @param offset_y Destination Y offset.
** @param offset_z Destination Z offset.
** @param width Region width in texels.
** @param height Region height in texels.
** @param depth Region depth in texels.
** @param data Source texel data.
** @return Timepoint for the upload operation.
**
** @note Texture storage must already exist.
** @note The upload region must fit within the texture.
*/
static inline rt_timepoint rtTextureSubdata(rt_queue queue, rt_texture texture, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, u32 width, u32 height, u32 depth, const void* data) {
	return rt_rtTextureSubdata(queue, texture, mip, offset_x, offset_y, offset_z, width, height, depth, data);
}

/*!
** @brief Copy a texture view into a CPU-visible buffer.
**
** @param queue Queue used for the copy operation.
** @param texture_view Source texture view.
** @param buffer Destination buffer that will receive tightly packed pixels.
** @return Timepoint for the copy operation.
*/
static inline rt_timepoint rtTextureViewCopyToBuffer(rt_queue queue, rt_texture_view texture_view, rt_buffer buffer) {
	return rt_rtTextureViewCopyToBuffer(queue, texture_view, buffer);
}

/*!
** @brief Return the extent of a texture view.
**
** @param texture_view Texture view to query.
** @return Texture view extent, or zeros on failure.
*/
static inline rt_extent_3d rtTextureViewExtent(rt_texture_view texture_view) {
	return rt_rtTextureViewExtent(texture_view);
}

/*!
** @brief Create an empty framebuffer.
**
** @return New framebuffer handle.
*/
static inline rt_framebuffer rtFramebufferCreate(void) {
	return rt_rtFramebufferCreate();
}

/*!
** @brief Retire a framebuffer handle.
**
** @param framebuffer Framebuffer to destroy.
*/
static inline void rtFramebufferDestroy(rt_framebuffer framebuffer) {
	rt_rtFramebufferDestroy(framebuffer);
}

/*!
** @brief Return a framebuffer color attachment view by slot.
**
** @param framebuffer Framebuffer to query.
** @param slot Color attachment slot.
** @return Texture view at @p slot, or NULL when no view is bound there.
*/
static inline rt_texture_view rtFramebufferColorView(rt_framebuffer framebuffer, u32 slot) {
	return rt_rtFramebufferColorView(framebuffer, slot);
}

/*!
** @brief Set a framebuffer color attachment view.
**
** @param framebuffer Framebuffer to update.
** @param slot Color attachment slot.
** @param view Color texture view to bind, or NULL to remove the attachment.
*/
static inline void rtFramebufferSetColorView(rt_framebuffer framebuffer, u32 slot, rt_texture_view view) {
	rt_rtFramebufferSetColorView(framebuffer, slot, view);
}

/*!
** @brief Set the framebuffer depth attachment view.
**
** @param framebuffer Framebuffer to update.
** @param view Depth texture view to bind, or NULL to remove the depth attachment.
*/
static inline void rtFramebufferDepthView(rt_framebuffer framebuffer, rt_texture_view view) {
	rt_rtFramebufferDepthView(framebuffer, view);
}

/*!
** @brief Create an unconfigured graphics program.
**
** @return New graphics program handle.
**
** @note Creation takes no shader data.
** @note Backend pipeline objects are created lazily when drawing.
*/
static inline rt_graphics_program rtGraphicsProgramCreate(void) {
	return rt_rtGraphicsProgramCreate();
}

/*!
** @brief Retire a graphics program handle.
**
** @param program Graphics program to destroy.
*/
static inline void rtGraphicsProgramDestroy(rt_graphics_program program) {
	rt_rtGraphicsProgramDestroy(program);
}

/*!
** @brief Define the vertex input layout used by a graphics program.
**
** @param program Graphics program to configure.
** @param layout Vertex buffer stride and attributes, or NULL for no vertex input.
**
** @note The layout is copied by the implementation.
** @note Changing the layout invalidates any cached backend pipeline.
*/
static inline void rtGraphicsProgramVertexLayout(rt_graphics_program program, const rt_vertex_layout* layout) {
	rt_rtGraphicsProgramVertexLayout(program, layout);
}

/*!
** @brief Upload the vertex shader for a graphics program.
**
** @param program Graphics program to configure.
** @param size Size in bytes of @p data.
** @param data GLSL shader source.
*/
static inline void rtGraphicsProgramVertexShader(rt_graphics_program program, u64 size, const void* data) {
	rt_rtGraphicsProgramVertexShader(program, size, data);
}

/*!
** @brief Upload the fragment shader for a graphics program.
**
** @param program Graphics program to configure.
** @param size Size in bytes of @p data.
** @param data GLSL shader source.
*/
static inline void rtGraphicsProgramFragmentShader(rt_graphics_program program, u64 size, const void* data) {
	rt_rtGraphicsProgramFragmentShader(program, size, data);
}

/*!
** @brief Set rasterization state for a graphics program.
**
** @param program Graphics program to configure.
** @param cull_mode Which triangle faces to cull.
** @param front_face Winding order considered front-facing in Rutile clip space.
** @param fill_mode Triangle fill mode.
**
** @note Changing raster state invalidates any cached backend pipeline.
*/
static inline void rtGraphicsProgramRasterState(rt_graphics_program program, enum rt_cull_mode cull_mode, enum rt_front_face front_face, enum rt_fill_mode fill_mode) {
	rt_rtGraphicsProgramRasterState(program, cull_mode, front_face, fill_mode);
}

static inline void rtGraphicsProgramBlendState(rt_graphics_program program, bool enabled, enum rt_blend_factor src_color, enum rt_blend_factor dst_color, enum rt_blend_op color_op, enum rt_blend_factor src_alpha, enum rt_blend_factor dst_alpha, enum rt_blend_op alpha_op) {
	rt_rtGraphicsProgramBlendState(program, enabled, src_color, dst_color, color_op, src_alpha, dst_alpha, alpha_op);
}

/*!
** @brief Link a graphics program after setting shader and layout state.
**
** @param program Graphics program to link.
**
** @note A graphics program must be linked before it can be used for drawing.
** @note Changing shaders or vertex layout invalidates the link.
*/
static inline void rtGraphicsProgramLink(rt_graphics_program program) {
	rt_rtGraphicsProgramLink(program);
}

/*!
** @brief Return the shader uniform location with the requested name.
**
** @param program Linked graphics program to query.
** @param name Null-terminated shader-visible uniform name.
** @return Location handle, or NULL when the program has no matching uniform.
**
** @note Locations are owned by the graphics program.
** @note Locations become invalid when the program is relinked or destroyed.
*/
static inline rt_uniform_location rtGraphicsProgramUniformLocation(rt_graphics_program program, const char* name) {
	return rt_rtGraphicsProgramUniformLocation(program, name);
}

/*!
** @brief Create an unrecorded command buffer handle.
**
** @return New command buffer handle.
**
** @note Command buffers are associated with a queue when recording begins.
** @note Creation takes no configuration.
*/
static inline rt_command_buffer rtCmdCreate(void) {
	return rt_rtCmdCreate();
}

/*!
** @brief Retire a command buffer handle.
**
** @param command_buffer Command buffer to destroy.
**
** @note Destruction ends the public lifetime of the command buffer.
** @note Work already submitted may continue to reference the zombie command buffer.
*/
static inline void rtCmdDestroy(rt_command_buffer command_buffer) {
	rt_rtCmdDestroy(command_buffer);
}

/*!
** @brief Begin recording a command buffer for a queue.
**
** @param command_buffer Command buffer to record.
** @param queue Queue the command buffer will be submitted to.
**
** @note Queue selection happens at begin time.
** @note The command buffer must not already be recording.
*/
static inline void rtCmdBegin(rt_command_buffer command_buffer, rt_queue queue) {
	rt_rtCmdBegin(command_buffer, queue);
}

/*!
** @brief Begin rendering into a framebuffer.
**
** @param command_buffer Command buffer being recorded.
** @param framebuffer Framebuffer to render into.
*/
static inline void rtCmdBeginRendering(rt_command_buffer command_buffer, rt_framebuffer framebuffer) {
	rt_rtCmdBeginRendering(command_buffer, framebuffer);
}

/*!
** @brief Clear a color attachment of the current framebuffer.
**
** @param command_buffer Command buffer being recorded.
** @param color_index Color attachment index to clear.
** @param r Red component.
** @param g Green component.
** @param b Blue component.
** @param a Alpha component.
*/
static inline void rtCmdClearColor(rt_command_buffer command_buffer, u32 color_index, f32 r, f32 g, f32 b, f32 a) {
	rt_rtCmdClearColor(command_buffer, color_index, r, g, b, a);
}

/*!
** @brief Clear the current depth attachment.
**
** @param command_buffer Command buffer being recorded.
** @param depth Depth clear value.
*/
static inline void rtCmdClearDepth(rt_command_buffer command_buffer, f32 depth) {
	rt_rtCmdClearDepth(command_buffer, depth);
}

/*!
** @brief Clear the current stencil attachment.
**
** @param command_buffer Command buffer being recorded.
** @param stencil Stencil clear value.
*/
static inline void rtCmdClearStencil(rt_command_buffer command_buffer, u32 stencil) {
	rt_rtCmdClearStencil(command_buffer, stencil);
}

/*!
** @brief Select the graphics program used by following draw commands.
**
** @param command_buffer Command buffer being recorded.
** @param program Graphics program to use.
**
** @note Graphics state is recorded into the command buffer.
*/
static inline void rtCmdUseGraphicsProgram(rt_command_buffer command_buffer, rt_graphics_program program) {
	rt_rtCmdUseGraphicsProgram(command_buffer, program);
}

/*!
** @brief Set the scissor rectangle used by following draw commands.
**
** @param command_buffer Command buffer being recorded.
** @param x Left coordinate in framebuffer pixels.
** @param y Top coordinate in framebuffer pixels.
** @param width Scissor rectangle width in pixels.
** @param height Scissor rectangle height in pixels.
**
** @note Coordinates use framebuffer top-left origin.
** @note Binding a graphics program resets the scissor to the full framebuffer.
*/
static inline void rtCmdSetScissor(rt_command_buffer command_buffer, u32 x, u32 y, u32 width, u32 height) {
	rt_rtCmdSetScissor(command_buffer, x, y, width, height);
}

/*!
** @brief Bind a buffer to a shader uniform location for following draw commands.
**
** @param command_buffer Command buffer being recorded.
** @param location Program uniform location to set.
** @param buffer Buffer with RT_BUFFER_USAGE_UNIFORM storage.
** @param offset Byte offset into @p buffer.
** @param size Number of bytes visible through the binding.
**
** @note Uniform state is recorded into the command buffer.
** @note The location must belong to the currently bound graphics program.
*/
static inline void rtCmdUniformBuffer(rt_command_buffer command_buffer, rt_uniform_location location, rt_buffer buffer, u64 offset, u64 size) {
	rt_rtCmdUniformBuffer(command_buffer, location, buffer, offset, size);
}

/*!
** @brief Bind a sampled texture to a shader uniform location for following draw commands.
**
** @param command_buffer Command buffer being recorded.
** @param location Program uniform location to set.
** @param texture_view Texture view to sample.
** @note Texture state is recorded into the command buffer.
** @note The location must belong to the currently bound graphics program.
*/
static inline void rtCmdUniformTexture(rt_command_buffer command_buffer, rt_uniform_location location, rt_texture_view texture_view) {
	rt_rtCmdUniformTexture(command_buffer, location, texture_view);
}

/*!
** @brief Bind the vertex buffer used by following draw commands.
**
** @param command_buffer Command buffer being recorded.
** @param buffer Vertex buffer to bind.
** @param offset Byte offset into @p buffer.
*/
static inline void rtCmdBindVertexBuffer(rt_command_buffer command_buffer, rt_buffer buffer, u64 offset) {
	rt_rtCmdBindVertexBuffer(command_buffer, buffer, offset);
}

/*!
** @brief Record a non-indexed draw.
**
** @param command_buffer Command buffer being recorded.
** @param vertex_count Number of vertices to draw.
** @param first_vertex First vertex index.
*/
static inline void rtCmdDraw(rt_command_buffer command_buffer, u32 vertex_count, u32 first_vertex) {
	rt_rtCmdDraw(command_buffer, vertex_count, first_vertex);
}

/*!
** @brief End the current rendering section.
**
** @param command_buffer Command buffer being recorded.
*/
static inline void rtCmdEndRendering(rt_command_buffer command_buffer) {
	rt_rtCmdEndRendering(command_buffer);
}

/*!
** @brief End command buffer recording.
**
** @param command_buffer Command buffer being recorded.
**
** @note The command buffer must have been begun before it is ended.
** @note After ending, the command buffer may be submitted to its queue.
*/
static inline void rtCmdEnd(rt_command_buffer command_buffer) {
	rt_rtCmdEnd(command_buffer);
}

/*!
** @brief Query a queue with the requested capability.
**
** @param capability Queue capability required by the caller.
** @return Queue handle, or NULL when no matching queue is available.
**
** @note Queue handles are borrowed handles.
** @note The returned queue remains valid until rtExit.
*/
static inline rt_queue rtQueueQuery(enum rt_queue_capability capability) {
	return rt_rtQueueQuery(capability);
}

/*!
** @brief Make the next queue submit wait for a timepoint.
**
** @param queue Queue that will wait before its next submit.
** @param timepoint Timepoint that must be reached before submitted work runs.
**
** @note Waiting on a null/completed timepoint records no dependency.
** @note The wait is performed by @p queue, not by blocking the CPU.
*/
static inline void rtQueueWait(rt_queue queue, rt_timepoint timepoint) {
	rt_rtQueueWait(queue, timepoint);
}

/*!
** @brief Submit a command buffer to a queue.
**
** @param queue Queue receiving the work.
** @param command_buffer Recorded command buffer.
** @return Timepoint that identifies completion of this submit.
**
** @note The returned timepoint is associated with @p queue.
** @note Command buffer compatibility with @p queue is a usage contract.
*/
static inline rt_timepoint rtQueueSubmit(rt_queue queue, rt_command_buffer command_buffer) {
	return rt_rtQueueSubmit(queue, command_buffer);
}

/*!
** @brief Flush queued work to the GPU.
**
** @param queue Queue whose recorded submissions should be sent.
** @return Last flushed queue timepoint, or a completed/null timepoint when no work was pending.
*/
static inline rt_timepoint rtQueueFlush(rt_queue queue) {
	return rt_rtQueueFlush(queue);
}

/*!
** @brief Wait until a timepoint is reached.
**
** @param timepoint Timepoint to wait for.
**
** @note Waiting on a null/completed timepoint returns immediately.
** @note The queue inside @p timepoint defines the synchronization domain.
*/
static inline void rtTimepointWait(rt_timepoint timepoint) {
	rt_rtTimepointWait(timepoint);
}

/*!
** @brief Return whether a timepoint has been reached.
**
** @param timepoint Timepoint to query.
** @return true when the timepoint is complete.
**
** @note Querying a timepoint does not block.
** @note A null/completed timepoint is always reached.
*/
static inline bool rtTimepointReached(rt_timepoint timepoint) {
	return rt_rtTimepointReached(timepoint);
}
#endif /* RT_NO_API_WRAPPERS */

#ifdef RUTILE_IMPL
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RT__MAX_LAYERS 16
#define RT__MAX_DLLS (RT__MAX_LAYERS + 1)

#if defined(_WIN32)
#define RT__THREAD_LOCAL __declspec(thread)
#elif defined(__cplusplus)
#define RT__THREAD_LOCAL thread_local
#else
#define RT__THREAD_LOCAL _Thread_local
#endif

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef HMODULE rt__dll_handle;
#else
#include <dlfcn.h>
#include <limits.h>
#include <unistd.h>
typedef void* rt__dll_handle;
#endif

static rt__dll_handle rt__dll_open(const char* path);
static void* rt__dll_symbol(rt__dll_handle dll, const char* symbol);
static void rt__dll_close(rt__dll_handle dll);

#if defined(_WIN32)
static rt__dll_handle rt__dll_open(const char* path) {
	return LoadLibraryA(path);
}

static bool rt__exe_dir_path(char* out, usize out_size) {
	DWORD length;
	char* slash;
	char* backslash;

	if (!out || out_size == 0) {
		return false;
	}
	out[0] = '\0';

	length = GetModuleFileNameA(NULL, out, (DWORD)out_size);
	if (length == 0 || length >= out_size) {
		out[0] = '\0';
		return false;
	}

	slash = strrchr(out, '/');
	backslash = strrchr(out, '\\');
	if (backslash && (!slash || backslash > slash)) {
		slash = backslash;
	}
	if (!slash) {
		out[0] = '\0';
		return false;
	}

	slash[1] = '\0';
	return true;
}

static void rt__dll_last_error_message(char* out, usize out_size) {
	DWORD err;
	DWORD written;

	if (!out || out_size == 0) {
		return;
	}
	out[0] = '\0';

	err = GetLastError();
	if (!err) {
		return;
	}

	written = FormatMessageA(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		err,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		out,
		(DWORD)out_size,
		NULL);
	if (written == 0) {
		snprintf(out, out_size, "Windows error %llu", (u64)err);
	}
	out[out_size - 1] = '\0';
}

static void* rt__dll_symbol(rt__dll_handle dll, const char* symbol) {
	return (void*)GetProcAddress(dll, symbol);
}

static void rt__dll_close(rt__dll_handle dll) {
	if (dll) {
		FreeLibrary(dll);
	}
}
#else
static rt__dll_handle rt__dll_open(const char* path) {
	return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

static bool rt__exe_dir_path(char* out, usize out_size) {
	if (!out || out_size == 0) {
		return false;
	}
	out[0] = '\0';

#if defined(__linux__)
	char path[PATH_MAX];
	ssize_t length = readlink("/proc/self/exe", path, sizeof(path) - 1);
	if (length <= 0) {
		return false;
	}
	path[length] = '\0';

	char* slash = strrchr(path, '/');
	if (!slash) {
		return false;
	}
	*(slash + 1) = '\0';

	snprintf(out, out_size, "%s", path);
	out[out_size - 1] = '\0';
	return true;
#else
	return false;
#endif
}

static void rt__dll_last_error_message(char* out, usize out_size) {
	const char* err;

	if (!out || out_size == 0) {
		return;
	}
	out[0] = '\0';

	err = dlerror();
	if (err) {
		snprintf(out, out_size, "%s", err);
		out[out_size - 1] = '\0';
	}
}

static void* rt__dll_symbol(rt__dll_handle dll, const char* symbol) {
	return dlsym(dll, symbol);
}

static void rt__dll_close(rt__dll_handle dll) {
	if (dll) {
		dlclose(dll);
	}
}
#endif

typedef struct rt_layer_link {
	rt_proc_chain chain;
	rt__dll_handle dll;
	rt_proc_chain next;
} rt_layer_link;

static rt__dll_handle rt__backend_dll;
static rt_layer_link rt__layer_links[RT__MAX_LAYERS];
static rt_proc_chain rt__chain;
bool rt_loaded = false;

static rt__dll_handle rt__dlls[RT__MAX_DLLS];
static u32 rt__dll_count;

static rt_proc_t rt__backend_proc(const rt_proc_chain* chain, const char* name) {

	if (!rt__backend_dll || !name) {
		return NULL;
	}

	rt_proc_t proc = (rt_proc_t)rt__dll_symbol(rt__backend_dll, name);
	if (proc) {
		return proc;
	}

	return NULL;
}

static rt_proc_t rt__layer_proc_at(u32 index, const char* name) {
	if (index >= RT__MAX_LAYERS || !name) {
		return NULL;
	}

	rt_layer_link* link = &rt__layer_links[index];
	if (!link->next.get_proc) {
		return NULL;
	}

	rt_proc_t next_proc = link->next.get_proc(&link->next, name);
	if (!next_proc) {
		return NULL;
	}

	rt_proc_t wrapper = (rt_proc_t)rt__dll_symbol(link->dll, name);
	if (wrapper) {
		return wrapper;
	}

	return next_proc;
}

#define RT__LAYER_PROC_LIST(X) \
	X(0)                       \
	X(1)                       \
	X(2)                       \
	X(3)                       \
	X(4)                       \
	X(5)                       \
	X(6)                       \
	X(7)                       \
	X(8)                       \
	X(9)                       \
	X(10)                      \
	X(11)                      \
	X(12)                      \
	X(13)                      \
	X(14)                      \
	X(15)

#define RT__LAYER_PROC(index)                                                                \
	static rt_proc_t rt__layer##index##_proc(const rt_proc_chain* chain, const char* name) { \
		return rt__layer_proc_at(index, name);                                               \
	}

RT__LAYER_PROC_LIST(RT__LAYER_PROC)

#undef RT__LAYER_PROC

#define RT__LAYER_PROC_ENTRY(index) rt__layer##index##_proc,

static rt_proc_t (*rt__layer_procs[RT__MAX_LAYERS])(const rt_proc_chain* chain, const char* name) = {
	RT__LAYER_PROC_LIST(RT__LAYER_PROC_ENTRY)
};

#undef RT__LAYER_PROC_ENTRY
#undef RT__LAYER_PROC_LIST

static enum rt_error rt__remember_dll(rt__dll_handle dll) {
	if (rt__dll_count >= RT__MAX_DLLS) {
		rt__dll_close(dll);
		return RT_IMPROPER_USAGE;
	}
	rt__dlls[rt__dll_count++] = dll;
	return RT_SUCCESS;
}

static void rt__close_dlls(void) {
	for (u32 i = rt__dll_count; i > 0; i--) {
		rt__dll_close(rt__dlls[i - 1]);
		rt__dlls[i - 1] = (rt__dll_handle)0;
	}
	rt__dll_count = 0;
}

static void rt__backend_dll_name(const char* name, char* out, usize out_size) {
#if defined(_WIN32)
	snprintf(out, out_size, "%s.dll", name);
#elif defined(__APPLE__)
	snprintf(out, out_size, "lib%s.dylib", name);
#else
	snprintf(out, out_size, "lib%s.so", name);
#endif
}

static void rt__layer_dll_name(const char* name, char* out, usize out_size) {
	const char* stem = name;
	if (strncmp(stem, "RT_", 3) == 0) {
		stem += 3;
	}

#if defined(_WIN32)
	snprintf(out, out_size, "rt-%s.dll", stem);
#elif defined(__APPLE__)
	snprintf(out, out_size, "librt-%s.dylib", stem);
#else
	snprintf(out, out_size, "librt-%s.so", stem);
#endif
	for (char* p = out; *p; p++) {
		if (*p == '_') {
			*p = '-';
		}
		if (*p >= 'A' && *p <= 'Z') {
			*p = (char)(*p - 'A' + 'a');
		}
	}
}

static enum rt_error rt__load_backend_dll(const char* path, const char* requested_name, rt__dll_handle* out_dll, char* message, usize message_size) {
	if (message && message_size) {
		message[0] = '\0';
	}
	if (!path || !requested_name || !out_dll) {
		return RT_IMPROPER_USAGE;
	}
	*out_dll = (rt__dll_handle)0;

	rt__dll_handle dll = rt__dll_open(path);
	if (!dll) {
		char dll_error[512];
		rt__dll_last_error_message(dll_error, sizeof(dll_error));
		if (message && message_size && dll_error[0]) {
			snprintf(message, message_size, "failed to load implementation DLL %s: %s", path, dll_error);
			message[message_size - 1] = '\0';
		} else if (message && message_size) {
			snprintf(message, message_size, "failed to load implementation DLL %s", path);
			message[message_size - 1] = '\0';
		}
		return RT_NO_BACKEND;
	}

	PFN_rtGetName get_name = (PFN_rtGetName)rt__dll_symbol(dll, "rtGetName");
	if (!get_name) {
		if (message && message_size) {
			snprintf(message, message_size, "implementation DLL %s does not export rtGetName", path);
			message[message_size - 1] = '\0';
		}
		rt__dll_close(dll);
		return RT_NO_BACKEND;
	}
	if (!get_name() || strcmp(get_name(), requested_name) != 0) {
		if (message && message_size) {
			snprintf(message, message_size, "implementation DLL %s reported name %s, expected %s", path, get_name() ? get_name() : "<null>", requested_name);
			message[message_size - 1] = '\0';
		}
		rt__dll_close(dll);
		return RT_NO_BACKEND;
	}

	enum rt_error err = rt__remember_dll(dll);
	if (err != RT_SUCCESS) {
		if (message && message_size) {
			snprintf(message, message_size, "too many loaded DLLs while loading %s", path);
			message[message_size - 1] = '\0';
		}
		return err;
	}

	*out_dll = dll;
	return RT_SUCCESS;
}

static enum rt_error rt__load_layer_dll(const char* path, const char* requested_name, rt_layer_link* out_link, char* message, usize message_size) {
	if (message && message_size) {
		message[0] = '\0';
	}
	if (!path || !requested_name || !out_link) {
		return RT_IMPROPER_USAGE;
	}
	out_link->dll = (rt__dll_handle)0;

	rt__dll_handle dll = rt__dll_open(path);
	if (!dll) {
		char dll_error[512];
		rt__dll_last_error_message(dll_error, sizeof(dll_error));
		if (message && message_size && dll_error[0]) {
			snprintf(message, message_size, "failed to load layer DLL %s: %s", path, dll_error);
			message[message_size - 1] = '\0';
		} else if (message && message_size) {
			snprintf(message, message_size, "failed to load layer DLL %s", path);
			message[message_size - 1] = '\0';
		}
		return RT_IMPROPER_USAGE;
	}

	PFN_rtLayerGetName get_name = (PFN_rtLayerGetName)rt__dll_symbol(dll, "rtLayerGetName");
	if (!get_name) {
		if (message && message_size) {
			snprintf(message, message_size, "layer DLL %s does not export rtLayerGetName", path);
			message[message_size - 1] = '\0';
		}
		rt__dll_close(dll);
		return RT_IMPROPER_USAGE;
	}
	if (!get_name() || strcmp(get_name(), requested_name) != 0) {
		if (message && message_size) {
			snprintf(message, message_size, "layer DLL %s reported name %s, expected %s", path, get_name() ? get_name() : "<null>", requested_name);
			message[message_size - 1] = '\0';
		}
		rt__dll_close(dll);
		return RT_IMPROPER_USAGE;
	}

	enum rt_error err = rt__remember_dll(dll);
	if (err != RT_SUCCESS) {
		if (message && message_size) {
			snprintf(message, message_size, "too many loaded DLLs while loading %s", path);
			message[message_size - 1] = '\0';
		}
		return err;
	}

	out_link->dll = dll;
	return RT_SUCCESS;
}

static enum rt_error rt__load_backend_named(const char* name, rt__dll_handle* out_dll, char* message, usize message_size) {
	char dll[128];
	rt__backend_dll_name(name, dll, sizeof(dll));
	enum rt_error err = rt__load_backend_dll(dll, name, out_dll, message, message_size);
	if (err == RT_SUCCESS) {
		return err;
	}

	{
		char exe_dir[1024];
		if (rt__exe_dir_path(exe_dir, sizeof(exe_dir))) {
			char path[1152];
			snprintf(path, sizeof(path), "%s%s", exe_dir, dll);
			path[sizeof(path) - 1] = '\0';
			err = rt__load_backend_dll(path, name, out_dll, message, message_size);
		}
	}
	return err;
}

static enum rt_error rt__load_layer_named(const char* name, rt_layer_link* out_link, char* message, usize message_size) {
	char dll[128];
	rt__layer_dll_name(name, dll, sizeof(dll));
	enum rt_error err = rt__load_layer_dll(dll, name, out_link, message, message_size);
	if (err == RT_SUCCESS) {
		return err;
	}

	{
		char exe_dir[1024];
		if (rt__exe_dir_path(exe_dir, sizeof(exe_dir))) {
			char path[1152];
			snprintf(path, sizeof(path), "%s%s", exe_dir, dll);
			path[sizeof(path) - 1] = '\0';
			err = rt__load_layer_dll(path, name, out_link, message, message_size);
		}
	}
	return err;
}

static void rt__layer_set_next(u32 index, rt_proc_chain next) {
	if (index >= RT__MAX_LAYERS) {
		return;
	}

	rt_layer_link* link = &rt__layer_links[index];
	link->next = next;

	PFN_rtLayerSetNext set_next = (PFN_rtLayerSetNext)rt__dll_symbol(link->dll, "rtLayerSetNext");
	if (set_next) {
		set_next(next);
	}

	link->chain.get_proc = rt__layer_procs[index];
}

PFN_rtInit rt_rtInit = NULL;
PFN_rtExit rt_rtExit = NULL;
PFN_rtSetOutput rt_rtSetOutput = NULL;
PFN_rtError rt_rtError = NULL;
PFN_rtErrorMessage rt_rtErrorMessage = NULL;
PFN_rtClearError rt_rtClearError = NULL;
PFN_rtGetName rt_rtGetName = NULL;
PFN_rtQueryFormatCapabilities rt_rtQueryFormatCapabilities = NULL;

PFN_rtBufferCreate rt_rtBufferCreate = NULL;
PFN_rtBufferDestroy rt_rtBufferDestroy = NULL;
PFN_rtBufferData rt_rtBufferData = NULL;
PFN_rtBufferSubdata rt_rtBufferSubdata = NULL;
PFN_rtBufferRead rt_rtBufferRead = NULL;
PFN_rtBufferMap rt_rtBufferMap = NULL;
PFN_rtBufferUnmap rt_rtBufferUnmap = NULL;

PFN_rtTextureCreate rt_rtTextureCreate = NULL;
PFN_rtTextureDestroy rt_rtTextureDestroy = NULL;
PFN_rtTextureViewCreate rt_rtTextureViewCreate = NULL;
PFN_rtTextureViewDestroy rt_rtTextureViewDestroy = NULL;
PFN_rtTextureViewFilter rt_rtTextureViewFilter = NULL;
PFN_rtTextureViewAddress rt_rtTextureViewAddress = NULL;
PFN_rtTextureViewAnisotropy rt_rtTextureViewAnisotropy = NULL;
PFN_rtTextureViewLod rt_rtTextureViewLod = NULL;

PFN_rtTextureCopy rt_rtTextureCopy = NULL;
PFN_rtTextureData rt_rtTextureData = NULL;
PFN_rtTextureSubcopy rt_rtTextureSubcopy = NULL;
PFN_rtTextureSubdata rt_rtTextureSubdata = NULL;
PFN_rtTextureViewCopyToBuffer rt_rtTextureViewCopyToBuffer = NULL;
PFN_rtTextureViewExtent rt_rtTextureViewExtent = NULL;
PFN_rtFramebufferCreate rt_rtFramebufferCreate = NULL;
PFN_rtFramebufferDestroy rt_rtFramebufferDestroy = NULL;
PFN_rtFramebufferColorView rt_rtFramebufferColorView = NULL;
PFN_rtFramebufferSetColorView rt_rtFramebufferSetColorView = NULL;
PFN_rtFramebufferDepthView rt_rtFramebufferDepthView = NULL;

PFN_rtGraphicsProgramCreate rt_rtGraphicsProgramCreate = NULL;
PFN_rtGraphicsProgramDestroy rt_rtGraphicsProgramDestroy = NULL;
PFN_rtGraphicsProgramVertexLayout rt_rtGraphicsProgramVertexLayout = NULL;
PFN_rtGraphicsProgramVertexShader rt_rtGraphicsProgramVertexShader = NULL;
PFN_rtGraphicsProgramFragmentShader rt_rtGraphicsProgramFragmentShader = NULL;
PFN_rtGraphicsProgramRasterState rt_rtGraphicsProgramRasterState = NULL;
PFN_rtGraphicsProgramBlendState rt_rtGraphicsProgramBlendState = NULL;
PFN_rtGraphicsProgramLink rt_rtGraphicsProgramLink = NULL;
PFN_rtGraphicsProgramUniformLocation rt_rtGraphicsProgramUniformLocation = NULL;

PFN_rtCmdCreate rt_rtCmdCreate = NULL;
PFN_rtCmdDestroy rt_rtCmdDestroy = NULL;
PFN_rtCmdBegin rt_rtCmdBegin = NULL;
PFN_rtCmdBeginRendering rt_rtCmdBeginRendering = NULL;
PFN_rtCmdClearColor rt_rtCmdClearColor = NULL;
PFN_rtCmdClearDepth rt_rtCmdClearDepth = NULL;
PFN_rtCmdClearStencil rt_rtCmdClearStencil = NULL;
PFN_rtCmdUseGraphicsProgram rt_rtCmdUseGraphicsProgram = NULL;
PFN_rtCmdSetScissor rt_rtCmdSetScissor = NULL;
PFN_rtCmdUniformBuffer rt_rtCmdUniformBuffer = NULL;
PFN_rtCmdUniformTexture rt_rtCmdUniformTexture = NULL;
PFN_rtCmdBindVertexBuffer rt_rtCmdBindVertexBuffer = NULL;
PFN_rtCmdDraw rt_rtCmdDraw = NULL;
PFN_rtCmdEndRendering rt_rtCmdEndRendering = NULL;
PFN_rtCmdEnd rt_rtCmdEnd = NULL;

PFN_rtQueueQuery rt_rtQueueQuery = NULL;
PFN_rtQueueWait rt_rtQueueWait = NULL;
PFN_rtQueueSubmit rt_rtQueueSubmit = NULL;
PFN_rtQueueFlush rt_rtQueueFlush = NULL;
PFN_rtTimepointWait rt_rtTimepointWait = NULL;
PFN_rtTimepointReached rt_rtTimepointReached = NULL;

static RT__THREAD_LOCAL enum rt_error rt__loader_error = RT_SUCCESS;
static RT__THREAD_LOCAL char rt__loader_error_message[1024] = "";

static void rt__loader_set_error(enum rt_error error, const char* message) {
	rt__loader_error = error;
	if (!message) {
		rt__loader_error_message[0] = '\0';
		return;
	}
	snprintf(rt__loader_error_message, sizeof(rt__loader_error_message), "%s", message);
	rt__loader_error_message[sizeof(rt__loader_error_message) - 1] = '\0';
}

static void rt__loader_set_errorf(enum rt_error error, const char* format, ...) {
	va_list args;

	rt__loader_error = error;
	if (!format) {
		rt__loader_error_message[0] = '\0';
		return;
	}

	va_start(args, format);
	vsnprintf(rt__loader_error_message, sizeof(rt__loader_error_message), format, args);
	va_end(args);
	rt__loader_error_message[sizeof(rt__loader_error_message) - 1] = '\0';
}

static void rt__loader_print_error(void) {
	if (rt__loader_error != RT_SUCCESS && rt__loader_error_message[0]) {
		fprintf(stderr, "%s\n", rt__loader_error_message);
	}
}

static void rt__loader_clear_error_state(void) {
	rt__loader_error = RT_SUCCESS;
	rt__loader_error_message[0] = '\0';
}

static enum rt_error rt__loader_error_code(void) {
	return rt__loader_error;
}

static const char* rt__loader_error_message_text(void) {
	return rt__loader_error_message;
}

static void rt__loader_clear_error(void) {
	rt__loader_clear_error_state();
}

#undef RT__THREAD_LOCAL

static enum rt_error rt__resolve_required_proc(const char* name, rt_proc_t* out_proc, char* message, usize message_size) {
	if (message && message_size) {
		message[0] = '\0';
	}
	if (!name || !out_proc) {
		return RT_IMPROPER_USAGE;
	}

	rt_proc_t proc = rtGetProc(name);
	if (!proc) {
		*out_proc = NULL;
		if (message && message_size) {
			snprintf(message, message_size, "loaded implementation did not provide required proc: %s", name);
			message[message_size - 1] = '\0';
		}
		return RT_EXTENSION_NOT_PRESENT;
	}

	*out_proc = proc;
	return RT_SUCCESS;
}

#define RT__CORE_RESOLVE(name)                                                             \
	do {                                                                                   \
		rt_proc_t _p = NULL;                                                               \
		enum rt_error _err = rt__resolve_required_proc(#name, &_p, message, message_size); \
		if (_err != RT_SUCCESS) {                                                          \
			return _err;                                                                   \
		}                                                                                  \
		rt_##name = (PFN_##name)_p;                                                        \
	} while (0)

static enum rt_error rt__load_core(char* message, usize message_size) {
	if (message && message_size) {
		message[0] = '\0';
	}
	RT__CORE_RESOLVE(rtInit);
	RT__CORE_RESOLVE(rtExit);
	RT__CORE_RESOLVE(rtSetOutput);
	RT__CORE_RESOLVE(rtError);
	RT__CORE_RESOLVE(rtErrorMessage);
	RT__CORE_RESOLVE(rtClearError);
	RT__CORE_RESOLVE(rtGetName);
	RT__CORE_RESOLVE(rtQueryFormatCapabilities);

	RT__CORE_RESOLVE(rtBufferCreate);
	RT__CORE_RESOLVE(rtBufferDestroy);
	RT__CORE_RESOLVE(rtBufferData);
	RT__CORE_RESOLVE(rtBufferSubdata);
	RT__CORE_RESOLVE(rtBufferRead);
	RT__CORE_RESOLVE(rtBufferMap);
	RT__CORE_RESOLVE(rtBufferUnmap);

	RT__CORE_RESOLVE(rtTextureCreate);
	RT__CORE_RESOLVE(rtTextureDestroy);
	RT__CORE_RESOLVE(rtTextureViewCreate);
	RT__CORE_RESOLVE(rtTextureViewDestroy);
	RT__CORE_RESOLVE(rtTextureViewFilter);
	RT__CORE_RESOLVE(rtTextureViewAddress);
	RT__CORE_RESOLVE(rtTextureViewAnisotropy);
	RT__CORE_RESOLVE(rtTextureViewLod);

	RT__CORE_RESOLVE(rtTextureCopy);
	RT__CORE_RESOLVE(rtTextureData);
	RT__CORE_RESOLVE(rtTextureSubcopy);
	RT__CORE_RESOLVE(rtTextureSubdata);
	RT__CORE_RESOLVE(rtTextureViewCopyToBuffer);
	RT__CORE_RESOLVE(rtTextureViewExtent);
	RT__CORE_RESOLVE(rtFramebufferCreate);
	RT__CORE_RESOLVE(rtFramebufferDestroy);
	RT__CORE_RESOLVE(rtFramebufferColorView);
	RT__CORE_RESOLVE(rtFramebufferSetColorView);
	RT__CORE_RESOLVE(rtFramebufferDepthView);

	RT__CORE_RESOLVE(rtGraphicsProgramCreate);
	RT__CORE_RESOLVE(rtGraphicsProgramDestroy);
	RT__CORE_RESOLVE(rtGraphicsProgramVertexLayout);
	RT__CORE_RESOLVE(rtGraphicsProgramVertexShader);
	RT__CORE_RESOLVE(rtGraphicsProgramFragmentShader);
	RT__CORE_RESOLVE(rtGraphicsProgramRasterState);
	RT__CORE_RESOLVE(rtGraphicsProgramBlendState);
	RT__CORE_RESOLVE(rtGraphicsProgramLink);
	RT__CORE_RESOLVE(rtGraphicsProgramUniformLocation);

	RT__CORE_RESOLVE(rtCmdCreate);
	RT__CORE_RESOLVE(rtCmdDestroy);
	RT__CORE_RESOLVE(rtCmdBegin);
	RT__CORE_RESOLVE(rtCmdBeginRendering);
	RT__CORE_RESOLVE(rtCmdClearColor);
	RT__CORE_RESOLVE(rtCmdClearDepth);
	RT__CORE_RESOLVE(rtCmdClearStencil);
	RT__CORE_RESOLVE(rtCmdUseGraphicsProgram);
	RT__CORE_RESOLVE(rtCmdSetScissor);
	RT__CORE_RESOLVE(rtCmdUniformBuffer);
	RT__CORE_RESOLVE(rtCmdUniformTexture);
	RT__CORE_RESOLVE(rtCmdBindVertexBuffer);
	RT__CORE_RESOLVE(rtCmdDraw);
	RT__CORE_RESOLVE(rtCmdEndRendering);
	RT__CORE_RESOLVE(rtCmdEnd);

	RT__CORE_RESOLVE(rtQueueQuery);
	RT__CORE_RESOLVE(rtQueueWait);
	RT__CORE_RESOLVE(rtQueueSubmit);
	RT__CORE_RESOLVE(rtQueueFlush);
	RT__CORE_RESOLVE(rtTimepointWait);
	RT__CORE_RESOLVE(rtTimepointReached);

	return RT_SUCCESS;
}

#undef RT__CORE_RESOLVE

enum rt_error rtLoad(const char* backend_name, const char* const* layer_names, u32 layer_count) {
	rt__loader_clear_error_state();
	if (!backend_name) {
		rt__loader_set_errorf(RT_NO_BACKEND, "rtLoad backend_name is NULL");
		rt__loader_print_error();
		return RT_NO_BACKEND;
	}
	if (layer_count > RT__MAX_LAYERS) {
		rt__loader_set_errorf(RT_IMPROPER_USAGE, "rtLoad requested too many layers: %u", layer_count);
		rt__loader_print_error();
		return RT_IMPROPER_USAGE;
	}
	if (layer_count && !layer_names) {
		rt__loader_set_errorf(RT_IMPROPER_USAGE, "rtLoad layer_count is %u but layer_names is NULL", layer_count);
		rt__loader_print_error();
		return RT_IMPROPER_USAGE;
	}

	rtUnload();

	char message[1024];
	enum rt_error err = rt__load_backend_named(backend_name, &rt__backend_dll, message, sizeof(message));
	if (err != RT_SUCCESS) {
		rt__close_dlls();
		if (rt__loader_error == RT_SUCCESS) {
			rt__loader_set_errorf(err, "%s", message[0] ? message : "failed to load implementation");
		}
		rt__loader_print_error();
		return err;
	}
	for (u32 i = 0; i < layer_count; i++) {
		err = rt__load_layer_named(layer_names[i], &rt__layer_links[i], message, sizeof(message));
		if (err != RT_SUCCESS) {
			rt__close_dlls();
			rt__loader_set_errorf(err, "%s", message[0] ? message : "failed to load layer");
			rt__loader_print_error();
			return err;
		}
	}

	rt_proc_chain chain;
	chain.get_proc = rt__backend_proc;
	for (u32 i = 0; i < layer_count; i++) {
		rt__layer_set_next(i, chain);
		chain = rt__layer_links[i].chain;
	}
	rt__chain = chain;

	err = rt__load_core(message, sizeof(message));
	if (err != RT_SUCCESS) {
		if (rt__loader_error == RT_SUCCESS) {
			rt__loader_set_errorf(err, "%s", message[0] ? message : "failed to resolve required core procedures");
		}
		rtUnload();
		rt__loader_print_error();
		return err;
	}
	rt_loaded = true;
	return RT_SUCCESS;
}

void rtUnload(void) {
	rt_loaded = false;
	rt__chain.get_proc = NULL;
	rt__backend_dll = (rt__dll_handle)0;
	rt_rtInit = NULL;
	rt_rtExit = NULL;
	rt_rtSetOutput = NULL;
	rt_rtError = NULL;
	rt_rtErrorMessage = NULL;
	rt_rtClearError = NULL;
	rt_rtGetName = NULL;
	rt_rtQueryFormatCapabilities = NULL;
	rt_rtBufferCreate = NULL;
	rt_rtBufferDestroy = NULL;
	rt_rtBufferData = NULL;
	rt_rtBufferSubdata = NULL;
	rt_rtBufferRead = NULL;
	rt_rtBufferMap = NULL;
	rt_rtBufferUnmap = NULL;
	rt_rtTextureCreate = NULL;
	rt_rtTextureDestroy = NULL;
	rt_rtTextureViewCreate = NULL;
	rt_rtTextureViewDestroy = NULL;
	rt_rtTextureViewFilter = NULL;
	rt_rtTextureViewAddress = NULL;
	rt_rtTextureViewAnisotropy = NULL;
	rt_rtTextureViewLod = NULL;

	rt_rtTextureCopy = NULL;
	rt_rtTextureData = NULL;
	rt_rtTextureSubcopy = NULL;
	rt_rtTextureSubdata = NULL;
	rt_rtTextureViewCopyToBuffer = NULL;
	rt_rtTextureViewExtent = NULL;
	rt_rtFramebufferCreate = NULL;
	rt_rtFramebufferDestroy = NULL;
	rt_rtFramebufferColorView = NULL;
	rt_rtFramebufferSetColorView = NULL;
	rt_rtFramebufferDepthView = NULL;
	rt_rtGraphicsProgramCreate = NULL;
	rt_rtGraphicsProgramDestroy = NULL;
	rt_rtGraphicsProgramVertexLayout = NULL;
	rt_rtGraphicsProgramVertexShader = NULL;
	rt_rtGraphicsProgramFragmentShader = NULL;
	rt_rtGraphicsProgramRasterState = NULL;
	rt_rtGraphicsProgramBlendState = NULL;
	rt_rtGraphicsProgramLink = NULL;
	rt_rtGraphicsProgramUniformLocation = NULL;
	rt_rtCmdCreate = NULL;
	rt_rtCmdDestroy = NULL;
	rt_rtCmdBegin = NULL;
	rt_rtCmdBeginRendering = NULL;
	rt_rtCmdClearColor = NULL;
	rt_rtCmdClearDepth = NULL;
	rt_rtCmdClearStencil = NULL;
	rt_rtCmdUseGraphicsProgram = NULL;
	rt_rtCmdSetScissor = NULL;
	rt_rtCmdUniformBuffer = NULL;
	rt_rtCmdUniformTexture = NULL;
	rt_rtCmdBindVertexBuffer = NULL;
	rt_rtCmdDraw = NULL;
	rt_rtCmdEndRendering = NULL;
	rt_rtCmdEnd = NULL;
	rt_rtQueueQuery = NULL;
	rt_rtQueueWait = NULL;
	rt_rtQueueSubmit = NULL;
	rt_rtQueueFlush = NULL;
	rt_rtTimepointWait = NULL;
	rt_rtTimepointReached = NULL;
	rt__close_dlls();
}

rt_proc_t rtGetProc(const char* name) {
	if (!name || !rt__chain.get_proc) {
		return NULL;
	}
	return rt__chain.get_proc(&rt__chain, name);
}

bool rtLoaded(void) {
	return rt_loaded;
}

#ifndef RT_NO_API_WRAPPERS
/*!
** @brief Return the current error code.
**
** @return Current error code.
**
** @note This reports backend/runtime error state only.
*/
static inline enum rt_error rtError(void) {
	return rt_rtError();
}

/*!
** @brief Return the current error message.
**
** @return Null-terminated message owned by Rutile.
**
** @note This reports backend/runtime error text only.
*/
static inline const char* rtErrorMessage(void) {
	return rt_rtErrorMessage();
}

/*!
** @brief Clear the current error state.
**
** @note This clears backend/runtime error state only.
*/
static inline void rtClearError(void) {
	rt_rtClearError();
}
#endif

#endif /* RUTILE_IMPL */

#endif /* RUTILE_LOADER_ONLY */

#ifdef __cplusplus
}
#endif

#endif /* RUTILE_H */
