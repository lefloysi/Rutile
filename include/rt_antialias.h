#ifndef RT_ANTIALIAS_H
#define RT_ANTIALIAS_H
#include "rutile.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct rt_antialias_t* rt_antialias;

/* Backend-independent edge anti-aliasing. Create after rtInit, destroy before
 * rtUnload, and keep it alive until all commands using it have completed.
 * Begin starts rendering to an owned full-resolution target. Resolve ends that
 * pass and starts the destination pass, leaving it open for further rendering.
 * Begin/Resolve replace the graphics state; they do not preserve it. */
rt_antialias rtAntialiasCreate(void);
void rtAntialiasDestroy(rt_antialias antialias);
void rtAntialiasBegin(rt_antialias antialias, rt_command_buffer commands, usize width, usize height);
void rtAntialiasResolve(rt_antialias antialias, rt_command_buffer commands, rt_framebuffer destination);

#ifdef __cplusplus
}
#endif
#endif
