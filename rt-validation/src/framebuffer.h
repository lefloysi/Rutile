#ifndef RTVAL_FRAMEBUFFER_H
#define RTVAL_FRAMEBUFFER_H

#include "handles.h"

struct rtval_framebuffer {
	rt_framebuffer backend;
	bool owns_backend;
};

struct rtval_framebuffer* rtval_framebuffer_create(void);
struct rtval_framebuffer* rtval_framebuffer_wrap(rt_framebuffer backend);
void                      rtval_framebuffer_destroy(struct rtval_framebuffer* framebuffer);
rt_texture_view           rtval_framebuffer_color_view(struct rtval_framebuffer* framebuffer, u32 slot);
void                      rtval_framebuffer_set_color_view(struct rtval_framebuffer* framebuffer, u32 slot, struct rtval_texture_view* view);
void                      rtval_framebuffer_set_depth_view(struct rtval_framebuffer* framebuffer, struct rtval_texture_view* view);

#endif
