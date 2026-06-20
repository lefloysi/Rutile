#ifndef RT_EXT_SWAPCHAIN_H
#define RT_EXT_SWAPCHAIN_H

/*
 * RT_EXT_SWAPCHAIN extension package.
 */

#include "rutile.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct rt_swapchain_t rt_swapchain_t;
typedef rt_swapchain_t* rt_swapchain;

typedef struct rt_swapchain_acquire_result {
	rt_framebuffer framebuffer;
	rt_timepoint timepoint;
} rt_swapchain_acquire_result;

typedef rt_swapchain (*PFN_rtSwapchainCreate)(void);
typedef void (*PFN_rtSwapchainDestroy)(rt_swapchain swapchain);
typedef void (*PFN_rtSwapchainResize)(rt_swapchain swapchain, u32 width, u32 height);
typedef rt_swapchain_acquire_result (*PFN_rtSwapchainAcquire)(rt_swapchain swapchain);
typedef void (*PFN_rtSwapchainPresent)(rt_swapchain swapchain, rt_timepoint rendered);
typedef void (*PFN_rtSwapchainDepthViewEnable)(rt_swapchain swapchain, bool enable);

extern PFN_rtSwapchainCreate rt_rtSwapchainCreate;
extern PFN_rtSwapchainDestroy rt_rtSwapchainDestroy;
extern PFN_rtSwapchainResize rt_rtSwapchainResize;
extern PFN_rtSwapchainAcquire rt_rtSwapchainAcquire;
extern PFN_rtSwapchainPresent rt_rtSwapchainPresent;
extern PFN_rtSwapchainDepthViewEnable rt_rtSwapchainDepthViewEnable;
bool rtLoad_RT_EXT_SWAPCHAIN(void);

#ifndef RT_NO_API_WRAPPERS
static inline rt_swapchain rtSwapchainCreate(void) { return rt_rtSwapchainCreate(); }
static inline void rtSwapchainDestroy(rt_swapchain swapchain) { rt_rtSwapchainDestroy(swapchain); }
static inline void rtSwapchainResize(rt_swapchain swapchain, u32 width, u32 height) { rt_rtSwapchainResize(swapchain, width, height); }
static inline rt_swapchain_acquire_result rtSwapchainAcquire(rt_swapchain swapchain) { return rt_rtSwapchainAcquire(swapchain); }
static inline void rtSwapchainPresent(rt_swapchain swapchain, rt_timepoint rendered) { rt_rtSwapchainPresent(swapchain, rendered); }
static inline void rtSwapchainDepthViewEnable(rt_swapchain swapchain, bool enable) { rt_rtSwapchainDepthViewEnable(swapchain, enable); }
#endif

#ifdef RUTILE_IMPL

PFN_rtSwapchainCreate rt_rtSwapchainCreate = NULL;
PFN_rtSwapchainDestroy rt_rtSwapchainDestroy = NULL;
PFN_rtSwapchainResize rt_rtSwapchainResize = NULL;
PFN_rtSwapchainAcquire rt_rtSwapchainAcquire = NULL;
PFN_rtSwapchainPresent rt_rtSwapchainPresent = NULL;
PFN_rtSwapchainDepthViewEnable rt_rtSwapchainDepthViewEnable = NULL;

#define RT__SWAPCHAIN_RESOLVE(name)                                                       \
	do {                                                                                  \
		rt_proc_t _p = rtGetProc(#name);                                                  \
		if (!_p) {                                                                        \
			return false;                                                                 \
		}                                                                                 \
		rt_##name = (PFN_##name)_p;                                                       \
	} while (0)

bool rtLoad_RT_EXT_SWAPCHAIN(void) {
	RT__SWAPCHAIN_RESOLVE(rtSwapchainCreate);
	RT__SWAPCHAIN_RESOLVE(rtSwapchainDestroy);
	RT__SWAPCHAIN_RESOLVE(rtSwapchainResize);
	RT__SWAPCHAIN_RESOLVE(rtSwapchainAcquire);
	RT__SWAPCHAIN_RESOLVE(rtSwapchainPresent);
	RT__SWAPCHAIN_RESOLVE(rtSwapchainDepthViewEnable);
	return true;
}

#undef RT__SWAPCHAIN_RESOLVE

#endif /* RUTILE_IMPL */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* RT_EXT_SWAPCHAIN_H */
