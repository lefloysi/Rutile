#ifndef RT_SYNC_H
#define RT_SYNC_H

#include <stdbool.h>
#include "rutile.h"

#ifdef __cplusplus
extern "C" {
#endif

struct rt_condition;
struct rt_event;
struct rt_mutex;
struct rt_thread;

typedef unsigned (*rt_thread_proc)(void* data);

struct rt_event* rt_event_create(bool manual_reset, bool initial_state);
void rt_event_destroy(struct rt_event* event);
void rt_event_signal(struct rt_event* event);
void rt_event_reset(struct rt_event* event);
void rt_event_wait(struct rt_event* event);
u32 rt_event_wait_any(struct rt_event** events, u32 count);
void* rt_event_native_handle(struct rt_event* event);
void rt_native_wait_handle_wait(void* handle);
void rt_native_wait_handle_close(void* handle);

struct rt_mutex* rt_mutex_create(void);
void rt_mutex_destroy(struct rt_mutex* mutex);
void rt_mutex_lock(struct rt_mutex* mutex);
void rt_mutex_unlock(struct rt_mutex* mutex);

struct rt_condition* rt_condition_create(void);
void rt_condition_destroy(struct rt_condition* condition);
void rt_condition_wait(struct rt_condition* condition, struct rt_mutex* mutex);
void rt_condition_broadcast(struct rt_condition* condition);

struct rt_thread* rt_thread_create(rt_thread_proc proc, void* data);
void rt_thread_join(struct rt_thread* thread);
unsigned rt_thread_id(struct rt_thread* thread);
unsigned rt_current_thread_id(void);

#ifdef __cplusplus
}
#endif
#endif /* RT_SYNC_H */
