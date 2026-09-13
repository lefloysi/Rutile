#include "rt_antialias.h"
#include "rt_antialias_program.inc"
#include <stdlib.h>

struct rt_antialias_t {
    rt_program program;
    rt_buffer vertices;
    rt_texture texture;
    rt_texture_view view;
    rt_framebuffer framebuffer;
    rt_sampler sampler;
    rt_location input, source, pixel;
    usize width, height;
    bool uploaded;
};

rt_antialias rtAntialiasCreate(void) {
    rt_antialias aa = calloc(1,sizeof(*aa));
    if (!aa) { return NULL; }
    const rt_vertex_attribute attribute = {"point",0,RT_RG32_SFLOAT};
    const rt_vertex_input input = {&attribute,1,sizeof(f32)*2,RT_VERTEX_RATE_VERTEX};
    const rt_vertex_layout layout = {&input,1};
    aa->program = rtProgramCreate();
    rtProgramSource(aa->program,"antialias",rt_antialias_program,sizeof(rt_antialias_program));
    rtProgramSetLayout(aa->program,&layout);
    rtProgramFinalize(aa->program);
    aa->input = rtProgramInputLocation(aa->program,&attribute,1);
    aa->source = rtProgramUniformLocation(aa->program,"source");
    aa->pixel = rtProgramUniformLocation(aa->program,"pixel");
    aa->vertices = rtBufferCreate();
    rtBufferResize(aa->vertices,RT_DEVICE_MEMORY,sizeof(f32)*6);
    aa->texture = rtTextureCreate();
    aa->view = rtTextureViewCreate();
    aa->framebuffer = rtFramebufferCreate();
    aa->sampler = rtSamplerCreate();
    rtSamplerSetFilter(aa->sampler,RT_FILTER_LINEAR,RT_FILTER_LINEAR,RT_MIP_FILTER_NONE);
    rtSamplerSetAddress(aa->sampler,RT_ADDRESS_CLAMP,RT_ADDRESS_CLAMP,RT_ADDRESS_CLAMP);
    return aa;
}

void rtAntialiasDestroy(rt_antialias aa) {
    if (!aa) { return; }
    rtFramebufferDestroy(aa->framebuffer);
    rtTextureViewDestroy(aa->view);
    rtTextureDestroy(aa->texture);
    rtSamplerDestroy(aa->sampler);
    rtBufferDestroy(aa->vertices);
    rtProgramDestroy(aa->program);
    free(aa);
}

void rtAntialiasBegin(rt_antialias aa, rt_command_buffer commands, usize width, usize height) {
    if (!aa || !width || !height) { return; }
    if (aa->width != width || aa->height != height) {
        aa->width = width; aa->height = height;
        rtTextureResize(aa->texture,RT_TEXTURE_2D,RT_RGBA8_UNORM,(rt_extent_3d){width,height,1},1);
        rtTextureViewSetTexture(aa->view,aa->texture);
        rtFramebufferSetColorView(aa->framebuffer,aa->view,NULL);
    }
    if (!aa->uploaded) {
        static const f32 vertices[] = {-1,-1,3,-1,-1,3};
        rtCmdBufferData(commands,aa->vertices,(rt_buffer_range){sizeof(vertices),0},(const u08*)vertices);
        rtCmdBufferBarrier(commands,aa->vertices,(rt_buffer_range){sizeof(vertices),0},(rt_access){RT_STAGE_TRANSFER,RT_ACCESS_WRITE},(rt_access){RT_STAGE_VERTEX,RT_ACCESS_READ});
        aa->uploaded = true;
    }
    rtCmdBeginRendering(commands,aa->framebuffer);
    rtCmdSetViewport(commands,0,0,width,height,0,1);
    rtCmdSetScissor(commands,0,0,width,height);
    rtCmdClearColor(commands,NULL,0,0,0,1);
    rtCmdClear(commands,RT_CLEAR_COLOR);
}

void rtAntialiasResolve(rt_antialias aa, rt_command_buffer commands, rt_framebuffer destination) {
    if (!aa || !aa->width || !aa->height) { return; }
    const f32 pixel[] = {1.f/(f32)aa->width,1.f/(f32)aa->height};
    rtCmdEndRendering(commands);
    rtCmdTextureBarrier(commands,aa->texture,(rt_texture_range){RT_TEXTURE_ASPECT_COLOR,0,1,0,1,{aa->width,aa->height,1},{0,0,0}},(rt_access){RT_STAGE_COLOR_ATTACHMENT,RT_ACCESS_WRITE},(rt_access){RT_STAGE_FRAGMENT,RT_ACCESS_READ});
    rtCmdBeginRendering(commands,destination);
    rtCmdSetViewport(commands,0,0,aa->width,aa->height,0,1);
    rtCmdSetScissor(commands,0,0,aa->width,aa->height);
    rtCmdUseProgram(commands,aa->program);
    rtCmdUniformData(commands,aa->pixel,(const u08*)pixel,sizeof(pixel));
    rtCmdBindTexture(commands,aa->source,aa->view);
    rtCmdBindSampler(commands,aa->source,aa->sampler);
    rtCmdVertexBuffer(commands,aa->input,aa->vertices,(rt_buffer_range){sizeof(f32)*6,0});
    rtCmdDraw(commands,3,0);
}
