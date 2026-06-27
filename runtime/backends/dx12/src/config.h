#ifndef RTDX_CONFIG_H
#define RTDX_CONFIG_H

#ifdef __cplusplus
#define RTDX_EXTERN_C_ENTER extern "C" {
#define RTDX_EXTERN_C_EXIT }
#else
#define RTDX_EXTERN_C_ENTER
#define RTDX_EXTERN_C_EXIT
#endif

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTDX_EXTERN_C_ENTER

#if defined(_WIN32)
#define RTDX_API __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define RTDX_API __attribute__((visibility("default")))
#else
#define RTDX_API
#endif

#ifndef __cplusplus
#ifndef thread_local
#if defined(_WIN32)
#define thread_local __declspec(thread)
#else
#define thread_local _Thread_local
#endif
#endif
#endif

#define RTDX_MAX_FRAMES_IN_FLIGHT 3

RTDX_EXTERN_C_EXIT
#endif /* RTDX_CONFIG_H */


