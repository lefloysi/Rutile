#ifndef RTVAL_HANDLES_H
#define RTVAL_HANDLES_H

#include "procs.h"

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtval_buffer;
struct rtval_texture;
struct rtval_texture_view;
struct rtval_framebuffer;
struct rtval_graphics_program;
struct rtval_compute_program;
struct rtval_command_buffer;
struct rtval_queue;
struct rtval_swapchain;

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

#define RTVAL_DECLARE_NEW_RESOURCE(type, public_type)                                                                   \
	static inline struct rtval_##type* rtval_##type##_from_handle(public_type h) { return (struct rtval_##type*)h; }    \
	static inline public_type rtval_##type##_to_handle(struct rtval_##type* h) { return (public_type)h; }

RTVAL_DECLARE_NEW_RESOURCE(buffer,            rt_buffer)
RTVAL_DECLARE_NEW_RESOURCE(texture,           rt_texture)
RTVAL_DECLARE_NEW_RESOURCE(texture_view,      rt_texture_view)
RTVAL_DECLARE_NEW_RESOURCE(framebuffer,       rt_framebuffer)
RTVAL_DECLARE_NEW_RESOURCE(graphics_program,  rt_graphics_program)
RTVAL_DECLARE_NEW_RESOURCE(compute_program,   rt_compute_program)
RTVAL_DECLARE_NEW_RESOURCE(command_buffer,    rt_command_buffer)
RTVAL_DECLARE_NEW_RESOURCE(queue,             rt_queue)
RTVAL_DECLARE_NEW_RESOURCE(swapchain,         rt_swapchain)

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtval_queue* rtval_queue_wrap(rt_queue backend);
void                rtval_queue_release_all(void);

rt_timepoint rtval_timepoint_wrap(rt_timepoint backend_tp);
rt_timepoint rtval_timepoint_unwrap(rt_timepoint public_tp);

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

typedef enum rtval_handle_type {
	RTVAL_HANDLE_TYPE_BUFFER,
	RTVAL_HANDLE_TYPE_TEXTURE,
	RTVAL_HANDLE_TYPE_TEXTURE_VIEW,
	RTVAL_HANDLE_TYPE_FRAMEBUFFER,
	RTVAL_HANDLE_TYPE_GRAPHICS_PROGRAM,
	RTVAL_HANDLE_TYPE_COMPUTE_PROGRAM,
	RTVAL_HANDLE_TYPE_COMMAND_BUFFER,
	RTVAL_HANDLE_TYPE_QUEUE,
	RTVAL_HANDLE_TYPE_SWAPCHAIN,
	RTVAL_HANDLE_TYPE_COUNT,
} rtval_handle_type;

void* rtval_handle_create(rtval_handle_type type);
void* rtval_handle_payload(void* handle);
bool  rtval_handle_is_live(void* handle);
void  rtval_handle_destroy(void* handle);
void  rtval_handle_report_leaks(void);
void  rtval_handle_reset_registry(void);

#define RTVAL_PAYLOAD(handle, type) ((type*)rtval_handle_payload((handle)))

#endif
