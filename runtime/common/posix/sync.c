#include "sync.h"

#include <pthread.h>
#include <stdlib.h>
#include <time.h>

struct rt_event {
	pthread_mutex_t mutex;
	pthread_cond_t condition;
	bool manual_reset;
	bool signaled;
};

struct rt_mutex {
	pthread_mutex_t mutex;
};

struct rt_condition {
	pthread_cond_t condition;
};

struct rt_thread {
	pthread_t thread;
};

struct rt_thread_start {
	rt_thread_proc proc;
	void* data;
};

static void* rt_thread_entry(void* data) {
	struct rt_thread_start* start = (struct rt_thread_start*)data;
	rt_thread_proc proc = start->proc;
	void* proc_data = start->data;
	free(start);
	return (void*)(uintptr_t)proc(proc_data);
}

static bool rt_event_try_consume(struct rt_event* event) {
	bool signaled;

	pthread_mutex_lock(&event->mutex);
	signaled = event->signaled;
	if (signaled && !event->manual_reset) {
		event->signaled = false;
	}
	pthread_mutex_unlock(&event->mutex);
	return signaled;
}

struct rt_event* rt_event_create(bool manual_reset, bool initial_state) {
	struct rt_event* event = calloc(1, sizeof(*event));
	if (!event) {
		return NULL;
	}
	event->manual_reset = manual_reset;
	event->signaled = initial_state;
	if (pthread_mutex_init(&event->mutex, NULL) != 0) {
		free(event);
		return NULL;
	}
	if (pthread_cond_init(&event->condition, NULL) != 0) {
		pthread_mutex_destroy(&event->mutex);
		free(event);
		return NULL;
	}
	return event;
}

void rt_event_destroy(struct rt_event* event) {
	if (!event) {
		return;
	}
	pthread_cond_destroy(&event->condition);
	pthread_mutex_destroy(&event->mutex);
	free(event);
}

void rt_event_signal(struct rt_event* event) {
	pthread_mutex_lock(&event->mutex);
	event->signaled = true;
	if (event->manual_reset) {
		pthread_cond_broadcast(&event->condition);
	} else {
		pthread_cond_signal(&event->condition);
	}
	pthread_mutex_unlock(&event->mutex);
}

void rt_event_reset(struct rt_event* event) {
	pthread_mutex_lock(&event->mutex);
	event->signaled = false;
	pthread_mutex_unlock(&event->mutex);
}

void rt_event_wait(struct rt_event* event) {
	pthread_mutex_lock(&event->mutex);
	while (!event->signaled) {
		pthread_cond_wait(&event->condition, &event->mutex);
	}
	if (!event->manual_reset) {
		event->signaled = false;
	}
	pthread_mutex_unlock(&event->mutex);
}

u32 rt_event_wait_any(struct rt_event** events, u32 count) {
	const struct timespec sleep_time = { 0, 1000000 };

	for (;;) {
		for (u32 i = 0; i < count; ++i) {
			if (rt_event_try_consume(events[i])) {
				return i;
			}
		}
		nanosleep(&sleep_time, NULL);
	}
}

void* rt_event_native_handle(struct rt_event* event) {
	(void)event;
	return NULL;
}

void rt_native_wait_handle_wait(void* handle) {
	(void)handle;
}

void rt_native_wait_handle_close(void* handle) {
	(void)handle;
}

struct rt_mutex* rt_mutex_create(void) {
	struct rt_mutex* mutex = calloc(1, sizeof(*mutex));
	if (!mutex) {
		return NULL;
	}
	if (pthread_mutex_init(&mutex->mutex, NULL) != 0) {
		free(mutex);
		return NULL;
	}
	return mutex;
}

void rt_mutex_destroy(struct rt_mutex* mutex) {
	if (!mutex) {
		return;
	}
	pthread_mutex_destroy(&mutex->mutex);
	free(mutex);
}

void rt_mutex_lock(struct rt_mutex* mutex) {
	pthread_mutex_lock(&mutex->mutex);
}

void rt_mutex_unlock(struct rt_mutex* mutex) {
	pthread_mutex_unlock(&mutex->mutex);
}

struct rt_condition* rt_condition_create(void) {
	struct rt_condition* condition = calloc(1, sizeof(*condition));
	if (!condition) {
		return NULL;
	}
	if (pthread_cond_init(&condition->condition, NULL) != 0) {
		free(condition);
		return NULL;
	}
	return condition;
}

void rt_condition_destroy(struct rt_condition* condition) {
	if (!condition) {
		return;
	}
	pthread_cond_destroy(&condition->condition);
	free(condition);
}

void rt_condition_wait(struct rt_condition* condition, struct rt_mutex* mutex) {
	pthread_cond_wait(&condition->condition, &mutex->mutex);
}

void rt_condition_broadcast(struct rt_condition* condition) {
	pthread_cond_broadcast(&condition->condition);
}

struct rt_thread* rt_thread_create(rt_thread_proc proc, void* data) {
	struct rt_thread* thread = calloc(1, sizeof(*thread));
	struct rt_thread_start* start = calloc(1, sizeof(*start));
	if (!thread || !start) {
		free(start);
		free(thread);
		return NULL;
	}
	start->proc = proc;
	start->data = data;
	if (pthread_create(&thread->thread, NULL, rt_thread_entry, start) != 0) {
		free(start);
		free(thread);
		return NULL;
	}
	return thread;
}

void rt_thread_join(struct rt_thread* thread) {
	if (!thread) {
		return;
	}
	pthread_join(thread->thread, NULL);
	free(thread);
}

unsigned rt_thread_id(struct rt_thread* thread) {
	return thread ? (unsigned)(uintptr_t)thread->thread : 0;
}

unsigned rt_current_thread_id(void) {
	return (unsigned)(uintptr_t)pthread_self();
}
