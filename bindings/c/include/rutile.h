#ifndef RUTILE_H
#define RUTILE_H

/*!
** @file rutile.h
** @brief Rutile public C API and dynamic loader.
**
** Rutile is a graphics abstraction. This header provides:
**   - The loader (@ref rtLoad, @ref rtUnload, @ref rtLoaded, @ref rtGetProc).
**     The loader resolves a backend DLL plus an optional ordered chain of
**     layer DLLs, then exposes the core API through inline wrappers.
**   - The core API surface itself (resources, command buffers, queues,
**     synchronization). All core entry points are dispatched through function
**     pointers populated by the loader.
**
** @section threading Threading
** Any function in this API may be called from any thread, concurrently with
** any other function. The only constraint is that the caller must not mutate
** the same resource from two threads at once. For example, two threads may
** record into two different command buffers in parallel, but two threads must
** not call @ref rtBufferData on the same buffer concurrently.
**
** @section timepoints Timepoints
** Work that touches the GPU returns an @ref rt_timepoint - an opaque signal
** that fires once the operation has completed on the device. Pass a timepoint
** to @ref rtTimepointWait to block the CPU until completion, to
** @ref rtTimepointReached to poll, or to @ref rtQueueWait to make a queue
** wait for it before its next submission. A zero-initialized timepoint
** (`{ NULL, 0 }`) is the canonical "already reached" sentinel and is always
** safe to wait on; it records no dependency.
**
** @section errors Error model
** Aside from the loader entry points (@ref rtLoad, @ref rtLoadDevelopment),
** core API functions do not return error codes. They report failure
** out-of-band by recording an error code and message on the current
** thread, retrievable via @ref rtError and @ref rtErrorMessage and cleared
** by @ref rtClearError.
**
** **Any core function may record @ref RT_IMPROPER_USAGE.** This is the
** contract layers rely on: a validation layer in the dispatch chain is
** entitled to inspect arguments and surface usage violations through the
** error state of any call, even when the backend itself would have
** silently proceeded. Backends are not required to detect usage violations
** on their own.
**
** Beyond that universal rule, each function documents the additional
** error codes it may produce in an `@error` block. Functions that have no
** `@error` block can only report RT_IMPROPER_USAGE (and only when a
** validation layer is active).
**
** Functions that return a handle indicate failure by returning NULL; the
** specific error code is recorded as described above.
**
** **Destroy functions never record an error.** They are infallible by
** contract - not even RT_IMPROPER_USAGE, and not even from a validation
** layer. This includes safety on NULL and on already-destroyed handles.
**
** @section build_macros Build macros
**   - `RT_BUILD_DLL`       - define when building Rutile itself as a DLL.
**   - `RUTILE_IMPL`        - define in exactly one translation unit to emit
**                            the loader implementation.
**   - `RUTILE_LOADER_ONLY` - define to expose only the loader entry points
**                            and skip the core API declarations.
**   - `RT_NO_API_WRAPPERS` - define to skip the static-inline core wrappers
**                            and use the `rt_rt*` function pointers directly.
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
#define RT_FEATURE_NOT_SUPPORTED ((enum rt_error)16)

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
	const char* name;
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
** @brief Load a backend and an optional ordered chain of layers.
**
** Opens the backend shared library, opens each layer shared library in order,
** wires them into a dispatch chain (calls flow through layer 0, layer 1, ...,
** finally reaching the backend), and resolves every required core entry point
** into the `rt_rt*` function pointers. After a successful call,
** @ref rtLoaded returns true and the static-inline wrappers are safe to use.
**
** At most one backend may be loaded at a time. Calling @ref rtLoad while a
** backend is already loaded implicitly unloads the previous one first.
**
** @param backend_name  Backend name (e.g. `"rutile-vulkan"`). The loader maps
**                      this to a platform-specific DLL/SO/dylib filename.
** @param layer_names   Optional array of layer names, applied in order. The
**                      first entry sits closest to the application; the last
**                      sits closest to the backend. May be NULL when
**                      @p layer_count is 0.
** @param layer_count   Number of entries in @p layer_names.
**
** @return RT_SUCCESS on success.
** @return RT_NO_BACKEND if the backend DLL could not be opened or did not
**         export the expected entry points.
** @return RT_IMPROPER_USAGE for invalid arguments or layer-loading failures.
** @return RT_EXTENSION_NOT_PRESENT if the backend is missing a required core
**         entry point.
**
** @note On failure the loader is left fully unloaded. Use @ref rtGetProc to
**       resolve optional/extension entry points after a successful load.
*/
enum rt_error rtLoad(const char* backend_name, const char* const* layer_names, u32 layer_count);

/*!
** @brief Load a backend and layers in best-effort mode for development.
**
** Same intent as @ref rtLoad but tolerates missing pieces: a missing backend
** leaves Rutile unloaded but still returns RT_SUCCESS; layers that fail to
** load are skipped; entry points that the backend does not provide are left
** as NULL function pointers rather than failing the load.
**
** Intended for iterative development where backends are being built out and
** not every entry point exists yet. Production code should call @ref rtLoad.
**
** @param backend_name  Backend name, or NULL to load no backend.
** @param layer_names   Optional array of layer names (see @ref rtLoad).
** @param layer_count   Number of entries in @p layer_names.
**
** @return RT_SUCCESS in almost all cases; RT_IMPROPER_USAGE only for
**         argument-shape errors (e.g. non-zero @p layer_count with NULL
**         @p layer_names).
*/
enum rt_error rtLoadDevelopment(const char* backend_name, const char* const* layer_names, u32 layer_count);

/*!
** @brief Unload the current backend and layers.
**
** Clears every dispatch function pointer back to NULL, tears down the
** dispatch chain, and closes every shared library that was opened by the
** loader. After this call @ref rtLoaded returns false and the static-inline
** core wrappers must not be called.
**
** Safe to call when nothing is loaded; in that case it is a no-op.
*/
void rtUnload(void);

/*!
** @brief Report whether a backend is currently loaded.
**
** @return true when the loader has a backend wired up and the core wrappers
**         may be called; false otherwise.
*/
bool rtLoaded(void);

/*!
** @brief Resolve a named entry point through the current dispatch chain.
**
** This is the loader's general lookup mechanism. It returns the function
** pointer for @p name as seen at the head of the dispatch chain - i.e. with
** every loaded layer's interception applied. The core wrappers use this
** internally; callers reach for it directly to load optional or
** extension-provided entry points.
**
** @param name  Null-terminated entry-point name (e.g. `"rtBufferData"`, or
**              the name of an extension function).
** @return Function pointer to the entry, or NULL if no loaded module
**         provides it.
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

typedef rt_texture (*PFN_rtTextureCreate)(void);
typedef void (*PFN_rtTextureDestroy)(rt_texture texture);
typedef rt_texture_view (*PFN_rtTextureViewCreate)(void);
typedef void (*PFN_rtTextureViewBind)(rt_texture_view texture_view, rt_texture texture);
typedef void (*PFN_rtTextureViewDestroy)(rt_texture_view texture_view);
typedef void (*PFN_rtTextureViewFilter)(rt_texture_view texture_view, enum rt_filter mag_filter, enum rt_filter min_filter, enum rt_mip_filter mip_filter);
typedef void (*PFN_rtTextureViewAddress)(rt_texture_view texture_view, enum rt_address_mode address_u, enum rt_address_mode address_v, enum rt_address_mode address_w);
typedef void (*PFN_rtTextureViewAnisotropy)(rt_texture_view texture_view, u32 max_anisotropy);
typedef void (*PFN_rtTextureViewLod)(rt_texture_view texture_view, f32 min_lod, f32 max_lod, f32 lod_bias);

typedef rt_timepoint (*PFN_rtTextureCopy)(rt_texture src_texture, u32 src_mip, rt_texture dst_texture, u32 dst_mip);
typedef rt_timepoint (*PFN_rtTextureData)(rt_texture texture, enum rt_texture_type type, u32 mip, u32 width, u32 height, u32 depth, enum rt_format format, const void* data);
typedef rt_timepoint (*PFN_rtTextureSubcopy)(rt_texture src_texture, u32 src_mip, u32 src_x, u32 src_y, u32 src_z, rt_texture dst_texture, u32 dst_mip, u32 dst_x, u32 dst_y, u32 dst_z, u32 width, u32 height, u32 depth);
typedef rt_timepoint (*PFN_rtTextureSubdata)(rt_texture texture, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, u32 width, u32 height, u32 depth, const void* data);
typedef rt_timepoint (*PFN_rtTextureViewCopyToBuffer)(rt_texture_view texture_view, rt_buffer buffer);
typedef rt_extent_3d (*PFN_rtTextureViewExtent)(rt_texture_view texture_view);
typedef rt_framebuffer (*PFN_rtFramebufferCreate)(void);
typedef void (*PFN_rtFramebufferDestroy)(rt_framebuffer framebuffer);
typedef rt_texture_view (*PFN_rtFramebufferColorView)(rt_framebuffer framebuffer, u32 slot);
typedef void (*PFN_rtFramebufferSetColorView)(rt_framebuffer framebuffer, u32 slot, rt_texture_view view);
typedef void (*PFN_rtFramebufferDepthView)(rt_framebuffer framebuffer, rt_texture_view view);

typedef rt_graphics_program (*PFN_rtGraphicsProgramCreate)(void);
typedef void (*PFN_rtGraphicsProgramDestroy)(rt_graphics_program program);
typedef void (*PFN_rtGraphicsProgramLayout)(rt_graphics_program program, const rt_vertex_layout* layout);
typedef void (*PFN_rtGraphicsProgramSource)(rt_graphics_program program, u64 size, const void* data);
typedef void (*PFN_rtGraphicsProgramRasterState)(rt_graphics_program program, enum rt_cull_mode cull_mode, enum rt_front_face front_face, enum rt_fill_mode fill_mode);
typedef void (*PFN_rtGraphicsProgramBlendState)(rt_graphics_program program, bool enabled, enum rt_blend_factor src_color, enum rt_blend_factor dst_color, enum rt_blend_op color_op, enum rt_blend_factor src_alpha, enum rt_blend_factor dst_alpha, enum rt_blend_op alpha_op);
typedef void (*PFN_rtGraphicsProgramFinalize)(rt_graphics_program program);
typedef void (*PFN_rtGraphicsProgramReset)(rt_graphics_program program);
typedef rt_uniform_location (*PFN_rtGraphicsProgramUniformLocation)(rt_graphics_program program, const char* name);

typedef rt_command_buffer (*PFN_rtCommandBufferCreate)(void);
typedef void (*PFN_rtCommandBufferDestroy)(rt_command_buffer command_buffer);
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

extern PFN_rtTextureCreate rt_rtTextureCreate;
extern PFN_rtTextureDestroy rt_rtTextureDestroy;
extern PFN_rtTextureViewCreate rt_rtTextureViewCreate;
extern PFN_rtTextureViewBind rt_rtTextureViewBind;
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
extern PFN_rtGraphicsProgramLayout rt_rtGraphicsProgramLayout;
extern PFN_rtGraphicsProgramSource rt_rtGraphicsProgramSource;
extern PFN_rtGraphicsProgramRasterState rt_rtGraphicsProgramRasterState;
extern PFN_rtGraphicsProgramBlendState rt_rtGraphicsProgramBlendState;
extern PFN_rtGraphicsProgramFinalize rt_rtGraphicsProgramFinalize;
extern PFN_rtGraphicsProgramReset rt_rtGraphicsProgramReset;
extern PFN_rtGraphicsProgramUniformLocation rt_rtGraphicsProgramUniformLocation;

extern PFN_rtCommandBufferCreate rt_rtCommandBufferCreate;
extern PFN_rtCommandBufferDestroy rt_rtCommandBufferDestroy;
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
** @brief Initialize Rutile with an explicit set of features.
**
** Brings up the loaded backend and any layers, requesting the listed
** features. Features are distinct from extensions: a feature is a contract
** the application announces ahead of time ("I will use presentation"), and
** the backend opts in to the corresponding support during initialization.
** Extensions, in contrast, may be queried and used at any time.
**
** A feature must be enabled here even when the backend would otherwise
** support it - the backend will only set up the matching support for
** features that were explicitly requested.
**
** Currently defined features:
**   - @ref RT_FEATURE_PRESENTATION
**
** If any requested feature is not supported by the loaded backend,
** initialization fails. The error code is set to RT_FEATURE_NOT_SUPPORTED
** and an explanatory message naming the offending feature is recorded; query
** them via @ref rtError and @ref rtErrorMessage.
**
** @param features       Array of feature name strings, or NULL when
**                       @p feature_count is 0.
** @param feature_count  Number of entries in @p features.
**
** @error RT_FEATURE_NOT_SUPPORTED   A requested feature is not supported by
**                                   the loaded backend.
** @error RT_ALREADY_INITIALIZED     rtInit has already been called without
**                                   an intervening rtExit.
** @error RT_INITIALIZATION_FAILED   The backend reported a generic
**                                   initialization failure.
** @error RT_OUT_OF_HOST_MEMORY      Host allocation failed during init.
** @error RT_OUT_OF_DEVICE_MEMORY    Device allocation failed during init.
*/
static inline void rtInit(const char* const* features, u32 feature_count) {
	rt_rtInit(features, feature_count);
}

/*!
** @brief Shut down Rutile.
**
** Releases all backend-owned resources that were created since @ref rtInit
** and returns the runtime to a pre-initialization state. The loader itself
** remains loaded; call @ref rtInit again to bring the runtime back up, or
** @ref rtUnload to also tear down the backend DLLs.
**
** All public handles (buffers, textures, programs, command buffers,
** framebuffers, queues, uniform locations, timepoints) acquired since the
** matching @ref rtInit are invalidated. Destroy your own resources before
** calling.
*/
static inline void rtExit(void) {
	rt_rtExit();
}

/*!
** @brief Install a callback that receives diagnostic output from Rutile.
**
** Routes log/debug messages produced by the backend and active layers
** through @p output instead of the default destination. Pass NULL for
** @p output to restore the default behavior.
**
** @param output     Callback to receive messages, or NULL to restore the
**                   default.
** @param user_data  Opaque pointer forwarded to every invocation of
**                   @p output. Rutile never dereferences or owns it.
**
** @note This entry point is provisional and likely to be reworked. Treat its
**       exact behavior (threading, message format, when it fires) as
**       unstable.
*/
static inline void rtSetOutput(PFN_rtOutput output, void* user_data) {
	rt_rtSetOutput(output, user_data);
}

#ifndef RUTILE_IMPL
/*!
** @brief Return the most recently recorded backend/runtime error code.
**
** Reports the current error state of the loaded backend and layers (not the
** loader itself, which surfaces its errors through @ref rtLoad's return
** value). Returns RT_SUCCESS when no error is recorded.
**
** @return The current error code, or RT_SUCCESS if none.
*/
static inline enum rt_error rtError(void) {
	return rt_rtError();
}

/*!
** @brief Return a human-readable description of the current error.
**
** Pairs with @ref rtError. The returned pointer is owned by Rutile and
** remains valid until the next call that mutates error state (any further
** API call may invalidate it). The string may be empty when the code alone
** is sufficient.
**
** @return Null-terminated message, owned by Rutile.
*/
static inline const char* rtErrorMessage(void) {
	return rt_rtErrorMessage();
}

/*!
** @brief Clear the current backend/runtime error state.
**
** After this call @ref rtError returns RT_SUCCESS and @ref rtErrorMessage
** returns an empty string, until the next operation records a new error.
*/
static inline void rtClearError(void) {
	rt_rtClearError();
}
#endif

/*!
** @brief Return the loaded backend's self-reported name.
**
** Used by the loader to verify backend identity and useful to applications
** that want to log or branch on the active backend. The returned pointer is
** owned by the backend and remains valid until @ref rtUnload.
**
** @return Static null-terminated name string.
*/
static inline const char* rtGetName(void) {
	return rt_rtGetName();
}

/*!
** @brief Query which usages are supported for a given pixel format.
**
** Tells the application what it may legally do with @p format on the loaded
** backend: sample from it, attach it as color/depth, use it as transfer
** source/destination, bind it as storage, and so on. Use this before
** committing texture format choices that depend on the runtime device.
**
** @param format  Format to query.
** @return Bitset of @ref rt_format_usage flags. Returns RT_FORMAT_USAGE_NONE
**         for formats the backend does not support at all (including
**         unrecognized values).
*/
static inline enum rt_format_usage rtQueryFormatCapabilities(enum rt_format format) {
	return rt_rtQueryFormatCapabilities(format);
}

/*!
** @brief Create a buffer handle with no backing storage.
**
** Allocates only the handle. Storage is defined by a subsequent call to
** @ref rtBufferData, which sets size, usage, and mode in one step.
**
** @return New buffer handle, or NULL on failure.
**
** @error RT_OUT_OF_HOST_MEMORY  Host allocation for the handle failed.
*/
static inline rt_buffer rtBufferCreate(void) {
	return rt_rtBufferCreate();
}

/*!
** @brief Destroy a buffer and release its storage.
**
** Safe to call on NULL. Any GPU work still referencing the buffer continues
** to run against the now-zombie resource; the storage is reclaimed once that
** work completes.
**
** @param buffer  Buffer to destroy.
*/
static inline void rtBufferDestroy(rt_buffer buffer) {
	rt_rtBufferDestroy(buffer);
}

/*!
** @brief (Re)define buffer storage and optionally upload initial contents.
**
** Replaces any previously defined storage on @p buffer with a fresh
** allocation of @p size bytes, configured for the given mode and usage
** flags, and (if @p data is non-NULL) uploads @p size bytes from @p data
** into it. If @p data is NULL the contents are zero-initialized.
**
** @param buffer  Buffer to (re)configure.
** @param mode    Storage/update strategy hint (static vs dynamic).
** @param usage   Bitset of RT_BUFFER_USAGE_* flags describing how the buffer
**                will be used (vertex, index, uniform, storage, transfer).
** @param size    New storage size in bytes.
** @param data    Optional source bytes copied into the buffer, or NULL to
**                zero-initialize.
**
** @return Signal that fires when the upload has completed on the GPU.
**
** @error RT_OUT_OF_HOST_MEMORY    Host staging allocation failed.
** @error RT_OUT_OF_DEVICE_MEMORY  Device allocation for the storage failed.
*/
static inline rt_timepoint rtBufferData(rt_buffer buffer, enum rt_buffer_mode mode, enum rt_buffer_usage usage, u64 size, const void* data) {
	return rt_rtBufferData(buffer, mode, usage, size, data);
}

/*!
** @brief Upload bytes into an existing range of a buffer.
**
** Does not change the buffer's size, mode, or usage - only its contents in
** `[offset, offset + size)`. The range must lie fully within the storage
** previously defined by @ref rtBufferData.
**
** @param buffer  Destination buffer.
** @param offset  Byte offset into @p buffer.
** @param size    Number of bytes to upload.
** @param data    Source bytes copied into the buffer range.
**
** @return Signal that fires when the upload has completed on the GPU.
**
** @error RT_OUT_OF_DEVICE_MEMORY  Device-side staging allocation failed.
** @error RT_DEVICE_LOST           The device was lost while scheduling the
**                                 upload.
*/
static inline rt_timepoint rtBufferSubdata(rt_buffer buffer, u64 offset, u64 size, const void* data) {
	return rt_rtBufferSubdata(buffer, offset, size, data);
}

/*!
** @brief Copy bytes out of a buffer into application memory.
**
** Blocking readback. The call returns only after the requested bytes have
** been retrieved from @p buffer and written to @p data; any pending GPU work
** that produces those bytes is waited on internally.
**
** @param buffer  Source buffer.
** @param offset  Byte offset into @p buffer.
** @param size    Number of bytes to copy.
** @param data    Destination memory, at least @p size bytes.
**
** @error RT_OUT_OF_DEVICE_MEMORY  Device-side staging allocation failed.
** @error RT_DEVICE_LOST           The device was lost while waiting for the
**                                 contents to become readable.
*/
static inline void rtBufferRead(rt_buffer buffer, u64 offset, u64 size, void* data) {
	rt_rtBufferRead(buffer, offset, size, data);
}

/*!
** @brief Create a texture handle with no backing storage.
**
** Allocates only the handle. Storage (dimensionality, extents, format, mip
** contents) is defined by a subsequent call to @ref rtTextureData.
**
** @return New texture handle, or NULL on failure.
**
** @error RT_OUT_OF_HOST_MEMORY  Host allocation for the handle failed.
*/
static inline rt_texture rtTextureCreate(void) {
	return rt_rtTextureCreate();
}

/*!
** @brief Destroy a texture and release its storage.
**
** Safe to call on NULL. Any GPU work still referencing the texture continues
** to run against the now-zombie resource; storage is reclaimed once that
** work completes. Texture views that referenced this texture are
** invalidated.
**
** @param texture  Texture to destroy.
*/
static inline void rtTextureDestroy(rt_texture texture) {
	rt_rtTextureDestroy(texture);
}

/*!
** @brief Create an unbound texture view with default sampler state.
**
** A texture view bundles a texture reference with sampler state (filtering,
** addressing, anisotropy, LOD). The view is created unbound; call
** @ref rtTextureViewBind to associate it with a texture.
**
** @return New texture view handle, or NULL on failure.
**
** @error RT_OUT_OF_HOST_MEMORY  Host allocation for the handle failed.
*/
static inline rt_texture_view rtTextureViewCreate(void) {
	return rt_rtTextureViewCreate();
}

/*!
** @brief Bind a texture to an existing texture view.
**
** Re-points @p texture_view at @p texture. The view's sampler state is
** preserved across the rebind. The view becomes invalid if @p texture is
** later destroyed.
**
** @param texture_view  Texture view to update.
** @param texture       Texture to view through @p texture_view.
*/
static inline void rtTextureViewBind(rt_texture_view texture_view, rt_texture texture) {
	rt_rtTextureViewBind(texture_view, texture);
}

/*!
** @brief Destroy a texture view.
**
** Does not affect the underlying texture. Safe to call on NULL.
**
** @param texture_view  Texture view to destroy.
*/
static inline void rtTextureViewDestroy(rt_texture_view texture_view) {
	rt_rtTextureViewDestroy(texture_view);
}

/*!
** @brief Set the filtering used when sampling through a texture view.
**
** @param texture_view  Texture view to update.
** @param mag_filter    Filter used when the footprint covers less than one
**                      texel (magnification).
** @param min_filter    Filter used when the footprint covers more than one
**                      texel (minification).
** @param mip_filter    Filter applied between mip levels, or
**                      RT_MIP_FILTER_NONE to disable mip sampling.
*/
static inline void rtTextureViewFilter(rt_texture_view texture_view, enum rt_filter mag_filter, enum rt_filter min_filter, enum rt_mip_filter mip_filter) {
	rt_rtTextureViewFilter(texture_view, mag_filter, min_filter, mip_filter);
}

/*!
** @brief Set the per-axis address modes used when sampling through a view.
**
** Controls how out-of-range texture coordinates are resolved on each axis
** (clamp, repeat, mirror).
**
** @param texture_view  Texture view to update.
** @param address_u     Address mode for the U (X) axis.
** @param address_v     Address mode for the V (Y) axis.
** @param address_w     Address mode for the W (Z) axis. Ignored for 1D/2D
**                      textures.
*/
static inline void rtTextureViewAddress(rt_texture_view texture_view, enum rt_address_mode address_u, enum rt_address_mode address_v, enum rt_address_mode address_w) {
	rt_rtTextureViewAddress(texture_view, address_u, address_v, address_w);
}

/*!
** @brief Set the maximum anisotropy used when sampling through a view.
**
** @param texture_view    Texture view to update.
** @param max_anisotropy  Maximum anisotropic sample count. 0 maps to the
**                        backend default (no anisotropy).
*/
static inline void rtTextureViewAnisotropy(rt_texture_view texture_view, u32 max_anisotropy) {
	rt_rtTextureViewAnisotropy(texture_view, max_anisotropy);
}

/*!
** @brief Set the LOD selection state used when sampling through a view.
**
** @param texture_view  Texture view to update.
** @param min_lod       Lower clamp on the computed mip level.
** @param max_lod       Upper clamp on the computed mip level.
** @param lod_bias      Bias added to the computed mip level before clamping.
*/
static inline void rtTextureViewLod(rt_texture_view texture_view, f32 min_lod, f32 max_lod, f32 lod_bias) {
	rt_rtTextureViewLod(texture_view, min_lod, max_lod, lod_bias);
}

/*!
** @brief Copy one entire mip level of a texture to another texture's mip.
**
** Copies the full extent of (@p src_texture, @p src_mip) into
** (@p dst_texture, @p dst_mip). The two mip levels must have matching
** extents and compatible formats. Both textures must already have storage
** defined.
**
** @param src_texture  Source texture.
** @param src_mip      Mip level of @p src_texture to read from.
** @param dst_texture  Destination texture.
** @param dst_mip      Mip level of @p dst_texture to write to.
**
** @return Signal that fires when the copy has completed on the GPU.
**
** @error RT_DEVICE_LOST  The device was lost while scheduling the copy.
*/
static inline rt_timepoint rtTextureCopy(rt_texture src_texture, u32 src_mip, rt_texture dst_texture, u32 dst_mip) {
	return rt_rtTextureCopy(src_texture, src_mip, dst_texture, dst_mip);
}

/*!
** @brief (Re)define a mip level of a texture and optionally upload its data.
**
** Configures @p texture to hold a @p type texture of the given @p format,
** with storage for the mip level @p mip sized @p width × @p height ×
** @p depth texels. If @p data is non-NULL, the level is initialized from
** tightly packed source data in @p format; if NULL, the level is
** zero-initialized (matching @ref rtBufferData).
**
** Texture dimensionality (@p type) and format are properties of the texture
** as a whole; supplying different values across calls on the same texture
** redefines it.
**
** @param texture  Texture to (re)configure.
** @param type     Texture dimensionality (1D, 2D, 3D, 1D array, 2D array).
** @param mip      Mip level to define.
** @param width    Storage width in texels.
** @param height   Storage height in texels.
** @param depth    Storage depth in texels (or array length, for array types).
** @param format   Pixel format.
** @param data     Optional source texel data, or NULL.
**
** @return Signal that fires when the upload has completed on the GPU.
**
** @error RT_OUT_OF_HOST_MEMORY    Host staging allocation failed.
** @error RT_OUT_OF_DEVICE_MEMORY  Device allocation for the storage failed.
*/
static inline rt_timepoint rtTextureData(rt_texture texture, enum rt_texture_type type, u32 mip, u32 width, u32 height, u32 depth, enum rt_format format, const void* data) {
	return rt_rtTextureData(texture, type, mip, width, height, depth, format, data);
}

/*!
** @brief Copy a sub-region of one texture mip to a sub-region of another.
**
** Copies a @p width × @p height × @p depth block of texels from
** (@p src_texture, @p src_mip) at offset (@p src_x, @p src_y, @p src_z)
** into (@p dst_texture, @p dst_mip) at offset (@p dst_x, @p dst_y,
** @p dst_z). Both source and destination regions must fit within their
** respective mip extents.
**
** @return Signal that fires when the copy has completed on the GPU.
**
** @error RT_DEVICE_LOST  The device was lost while scheduling the copy.
*/
static inline rt_timepoint rtTextureSubcopy(rt_texture src_texture, u32 src_mip, u32 src_x, u32 src_y, u32 src_z, rt_texture dst_texture, u32 dst_mip, u32 dst_x, u32 dst_y, u32 dst_z, u32 width, u32 height, u32 depth) {
	return rt_rtTextureSubcopy(src_texture, src_mip, src_x, src_y, src_z, dst_texture, dst_mip, dst_x, dst_y, dst_z, width, height, depth);
}

/*!
** @brief Upload tightly packed texel data into a sub-region of a texture mip.
**
** Writes a @p width × @p height × @p depth block of texels from @p data
** into mip level @p mip of @p texture starting at offset (@p offset_x,
** @p offset_y, @p offset_z). The region must fit within the level extents
** previously defined by @ref rtTextureData.
**
** @return Signal that fires when the upload has completed on the GPU.
**
** @error RT_OUT_OF_DEVICE_MEMORY  Device-side staging allocation failed.
** @error RT_DEVICE_LOST           The device was lost while scheduling the
**                                 upload.
*/
static inline rt_timepoint rtTextureSubdata(rt_texture texture, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, u32 width, u32 height, u32 depth, const void* data) {
	return rt_rtTextureSubdata(texture, mip, offset_x, offset_y, offset_z, width, height, depth, data);
}

/*!
** @brief Copy the contents seen through a texture view into a buffer.
**
** Writes the view's texels into @p buffer as tightly packed bytes (no row
** padding) in the source texture's format. The buffer must have storage
** large enough to hold the result.
**
** @param texture_view  Source texture view.
** @param buffer        Destination buffer.
**
** @return Signal that fires when the copy has completed on the GPU.
**
** @error RT_DEVICE_LOST  The device was lost while scheduling the copy.
*/
static inline rt_timepoint rtTextureViewCopyToBuffer(rt_texture_view texture_view, rt_buffer buffer) {
	return rt_rtTextureViewCopyToBuffer(texture_view, buffer);
}

/*!
** @brief Return the texel extent visible through a texture view.
**
** Reports the width/height/depth of the texture (or selected mip) seen
** through @p texture_view, in texels.
**
** @param texture_view  Texture view to query.
** @return The view's extent, or `{0, 0, 0}` if the view is invalid or its
**         backing texture has been destroyed.
*/
static inline rt_extent_3d rtTextureViewExtent(rt_texture_view texture_view) {
	return rt_rtTextureViewExtent(texture_view);
}

/*!
** @brief Create an empty framebuffer with no attachments.
**
** Color slots and the depth slot are populated via
** @ref rtFramebufferSetColorView and @ref rtFramebufferDepthView.
**
** @return New framebuffer handle, or NULL on failure.
**
** @error RT_OUT_OF_HOST_MEMORY  Host allocation for the handle failed.
*/
static inline rt_framebuffer rtFramebufferCreate(void) {
	return rt_rtFramebufferCreate();
}

/*!
** @brief Destroy a framebuffer.
**
** Does not destroy the texture views that were attached to it. Safe to call
** on NULL.
**
** @param framebuffer  Framebuffer to destroy.
*/
static inline void rtFramebufferDestroy(rt_framebuffer framebuffer) {
	rt_rtFramebufferDestroy(framebuffer);
}

/*!
** @brief Return the texture view currently attached to a color slot.
**
** @param framebuffer  Framebuffer to query.
** @param slot         Color attachment slot index.
** @return The bound texture view, or NULL if @p slot is empty.
*/
static inline rt_texture_view rtFramebufferColorView(rt_framebuffer framebuffer, u32 slot) {
	return rt_rtFramebufferColorView(framebuffer, slot);
}

/*!
** @brief Attach (or detach) a texture view to a color slot of a framebuffer.
**
** @param framebuffer  Framebuffer to update.
** @param slot         Color attachment slot index.
** @param view         Texture view to attach, or NULL to clear the slot.
*/
static inline void rtFramebufferSetColorView(rt_framebuffer framebuffer, u32 slot, rt_texture_view view) {
	rt_rtFramebufferSetColorView(framebuffer, slot, view);
}

/*!
** @brief Attach (or detach) the depth/stencil texture view of a framebuffer.
**
** @param framebuffer  Framebuffer to update.
** @param view         Texture view of a depth or depth-stencil format to
**                     attach, or NULL to remove the depth attachment.
*/
static inline void rtFramebufferDepthView(rt_framebuffer framebuffer, rt_texture_view view) {
	rt_rtFramebufferDepthView(framebuffer, view);
}

/*!
** @brief Create an unconfigured graphics program.
**
** Allocates only the handle. The program is built up by setting its vertex
** layout, shader source, raster state, and blend state, then sealed with
** @ref rtGraphicsProgramFinalize before it can be used in a draw call.
**
** @return New graphics program handle, or NULL on failure.
**
** @error RT_OUT_OF_HOST_MEMORY  Host allocation for the handle failed.
*/
static inline rt_graphics_program rtGraphicsProgramCreate(void) {
	return rt_rtGraphicsProgramCreate();
}

/*!
** @brief Destroy a graphics program.
**
** Any uniform locations previously obtained from this program are
** invalidated. Safe to call on NULL.
**
** @param program  Graphics program to destroy.
*/
static inline void rtGraphicsProgramDestroy(rt_graphics_program program) {
	rt_rtGraphicsProgramDestroy(program);
}

/*!
** @brief Set the vertex input layout used by a graphics program.
**
** @p layout describes the stride of a single vertex and the per-attribute
** offsets/formats/names that the vertex shader will consume. The structure
** and its inner arrays are copied; the caller retains ownership.
**
** Pass NULL for @p layout if the program does not read vertex attributes
** (e.g. vertex IDs only).
**
** @param program  Graphics program to configure.
** @param layout   Vertex layout description, or NULL.
*/
static inline void rtGraphicsProgramLayout(rt_graphics_program program, const rt_vertex_layout* layout) {
	rt_rtGraphicsProgramLayout(program, layout);
}

/*!
** @brief Provide the shader code for a graphics program.
**
** @p data points to a compiled RTSL Program binary blob of @p size bytes.
** A single RTSL Program contains every shader entry point (vertex,
** fragment, etc.) the graphics program needs.
**
** Calling this on a finalized program is invalid; reset it first with
** @ref rtGraphicsProgramReset.
**
** @param program  Graphics program to configure.
** @param size     Size of @p data in bytes.
** @param data     Pointer to an RTSL Program binary.
*/
static inline void rtGraphicsProgramSource(rt_graphics_program program, u64 size, const void* data) {
	rt_rtGraphicsProgramSource(program, size, data);
}

/*!
** @brief Set rasterization state for a graphics program.
**
** @param program     Graphics program to configure.
** @param cull_mode   Which triangle faces are culled.
** @param front_face  Winding order considered front-facing.
** @param fill_mode   Triangle fill mode (solid or wireframe).
*/
static inline void rtGraphicsProgramRasterState(rt_graphics_program program, enum rt_cull_mode cull_mode, enum rt_front_face front_face, enum rt_fill_mode fill_mode) {
	rt_rtGraphicsProgramRasterState(program, cull_mode, front_face, fill_mode);
}

/*!
** @brief Set color-blend state for a graphics program.
**
** When @p enabled is false, blending is disabled and the remaining
** factor/op arguments are ignored - fragment shader output is written
** through unmodified.
**
** When @p enabled is true, the output color is blended as
** `color_op(src_color * src, dst_color * dst)` and the output alpha as
** `alpha_op(src_alpha * src, dst_alpha * dst)`.
**
** @param program     Graphics program to configure.
** @param enabled     Master enable for color blending.
** @param src_color   Source factor for the RGB channels.
** @param dst_color   Destination factor for the RGB channels.
** @param color_op    Combining operation for the RGB channels.
** @param src_alpha   Source factor for the alpha channel.
** @param dst_alpha   Destination factor for the alpha channel.
** @param alpha_op    Combining operation for the alpha channel.
*/
static inline void rtGraphicsProgramBlendState(rt_graphics_program program, bool enabled, enum rt_blend_factor src_color, enum rt_blend_factor dst_color, enum rt_blend_op color_op, enum rt_blend_factor src_alpha, enum rt_blend_factor dst_alpha, enum rt_blend_op alpha_op) {
	rt_rtGraphicsProgramBlendState(program, enabled, src_color, dst_color, color_op, src_alpha, dst_alpha, alpha_op);
}

/*!
** @brief Finalize a graphics program for use in draw commands.
**
** Locks in the current configuration (layout, source, raster/blend state)
** and performs any backend-side compilation/linking needed to make the
** program drawable. After this call, the configuration setters
** (@ref rtGraphicsProgramLayout, @ref rtGraphicsProgramSource,
** @ref rtGraphicsProgramRasterState, @ref rtGraphicsProgramBlendState) may
** not be called again until the program is reset with
** @ref rtGraphicsProgramReset.
**
** @ref rtGraphicsProgramUniformLocation may only be called on a finalized
** program. A program must be finalized before being bound by
** @ref rtCmdUseGraphicsProgram.
**
** Errors during compilation/linking are reported through
** @ref rtError / @ref rtErrorMessage (typically RT_SHADER_COMPILATION_FAILED
** or RT_SHADER_LINK_FAILED).
**
** @param program  Graphics program to finalize.
**
** @error RT_SHADER_COMPILATION_FAILED  A shader stage failed to compile.
** @error RT_SHADER_LINK_FAILED         The compiled stages failed to link
**                                      into a pipeline.
** @error RT_OUT_OF_HOST_MEMORY         Host allocation failed during
**                                      finalize.
** @error RT_OUT_OF_DEVICE_MEMORY       Device allocation failed during
**                                      finalize.
*/
static inline void rtGraphicsProgramFinalize(rt_graphics_program program) {
	rt_rtGraphicsProgramFinalize(program);
}

/*!
** @brief Return a finalized graphics program to a configurable state.
**
** Mirror of @ref rtGraphicsProgramFinalize. Discards the backend pipeline
** built by the previous finalize and re-opens the configuration setters.
** All uniform locations previously obtained from the program are
** invalidated.
**
** No-op when the program has not been finalized.
**
** @param program  Graphics program to reset.
*/
static inline void rtGraphicsProgramReset(rt_graphics_program program) {
	rt_rtGraphicsProgramReset(program);
}

/*!
** @brief Look up a named shader uniform on a finalized graphics program.
**
** The returned location handle is owned by @p program and is invalidated
** when the program is reset (@ref rtGraphicsProgramReset) or destroyed
** (@ref rtGraphicsProgramDestroy). Locations are used with the
** `rtCmdUniform*` family to bind resources during command-buffer recording.
**
** @param program  Finalized graphics program to query.
** @param name     Null-terminated uniform name as it appears in the shader.
** @return Uniform location handle, or NULL if no such uniform exists.
*/
static inline rt_uniform_location rtGraphicsProgramUniformLocation(rt_graphics_program program, const char* name) {
	return rt_rtGraphicsProgramUniformLocation(program, name);
}

/*!
** @brief Create an unrecorded command buffer.
**
** Allocates only the handle. The buffer is bound to a specific queue at
** @ref rtCmdBegin time; until then it has no queue affinity.
**
** @return New command buffer handle, or NULL on failure.
**
** @error RT_OUT_OF_HOST_MEMORY  Host allocation for the handle failed.
*/
static inline rt_command_buffer rtCommandBufferCreate(void) {
	return rt_rtCommandBufferCreate();
}

/*!
** @brief Destroy a command buffer.
**
** Work that has already been submitted continues to execute against the
** now-zombie command buffer; backing storage is reclaimed once that work
** completes. Safe to call on NULL.
**
** @param command_buffer  Command buffer to destroy.
*/
static inline void rtCommandBufferDestroy(rt_command_buffer command_buffer) {
	rt_rtCommandBufferDestroy(command_buffer);
}

/*!
** @brief Begin recording a command buffer, binding it to a queue.
**
** Resets any previously recorded contents and starts a fresh recording. The
** queue passed here is the only queue the resulting command buffer may be
** submitted to. The buffer must not already be in the recording state.
**
** Every `rtCmd*` call below requires the target command buffer to be in the
** recording state.
**
** @param command_buffer  Command buffer to record into.
** @param queue           Queue the recording targets.
*/
static inline void rtCmdBegin(rt_command_buffer command_buffer, rt_queue queue) {
	rt_rtCmdBegin(command_buffer, queue);
}

/*!
** @brief Begin a rendering pass into a framebuffer.
**
** Follows Vulkan dynamic-rendering semantics: a rendering pass is opened on
** @p framebuffer; subsequent draw and clear commands target its
** attachments until the matching @ref rtCmdEndRendering. Rendering passes
** must not be nested.
**
** @param command_buffer  Command buffer being recorded.
** @param framebuffer     Framebuffer whose attachments will be rendered to.
*/
static inline void rtCmdBeginRendering(rt_command_buffer command_buffer, rt_framebuffer framebuffer) {
	rt_rtCmdBeginRendering(command_buffer, framebuffer);
}

/*!
** @brief Clear a color attachment of the active framebuffer.
**
** Must be called between @ref rtCmdBeginRendering and
** @ref rtCmdEndRendering. The four components are RGBA in floating-point
** form; the backend converts them to the attachment's format.
**
** @param command_buffer  Command buffer being recorded.
** @param color_index     Color attachment slot to clear.
** @param r  Red.
** @param g  Green.
** @param b  Blue.
** @param a  Alpha.
*/
static inline void rtCmdClearColor(rt_command_buffer command_buffer, u32 color_index, f32 r, f32 g, f32 b, f32 a) {
	rt_rtCmdClearColor(command_buffer, color_index, r, g, b, a);
}

/*!
** @brief Clear the depth aspect of the active framebuffer's depth attachment.
**
** Must be called between @ref rtCmdBeginRendering and
** @ref rtCmdEndRendering.
**
** @param command_buffer  Command buffer being recorded.
** @param depth           Depth clear value (typically in [0, 1]).
*/
static inline void rtCmdClearDepth(rt_command_buffer command_buffer, f32 depth) {
	rt_rtCmdClearDepth(command_buffer, depth);
}

/*!
** @brief Clear the stencil aspect of the active framebuffer's depth attachment.
**
** Must be called between @ref rtCmdBeginRendering and
** @ref rtCmdEndRendering.
**
** @param command_buffer  Command buffer being recorded.
** @param stencil         Stencil clear value.
*/
static inline void rtCmdClearStencil(rt_command_buffer command_buffer, u32 stencil) {
	rt_rtCmdClearStencil(command_buffer, stencil);
}

/*!
** @brief Bind a graphics program for subsequent draw commands.
**
** The program must be finalized (@ref rtGraphicsProgramFinalize). The
** binding stays in effect until the next @ref rtCmdUseGraphicsProgram or
** until the command buffer ends.
**
** @param command_buffer  Command buffer being recorded.
** @param program         Finalized graphics program to bind.
*/
static inline void rtCmdUseGraphicsProgram(rt_command_buffer command_buffer, rt_graphics_program program) {
	rt_rtCmdUseGraphicsProgram(command_buffer, program);
}

/*!
** @brief Set the scissor rectangle used by subsequent draw commands.
**
** Coordinates use the framebuffer's top-left origin and are measured in
** pixels. Binding a graphics program resets the scissor to cover the full
** framebuffer.
**
** @param command_buffer  Command buffer being recorded.
** @param x       Left edge in framebuffer pixels.
** @param y       Top edge in framebuffer pixels.
** @param width   Rectangle width in pixels.
** @param height  Rectangle height in pixels.
*/
static inline void rtCmdSetScissor(rt_command_buffer command_buffer, u32 x, u32 y, u32 width, u32 height) {
	rt_rtCmdSetScissor(command_buffer, x, y, width, height);
}

/*!
** @brief Bind a buffer range to a uniform location for subsequent draws.
**
** Records into the command buffer that the shader uniform at @p location
** reads from `[offset, offset + size)` of @p buffer. @p buffer must have
** been defined with RT_BUFFER_USAGE_UNIFORM in its usage flags, and
** @p location must belong to the currently bound graphics program.
**
** @param command_buffer  Command buffer being recorded.
** @param location        Uniform location obtained from the bound program.
** @param buffer          Buffer to read from.
** @param offset          Byte offset into @p buffer.
** @param size            Number of bytes visible through the binding.
*/
static inline void rtCmdUniformBuffer(rt_command_buffer command_buffer, rt_uniform_location location, rt_buffer buffer, u64 offset, u64 size) {
	rt_rtCmdUniformBuffer(command_buffer, location, buffer, offset, size);
}

/*!
** @brief Bind a sampled texture view to a uniform location for subsequent draws.
**
** Records into the command buffer that the shader sampler at @p location
** samples through @p texture_view. @p location must belong to the
** currently bound graphics program.
**
** @param command_buffer  Command buffer being recorded.
** @param location        Uniform location obtained from the bound program.
** @param texture_view    Texture view to sample.
*/
static inline void rtCmdUniformTexture(rt_command_buffer command_buffer, rt_uniform_location location, rt_texture_view texture_view) {
	rt_rtCmdUniformTexture(command_buffer, location, texture_view);
}

/*!
** @brief Bind a vertex buffer for subsequent draw commands.
**
** The buffer's layout is interpreted using the vertex layout set on the
** currently bound graphics program.
**
** @param command_buffer  Command buffer being recorded.
** @param buffer          Buffer to read vertex data from.
** @param offset          Byte offset into @p buffer where vertex 0 begins.
*/
static inline void rtCmdBindVertexBuffer(rt_command_buffer command_buffer, rt_buffer buffer, u64 offset) {
	rt_rtCmdBindVertexBuffer(command_buffer, buffer, offset);
}

/*!
** @brief Record a non-indexed draw.
**
** Draws @p vertex_count vertices starting at vertex @p first_vertex using
** the currently bound graphics program, vertex buffer, and uniform
** bindings. Must be issued between @ref rtCmdBeginRendering and
** @ref rtCmdEndRendering.
**
** @param command_buffer  Command buffer being recorded.
** @param vertex_count    Number of vertices to draw.
** @param first_vertex    Index of the first vertex (added to vertex IDs).
*/
static inline void rtCmdDraw(rt_command_buffer command_buffer, u32 vertex_count, u32 first_vertex) {
	rt_rtCmdDraw(command_buffer, vertex_count, first_vertex);
}

/*!
** @brief End the current rendering pass.
**
** Closes the rendering pass opened by the most recent
** @ref rtCmdBeginRendering. After this call the command buffer is back in
** the "between passes" state; another @ref rtCmdBeginRendering may follow.
**
** @param command_buffer  Command buffer being recorded.
*/
static inline void rtCmdEndRendering(rt_command_buffer command_buffer) {
	rt_rtCmdEndRendering(command_buffer);
}

/*!
** @brief Finish recording.
**
** Transitions the command buffer out of the recording state. After this
** call it may be submitted to its bound queue via @ref rtQueueSubmit. The
** matching @ref rtCmdBegin must have been issued previously.
**
** @param command_buffer  Command buffer being recorded.
*/
static inline void rtCmdEnd(rt_command_buffer command_buffer) {
	rt_rtCmdEnd(command_buffer);
}

/*!
** @brief Get a queue offering the requested capability.
**
** Returns a borrowed handle to a backend-provided queue with the requested
** capability tier (transfer, compute, graphics). Queue handles are not
** owned by the caller and remain valid until @ref rtExit.
**
** If no queue with the requested capability exists, the function returns
** NULL without recording an error.
**
** @param capability  Required queue capability.
** @return Queue handle, or NULL when no matching queue is available.
*/
static inline rt_queue rtQueueQuery(enum rt_queue_capability capability) {
	return rt_rtQueueQuery(capability);
}

/*!
** @brief Insert a GPU-side wait for a timepoint on a queue.
**
** Inserts a dependency on @p queue: every subsequent submission on
** @p queue will wait for @p timepoint to be signaled before it runs. The
** wait is enforced by the device, not by blocking the CPU. The dependency
** is persistent - subsequent submissions all observe it until the
** timepoint is reached.
**
** Waiting on a zero-initialized/already-reached timepoint records no
** dependency.
**
** @param queue      Queue to insert the wait on.
** @param timepoint  Timepoint that must be reached before subsequent
**                   submissions on @p queue may run.
*/
static inline void rtQueueWait(rt_queue queue, rt_timepoint timepoint) {
	rt_rtQueueWait(queue, timepoint);
}

/*!
** @brief Submit a recorded command buffer to a queue.
**
** Hands @p command_buffer over to @p queue for execution. The submission
** may be deferred internally by the backend; use @ref rtQueueFlush to
** force it to be sent to the GPU. The command buffer must have been
** recorded against @p queue (the queue passed to @ref rtCmdBegin) and
** must have been ended with @ref rtCmdEnd.
**
** The returned timepoint is associated with @p queue and signals once
** this specific submission has completed on the GPU.
**
** @param queue           Queue receiving the work.
** @param command_buffer  Recorded, ended command buffer.
**
** @return Signal that fires when this submission has completed on the GPU.
**
** @error RT_DEVICE_LOST           The device was lost while queuing the
**                                 submission.
** @error RT_OUT_OF_DEVICE_MEMORY  Device allocation failed while queuing
**                                 the submission.
*/
static inline rt_timepoint rtQueueSubmit(rt_queue queue, rt_command_buffer command_buffer) {
	return rt_rtQueueSubmit(queue, command_buffer);
}

/*!
** @brief Flush any pending submissions on a queue to the GPU.
**
** @ref rtQueueSubmit is allowed to defer the actual handoff to the device;
** this call forces every pending submission on @p queue to be sent. Use it
** at the end of a frame, or before any CPU-side wait that requires the
** GPU to be making progress.
**
** @param queue  Queue to flush.
**
** @return Signal that fires when all flushed work has completed on the GPU.
**
** @error RT_DEVICE_LOST           The device was lost while flushing.
** @error RT_OUT_OF_DEVICE_MEMORY  Device allocation failed during the
**                                 flush.
*/
static inline rt_timepoint rtQueueFlush(rt_queue queue) {
	return rt_rtQueueFlush(queue);
}

/*!
** @brief Block the CPU until a timepoint is reached.
**
** Returns once the GPU has signaled @p timepoint. The queue stored inside
** @p timepoint defines the synchronization domain. A zero-initialized or
** already-reached timepoint returns immediately.
**
** @param timepoint  Timepoint to wait for.
*/
static inline void rtTimepointWait(rt_timepoint timepoint) {
	rt_rtTimepointWait(timepoint);
}

/*!
** @brief Non-blocking check of whether a timepoint has been reached.
**
** Polls the current state of @p timepoint. A zero-initialized timepoint is
** always considered reached.
**
** @param timepoint  Timepoint to query.
** @return true if the timepoint has been signaled (or is the always-reached
**         sentinel); false otherwise.
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
		NULL
	);
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

PFN_rtTextureCreate rt_rtTextureCreate = NULL;
PFN_rtTextureDestroy rt_rtTextureDestroy = NULL;
PFN_rtTextureViewCreate rt_rtTextureViewCreate = NULL;
PFN_rtTextureViewBind rt_rtTextureViewBind = NULL;
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
PFN_rtGraphicsProgramLayout rt_rtGraphicsProgramLayout = NULL;
PFN_rtGraphicsProgramSource rt_rtGraphicsProgramSource = NULL;
PFN_rtGraphicsProgramRasterState rt_rtGraphicsProgramRasterState = NULL;
PFN_rtGraphicsProgramBlendState rt_rtGraphicsProgramBlendState = NULL;
PFN_rtGraphicsProgramFinalize rt_rtGraphicsProgramFinalize = NULL;
PFN_rtGraphicsProgramReset rt_rtGraphicsProgramReset = NULL;
PFN_rtGraphicsProgramUniformLocation rt_rtGraphicsProgramUniformLocation = NULL;

PFN_rtCommandBufferCreate rt_rtCommandBufferCreate = NULL;
PFN_rtCommandBufferDestroy rt_rtCommandBufferDestroy = NULL;
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

	RT__CORE_RESOLVE(rtTextureCreate);
	RT__CORE_RESOLVE(rtTextureDestroy);
	RT__CORE_RESOLVE(rtTextureViewCreate);
	RT__CORE_RESOLVE(rtTextureViewBind);
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
	RT__CORE_RESOLVE(rtGraphicsProgramLayout);
	RT__CORE_RESOLVE(rtGraphicsProgramSource);
	RT__CORE_RESOLVE(rtGraphicsProgramRasterState);
	RT__CORE_RESOLVE(rtGraphicsProgramBlendState);
	RT__CORE_RESOLVE(rtGraphicsProgramFinalize);
	RT__CORE_RESOLVE(rtGraphicsProgramReset);
	RT__CORE_RESOLVE(rtGraphicsProgramUniformLocation);

	RT__CORE_RESOLVE(rtCommandBufferCreate);
	RT__CORE_RESOLVE(rtCommandBufferDestroy);
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

#define RT__CORE_TRY_RESOLVE(name)       \
	do {                                 \
		rt_proc_t _p = rtGetProc(#name); \
		if (_p) {                        \
			rt_##name = (PFN_##name)_p;  \
		}                                \
	} while (0)

static void rt__load_core_development(void) {
	RT__CORE_TRY_RESOLVE(rtInit);
	RT__CORE_TRY_RESOLVE(rtExit);
	RT__CORE_TRY_RESOLVE(rtSetOutput);
	RT__CORE_TRY_RESOLVE(rtError);
	RT__CORE_TRY_RESOLVE(rtErrorMessage);
	RT__CORE_TRY_RESOLVE(rtClearError);
	RT__CORE_TRY_RESOLVE(rtGetName);
	RT__CORE_TRY_RESOLVE(rtQueryFormatCapabilities);

	RT__CORE_TRY_RESOLVE(rtBufferCreate);
	RT__CORE_TRY_RESOLVE(rtBufferDestroy);
	RT__CORE_TRY_RESOLVE(rtBufferData);
	RT__CORE_TRY_RESOLVE(rtBufferSubdata);
	RT__CORE_TRY_RESOLVE(rtBufferRead);

	RT__CORE_TRY_RESOLVE(rtTextureCreate);
	RT__CORE_TRY_RESOLVE(rtTextureDestroy);
	RT__CORE_TRY_RESOLVE(rtTextureViewCreate);
	RT__CORE_TRY_RESOLVE(rtTextureViewBind);
	RT__CORE_TRY_RESOLVE(rtTextureViewDestroy);
	RT__CORE_TRY_RESOLVE(rtTextureViewFilter);
	RT__CORE_TRY_RESOLVE(rtTextureViewAddress);
	RT__CORE_TRY_RESOLVE(rtTextureViewAnisotropy);
	RT__CORE_TRY_RESOLVE(rtTextureViewLod);

	RT__CORE_TRY_RESOLVE(rtTextureCopy);
	RT__CORE_TRY_RESOLVE(rtTextureData);
	RT__CORE_TRY_RESOLVE(rtTextureSubcopy);
	RT__CORE_TRY_RESOLVE(rtTextureSubdata);
	RT__CORE_TRY_RESOLVE(rtTextureViewCopyToBuffer);
	RT__CORE_TRY_RESOLVE(rtTextureViewExtent);
	RT__CORE_TRY_RESOLVE(rtFramebufferCreate);
	RT__CORE_TRY_RESOLVE(rtFramebufferDestroy);
	RT__CORE_TRY_RESOLVE(rtFramebufferColorView);
	RT__CORE_TRY_RESOLVE(rtFramebufferSetColorView);
	RT__CORE_TRY_RESOLVE(rtFramebufferDepthView);

	RT__CORE_TRY_RESOLVE(rtGraphicsProgramCreate);
	RT__CORE_TRY_RESOLVE(rtGraphicsProgramDestroy);
	RT__CORE_TRY_RESOLVE(rtGraphicsProgramLayout);
	RT__CORE_TRY_RESOLVE(rtGraphicsProgramSource);
	RT__CORE_TRY_RESOLVE(rtGraphicsProgramRasterState);
	RT__CORE_TRY_RESOLVE(rtGraphicsProgramBlendState);
	RT__CORE_TRY_RESOLVE(rtGraphicsProgramFinalize);
	RT__CORE_TRY_RESOLVE(rtGraphicsProgramReset);
	RT__CORE_TRY_RESOLVE(rtGraphicsProgramUniformLocation);

	RT__CORE_TRY_RESOLVE(rtCommandBufferCreate);
	RT__CORE_TRY_RESOLVE(rtCommandBufferDestroy);
	RT__CORE_TRY_RESOLVE(rtCmdBegin);
	RT__CORE_TRY_RESOLVE(rtCmdBeginRendering);
	RT__CORE_TRY_RESOLVE(rtCmdClearColor);
	RT__CORE_TRY_RESOLVE(rtCmdClearDepth);
	RT__CORE_TRY_RESOLVE(rtCmdClearStencil);
	RT__CORE_TRY_RESOLVE(rtCmdUseGraphicsProgram);
	RT__CORE_TRY_RESOLVE(rtCmdSetScissor);
	RT__CORE_TRY_RESOLVE(rtCmdUniformBuffer);
	RT__CORE_TRY_RESOLVE(rtCmdUniformTexture);
	RT__CORE_TRY_RESOLVE(rtCmdBindVertexBuffer);
	RT__CORE_TRY_RESOLVE(rtCmdDraw);
	RT__CORE_TRY_RESOLVE(rtCmdEndRendering);
	RT__CORE_TRY_RESOLVE(rtCmdEnd);

	RT__CORE_TRY_RESOLVE(rtQueueQuery);
	RT__CORE_TRY_RESOLVE(rtQueueWait);
	RT__CORE_TRY_RESOLVE(rtQueueSubmit);
	RT__CORE_TRY_RESOLVE(rtQueueFlush);
	RT__CORE_TRY_RESOLVE(rtTimepointWait);
	RT__CORE_TRY_RESOLVE(rtTimepointReached);
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

enum rt_error rtLoadDevelopment(const char* backend_name, const char* const* layer_names, u32 layer_count) {
	rt__loader_clear_error_state();
	if (layer_count > RT__MAX_LAYERS) {
		rt__loader_set_errorf(RT_IMPROPER_USAGE, "rtLoadDevelopment requested too many layers: %u", layer_count);
		rt__loader_print_error();
		return RT_IMPROPER_USAGE;
	}
	if (layer_count && !layer_names) {
		rt__loader_set_errorf(RT_IMPROPER_USAGE, "rtLoadDevelopment layer_count is %u but layer_names is NULL", layer_count);
		rt__loader_print_error();
		return RT_IMPROPER_USAGE;
	}

	rtUnload();

	char message[1024];
	enum rt_error err = RT_SUCCESS;
	if (backend_name) {
		err = rt__load_backend_named(backend_name, &rt__backend_dll, message, sizeof(message));
		if (err != RT_SUCCESS) {
			rt__loader_set_errorf(RT_SUCCESS, "development loader skipped backend %s", backend_name);
			rt__loader_print_error();
		}
	}

	if (!rt__backend_dll) {
		rt_loaded = false;
		return RT_SUCCESS;
	}

	u32 loaded_layers = 0;
	for (u32 i = 0; i < layer_count; i++) {
		err = rt__load_layer_named(layer_names[i], &rt__layer_links[loaded_layers], message, sizeof(message));
		if (err != RT_SUCCESS) {
			rt__loader_set_errorf(RT_SUCCESS, "development loader skipped layer %s", layer_names[i]);
			rt__loader_print_error();
			continue;
		}
		loaded_layers++;
	}

	rt_proc_chain chain;
	chain.get_proc = rt__backend_proc;
	for (u32 i = 0; i < loaded_layers; i++) {
		rt__layer_set_next(i, chain);
		chain = rt__layer_links[i].chain;
	}
	rt__chain = chain;

	rt__load_core_development();
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
	rt_rtGraphicsProgramLayout = NULL;
	rt_rtGraphicsProgramSource = NULL;
	rt_rtGraphicsProgramRasterState = NULL;
	rt_rtGraphicsProgramBlendState = NULL;
	rt_rtGraphicsProgramFinalize = NULL;
	rt_rtGraphicsProgramReset = NULL;
	rt_rtGraphicsProgramUniformLocation = NULL;
	rt_rtCommandBufferCreate = NULL;
	rt_rtCommandBufferDestroy = NULL;
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
/* See the matching declarations outside #ifdef RUTILE_IMPL for documentation. */
static inline enum rt_error rtError(void) {
	return rt_rtError();
}

static inline const char* rtErrorMessage(void) {
	return rt_rtErrorMessage();
}

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
