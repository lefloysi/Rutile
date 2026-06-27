#ifndef RT_LAYER_VALIDATION_H
#define RT_LAYER_VALIDATION_H

#ifndef RUTILE_LOADER_ONLY
#define RUTILE_LOADER_ONLY
#include "rutile.h"
#undef RUTILE_LOADER_ONLY
#else
#include "rutile.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

RT_EXPORT const char* rtLayerGetName(void);
RT_EXPORT void        rtLayerSetNext(rt_proc_chain next);

#ifdef __cplusplus
}
#endif
#endif
