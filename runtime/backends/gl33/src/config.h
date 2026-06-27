#ifndef RTGL33_CONFIG_H
#define RTGL33_CONFIG_H

#ifdef __cplusplus
#define RTGL_EXTERN_C_ENTER extern "C" {
#define RTGL_EXTERN_C_EXIT }
#else
#define RTGL_EXTERN_C_ENTER
#define RTGL_EXTERN_C_EXIT
#endif /* __cplusplus */

#if defined(_WIN32)
#define RTGL_API __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define RTGL_API __attribute__((visibility("default")))
#else
#define RTGL_API
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

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

#define RTGL_CHECK_ALLOC(ptr, bytes, what)             \
	do {                                               \
		if (!(ptr)) {                                  \
			rtgl_throwf(                               \
				RT_OUT_OF_HOST_MEMORY,                 \
				"failed to allocate %zu bytes for %s", \
				(usize)(bytes),                        \
				(what)                                 \
			);                                         \
		}                                              \
	} while (0)

#define RTGL_ALLOC(var, type, count, what)     \
	type* var = calloc((count), sizeof(type)); \
	RTGL_CHECK_ALLOC(var, sizeof(type) * (usize)(count), (what))
#endif /* RTGL33_CONFIG_H */
