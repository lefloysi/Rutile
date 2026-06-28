#ifndef RTLOG_PROCS_H
#define RTLOG_PROCS_H

#define RT_NO_API_WRAPPERS
#include "rt_ext_compute.h"
#include "rt_ext_glfw.h"
#include "rt_ext_swapchain.h"
#include "rutile.h"
#undef RT_NO_API_WRAPPERS

extern PFN_rtInit next_rtInit;
extern PFN_rtExit next_rtExit;
extern PFN_rtSetOutput next_rtSetOutput;
extern PFN_rtError next_rtError;
extern PFN_rtErrorMessage next_rtErrorMessage;
extern PFN_rtClearError next_rtClearError;
extern PFN_rtGetName next_rtGetName;
extern PFN_rtQueryFormatCapabilities next_rtQueryFormatCapabilities;
extern PFN_rtBufferCreate next_rtBufferCreate;
extern PFN_rtBufferDestroy next_rtBufferDestroy;
extern PFN_rtBufferData next_rtBufferData;
extern PFN_rtBufferSubdata next_rtBufferSubdata;
extern PFN_rtBufferRead next_rtBufferRead;
extern PFN_rtTextureCreate next_rtTextureCreate;
extern PFN_rtTextureDestroy next_rtTextureDestroy;
extern PFN_rtTextureViewCreate next_rtTextureViewCreate;
extern PFN_rtTextureViewBind next_rtTextureViewBind;
extern PFN_rtTextureViewDestroy next_rtTextureViewDestroy;
extern PFN_rtTextureViewFilter next_rtTextureViewFilter;
extern PFN_rtTextureViewAddress next_rtTextureViewAddress;
extern PFN_rtTextureViewAnisotropy next_rtTextureViewAnisotropy;
extern PFN_rtTextureViewLod next_rtTextureViewLod;
extern PFN_rtTextureCopy next_rtTextureCopy;
extern PFN_rtTextureData next_rtTextureData;
extern PFN_rtTextureSubcopy next_rtTextureSubcopy;
extern PFN_rtTextureSubdata next_rtTextureSubdata;
extern PFN_rtTextureViewCopyToBuffer next_rtTextureViewCopyToBuffer;
extern PFN_rtTextureViewExtent next_rtTextureViewExtent;
extern PFN_rtFramebufferCreate next_rtFramebufferCreate;
extern PFN_rtFramebufferDestroy next_rtFramebufferDestroy;
extern PFN_rtFramebufferColorView next_rtFramebufferColorView;
extern PFN_rtFramebufferSetColorView next_rtFramebufferSetColorView;
extern PFN_rtFramebufferDepthView next_rtFramebufferDepthView;
extern PFN_rtGraphicsProgramCreate next_rtGraphicsProgramCreate;
extern PFN_rtGraphicsProgramDestroy next_rtGraphicsProgramDestroy;
extern PFN_rtGraphicsProgramLayout next_rtGraphicsProgramLayout;
extern PFN_rtGraphicsProgramSource next_rtGraphicsProgramSource;
extern PFN_rtGraphicsProgramRasterState next_rtGraphicsProgramRasterState;
extern PFN_rtGraphicsProgramBlendState next_rtGraphicsProgramBlendState;
extern PFN_rtGraphicsProgramFinalize next_rtGraphicsProgramFinalize;
extern PFN_rtGraphicsProgramReset next_rtGraphicsProgramReset;
extern PFN_rtGraphicsProgramUniformLocation next_rtGraphicsProgramUniformLocation;
extern PFN_rtComputeProgramCreate next_rtComputeProgramCreate;
extern PFN_rtComputeProgramDestroy next_rtComputeProgramDestroy;
extern PFN_rtComputeProgramShader next_rtComputeProgramShader;
extern PFN_rtComputeProgramLink next_rtComputeProgramLink;
extern PFN_rtCommandBufferCreate next_rtCommandBufferCreate;
extern PFN_rtCommandBufferDestroy next_rtCommandBufferDestroy;
extern PFN_rtCmdBegin next_rtCmdBegin;
extern PFN_rtCmdBeginRendering next_rtCmdBeginRendering;
extern PFN_rtCmdClearColor next_rtCmdClearColor;
extern PFN_rtCmdClearDepth next_rtCmdClearDepth;
extern PFN_rtCmdClearStencil next_rtCmdClearStencil;
extern PFN_rtCmdUseGraphicsProgram next_rtCmdUseGraphicsProgram;
extern PFN_rtCmdSetScissor next_rtCmdSetScissor;
extern PFN_rtCmdUseComputeProgram next_rtCmdUseComputeProgram;
extern PFN_rtCmdUniformBuffer next_rtCmdUniformBuffer;
extern PFN_rtCmdUniformTexture next_rtCmdUniformTexture;
extern PFN_rtCmdStorageBuffer next_rtCmdStorageBuffer;
extern PFN_rtCmdStorageTexture next_rtCmdStorageTexture;
extern PFN_rtCmdComputeBarrier next_rtCmdComputeBarrier;
extern PFN_rtCmdBindVertexBuffer next_rtCmdBindVertexBuffer;
extern PFN_rtCmdDraw next_rtCmdDraw;
extern PFN_rtCmdDispatch next_rtCmdDispatch;
extern PFN_rtCmdEndRendering next_rtCmdEndRendering;
extern PFN_rtCmdEnd next_rtCmdEnd;
extern PFN_rtQueueQuery next_rtQueueQuery;
extern PFN_rtQueueWait next_rtQueueWait;
extern PFN_rtQueueSubmit next_rtQueueSubmit;
extern PFN_rtQueueFlush next_rtQueueFlush;
extern PFN_rtTimepointWait next_rtTimepointWait;
extern PFN_rtTimepointReached next_rtTimepointReached;
extern PFN_rtSwapchainCreate next_rtSwapchainCreate;
extern PFN_rtSwapchainDestroy next_rtSwapchainDestroy;
extern PFN_rtSwapchainResize next_rtSwapchainResize;
extern PFN_rtSwapchainAcquire next_rtSwapchainAcquire;
extern PFN_rtSwapchainPresent next_rtSwapchainPresent;
extern PFN_rtSwapchainBindWindowGLFW next_rtSwapchainBindWindowGLFW;

void rtlog_set_output(PFN_rtOutput output, void* user_data);
void rtlog_call(const char* name);
void rtlog_printf(const char* format, ...);
u64 rtlog_now_ns(void);
const char* rtlog_elapsed(u64 start_ns);
const char* rtlog_pointer(const void* pointer);
const char* rtlog_timepoint(rt_timepoint timepoint);
void rtlog_error(const char* name);

void rtlog_rtInit(const char* const* features, u32 feature_count);
void rtlog_rtExit(void);
void rtlog_rtSetOutput(PFN_rtOutput output, void* user_data);
enum rt_error rtlog_rtError(void);
const char* rtlog_rtErrorMessage(void);
void rtlog_rtClearError(void);
const char* rtlog_rtGetName(void);
enum rt_format_usage rtlog_rtQueryFormatCapabilities(enum rt_format format);

rt_buffer rtlog_rtBufferCreate(void);
void rtlog_rtBufferDestroy(rt_buffer buffer);
rt_timepoint rtlog_rtBufferData(rt_buffer buffer, enum rt_buffer_mode mode, enum rt_buffer_usage usage, u64 size, const void* data);
rt_timepoint rtlog_rtBufferSubdata(rt_buffer buffer, u64 offset, u64 size, const void* data);
void rtlog_rtBufferRead(rt_buffer buffer, u64 offset, u64 size, void* data);

rt_texture rtlog_rtTextureCreate(void);
void rtlog_rtTextureDestroy(rt_texture texture);
rt_texture_view rtlog_rtTextureViewCreate(void);
void rtlog_rtTextureViewBind(rt_texture_view texture_view, rt_texture texture);
void rtlog_rtTextureViewDestroy(rt_texture_view texture_view);
void rtlog_rtTextureViewFilter(rt_texture_view texture_view, enum rt_filter mag_filter, enum rt_filter min_filter, enum rt_mip_filter mip_filter);
void rtlog_rtTextureViewAddress(rt_texture_view texture_view, enum rt_address_mode address_u, enum rt_address_mode address_v, enum rt_address_mode address_w);
void rtlog_rtTextureViewAnisotropy(rt_texture_view texture_view, u32 max_anisotropy);
void rtlog_rtTextureViewLod(rt_texture_view texture_view, f32 min_lod, f32 max_lod, f32 lod_bias);
rt_timepoint rtlog_rtTextureCopy(rt_queue queue, rt_texture src_texture, u32 src_mip, rt_texture dst_texture, u32 dst_mip);
rt_timepoint rtlog_rtTextureData(rt_texture texture, enum rt_texture_type type, u32 mip, u32 width, u32 height, u32 depth, enum rt_format format, const void* data);
rt_timepoint rtlog_rtTextureSubcopy(rt_texture src_texture, u32 src_mip, u32 src_x, u32 src_y, u32 src_z, rt_texture dst_texture, u32 dst_mip, u32 dst_x, u32 dst_y, u32 dst_z, u32 width, u32 height, u32 depth);
rt_timepoint rtlog_rtTextureSubdata(rt_texture texture, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, u32 width, u32 height, u32 depth, const void* data);
rt_timepoint rtlog_rtTextureViewCopyToBuffer(rt_texture_view texture_view, rt_buffer buffer);
rt_extent_3d rtlog_rtTextureViewExtent(rt_texture_view texture_view);
rt_framebuffer rtlog_rtFramebufferCreate(void);
void rtlog_rtFramebufferDestroy(rt_framebuffer framebuffer);
rt_texture_view rtlog_rtFramebufferColorView(rt_framebuffer framebuffer, u32 slot);
void rtlog_rtFramebufferSetColorView(rt_framebuffer framebuffer, u32 slot, rt_texture_view view);
void rtlog_rtFramebufferDepthView(rt_framebuffer framebuffer, rt_texture_view view);

rt_graphics_program rtlog_rtGraphicsProgramCreate(void);
void rtlog_rtGraphicsProgramDestroy(rt_graphics_program program);
void rtlog_rtGraphicsProgramLayout(rt_graphics_program program, const rt_vertex_layout* layout);
void rtlog_rtGraphicsProgramSource(rt_graphics_program program, u64 size, const void* data);
void rtlog_rtGraphicsProgramRasterState(rt_graphics_program program, enum rt_cull_mode cull_mode, enum rt_front_face front_face, enum rt_fill_mode fill_mode);
void rtlog_rtGraphicsProgramBlendState(rt_graphics_program program, bool enabled, enum rt_blend_factor src_color, enum rt_blend_factor dst_color, enum rt_blend_op color_op, enum rt_blend_factor src_alpha, enum rt_blend_factor dst_alpha, enum rt_blend_op alpha_op);
void rtlog_rtGraphicsProgramFinalize(rt_graphics_program program);
void rtlog_rtGraphicsProgramReset(rt_graphics_program program);
rt_uniform_location rtlog_rtGraphicsProgramUniformLocation(rt_graphics_program program, const char* name);
rt_compute_program rtlog_rtComputeProgramCreate(void);
void rtlog_rtComputeProgramDestroy(rt_compute_program program);
void rtlog_rtComputeProgramShader(rt_compute_program program, u64 size, const void* data);
void rtlog_rtComputeProgramLink(rt_compute_program program);

rt_command_buffer rtlog_rtCommandBufferCreate(void);
void rtlog_rtCommandBufferDestroy(rt_command_buffer command_buffer);
void rtlog_rtCmdBegin(rt_command_buffer command_buffer, rt_queue queue);
void rtlog_rtCmdBeginRendering(rt_command_buffer command_buffer, rt_framebuffer framebuffer);
void rtlog_rtCmdClearColor(rt_command_buffer command_buffer, u32 color_index, f32 r, f32 g, f32 b, f32 a);
void rtlog_rtCmdClearDepth(rt_command_buffer command_buffer, f32 depth);
void rtlog_rtCmdClearStencil(rt_command_buffer command_buffer, u32 stencil);
void rtlog_rtCmdUseGraphicsProgram(rt_command_buffer command_buffer, rt_graphics_program program);
void rtlog_rtCmdSetScissor(rt_command_buffer command_buffer, u32 x, u32 y, u32 width, u32 height);
void rtlog_rtCmdUseComputeProgram(rt_command_buffer command_buffer, rt_compute_program program);
void rtlog_rtCmdUniformBuffer(rt_command_buffer command_buffer, rt_uniform_location location, rt_buffer buffer, u64 offset, u64 size);
void rtlog_rtCmdUniformTexture(rt_command_buffer command_buffer, rt_uniform_location location, rt_texture_view texture_view);
void rtlog_rtCmdStorageBuffer(rt_command_buffer command_buffer, u32 binding, rt_buffer buffer, u64 offset, u64 size);
void rtlog_rtCmdStorageTexture(rt_command_buffer command_buffer, u32 binding, rt_texture_view texture_view);
void rtlog_rtCmdComputeBarrier(rt_command_buffer command_buffer);
void rtlog_rtCmdBindVertexBuffer(rt_command_buffer command_buffer, rt_buffer buffer, u64 offset);
void rtlog_rtCmdDraw(rt_command_buffer command_buffer, u32 vertex_count, u32 first_vertex);
void rtlog_rtCmdDispatch(rt_command_buffer command_buffer, u32 group_count_x, u32 group_count_y, u32 group_count_z);
void rtlog_rtCmdEndRendering(rt_command_buffer command_buffer);
void rtlog_rtCmdEnd(rt_command_buffer command_buffer);

rt_queue rtlog_rtQueueQuery(enum rt_queue_capability capability);
void rtlog_rtQueueWait(rt_queue queue, rt_timepoint timepoint);
rt_timepoint rtlog_rtQueueSubmit(rt_queue queue, rt_command_buffer command_buffer);
rt_timepoint rtlog_rtQueueFlush(rt_queue queue);
void rtlog_rtTimepointWait(rt_timepoint timepoint);
bool rtlog_rtTimepointReached(rt_timepoint timepoint);

rt_swapchain rtlog_rtSwapchainCreate(void);
void rtlog_rtSwapchainDestroy(rt_swapchain swapchain);
void rtlog_rtSwapchainResize(rt_swapchain swapchain, u32 width, u32 height);
void rtlog_rtSwapchainSetVsync(rt_swapchain swapchain, bool enabled);
rt_swapchain_acquire_result rtlog_rtSwapchainAcquire(rt_swapchain swapchain);
void rtlog_rtSwapchainPresent(rt_swapchain swapchain, rt_timepoint rendered);
void rtlog_rtSwapchainBindWindowGLFW(rt_swapchain swapchain, GLFWwindow* window);

#endif
