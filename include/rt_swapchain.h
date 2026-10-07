#ifndef RT_SWAPCHAIN_H
#define RT_SWAPCHAIN_H

/*!
** @file rt_swapchain.h
** @brief Rutile presentation extension.
**
** Load Rutile, enable @ref RT_FEATURE_PRESENTATION with @ref rtInit, load this
** extension, create and bind a swapchain, then acquire and present one frame
** at a time.
*/

typedef struct rt_swapchain_t rt_swapchain_t;
typedef rt_swapchain_t* rt_swapchain;

#include "rutile.h"

/*! @brief Version of the extension contract declared by this header. */
#define RT_SWAPCHAIN_VERSION RT_HEADER_VERSION

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*!
** @brief Resources required to render one acquired frame.
**
** @p framebuffer remains valid until the matching @ref rtSwapchainPresent.
** Pass @p timepoint to @ref rtQueueWait before submitting commands that use
** the framebuffer. A NULL framebuffer means no frame was acquired.
*/
typedef struct rt_swapchain_acquire_result {
	rt_framebuffer framebuffer;
	rt_timepoint timepoint;
} rt_swapchain_acquire_result;

#if !defined(RT_TYPES_ONLY)

/*!
** @brief Make the swapchain procedures callable.
**
** Call after @ref rtLoad and before any swapchain procedure. Failure is
** reported through @ref rtError; swapchain procedures remain unavailable.
*/
void rtLoadSwapchain(void);

/*!
** @brief Create an unbound presentation swapchain.
**
** @ref RT_FEATURE_PRESENTATION must be enabled. Bind the returned swapchain to
** a window before calling @ref rtSwapchainAcquire.
**
** @return New swapchain, or NULL on failure.
*/
RT_API rt_swapchain rtSwapchainCreate(void);

/*!
** @brief Destroy a presentation swapchain.
**
** No frame may be acquired. Destroys the swapchain; framebuffers returned by
** it must not be used again.
**
** @param swapchain Swapchain to destroy.
*/
RT_API void rtSwapchainDestroy(rt_swapchain swapchain);

/*!
** @brief Set the presentation extent.
**
** @p width and @p height must be non-zero. No frame may be acquired. The next
** acquisitions may use the previous extent while a replacement is prepared.
** Render using the extent of the acquired framebuffer color view; its extent
** remains fixed until the matching presentation. Resize requests are coalesced.
**
** @param swapchain Swapchain whose images are recreated.
** @param width     Width in pixels.
** @param height    Height in pixels.
*/
RT_API void rtSwapchainResize(rt_swapchain swapchain, u32 width, u32 height);

/*!
** @brief Acquire the next framebuffer for rendering.
**
** Only one frame may be acquired from a swapchain at a time. Wait on the
** returned timepoint before submitting commands that use the framebuffer.
** Present the frame before acquiring another.
**
** @param swapchain Swapchain to acquire from.
** @return Framebuffer and readiness timepoint. If framebuffer is NULL, inspect
**         @ref rtError after clearing any earlier error to distinguish a
**         reported failure from no frame being available.
*/
RT_API rt_swapchain_acquire_result rtSwapchainAcquire(rt_swapchain swapchain);

/*!
** @brief Present the framebuffer acquired for the current frame.
**
** @p rendered must be the completion point for commands that used the
** acquired framebuffer. After this call, that framebuffer is invalid and a
** new frame may be acquired.
**
** @param swapchain Swapchain from which the current framebuffer was acquired.
** @param rendered  Completion point for the acquired frame's rendering.
*/
RT_API void rtSwapchainPresent(rt_swapchain swapchain, rt_timepoint rendered);

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

#endif /* !RT_TYPES_ONLY */

#define RT_SWAPCHAIN_PROCEDURES(X)                                                                          \
	X(rt_swapchain, rtSwapchainCreate, (void), ())                                                          \
	X(void, rtSwapchainDestroy, (rt_swapchain swapchain), (swapchain))                                      \
	X(void, rtSwapchainResize, (rt_swapchain swapchain, u32 width, u32 height), (swapchain, width, height)) \
	X(rt_swapchain_acquire_result, rtSwapchainAcquire, (rt_swapchain swapchain), (swapchain))               \
	X(void, rtSwapchainPresent, (rt_swapchain swapchain, rt_timepoint rendered), (swapchain, rendered))
/* RT_SWAPCHAIN_PROCEDURES */

#if !defined(RT_TYPES_ONLY)

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4098)
#endif
RT_SWAPCHAIN_PROCEDURES(RT_DECLARE_EXTENSION_PROCEDURE)
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif /* !RT_TYPES_ONLY */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* RT_SWAPCHAIN_H */
