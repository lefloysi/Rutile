#ifndef RTVK_CONFIG_H
#define RTVK_CONFIG_H

#ifdef __cplusplus
#define RTVK_EXTERN_C_ENTER extern "C" {
#define RTVK_EXTERN_C_EXIT }
#else
#define RTVK_EXTERN_C_ENTER
#define RTVK_EXTERN_C_EXIT
#endif /* __cplusplus */
RTVK_EXTERN_C_ENTER

#if defined(_WIN32)
#define RTVK_API __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define RTVK_API __attribute__((visibility("default")))
#else
#define RTVK_API
#endif

#ifndef __cplusplus
#ifndef thread_local
#if defined(_WIN32)
#define thread_local __declspec(thread)
#else
#define thread_local _Thread_local
#endif /* _WIN32 */
#endif /* thread_local */
#endif /* __cplusplus */

#define VK_ALLOCATOR NULL
#define RTVK_MAX_FRAMES_IN_FLIGHT 3

RTVK_EXTERN_C_EXIT
#endif /* RTVK_CONFIG_H */


