#include "buffer.h"

#include "system/atomic.h"
#include "context.h"
#include "error.h"
#include "execution/execution.h"
#include "resource/queue.h"

#include <stdlib.h>
#include <string.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/
rt_buffer rtBufferCreate(void) {
	rtgl_begin_errorable_operation();
	return rtgl_buffer_to_handle(rtgl_buffer_create(rtgl_get_current_context()));
}

void rtBufferDestroy(rt_buffer buffer) {
	rtgl_buffer_destroy(rtgl_get_current_context(), rtgl_buffer_from_handle(buffer));
}

void rtBufferResize(rt_buffer buffer, enum rt_memory_type memory_type, usize size) {
	rtgl_begin_errorable_operation();
	rtgl_buffer_resize(rtgl_get_current_context(), rtgl_buffer_from_handle(buffer), memory_type, size);
}

void rtBufferRead(rt_buffer buffer, rt_buffer_range range, u08* data, usize data_size) {
	rtgl_begin_errorable_operation();
	if (data_size < range.size) {
		rtgl_throwf(RT_IMPROPER_USAGE, "buffer read destination is too small");
		return;
	}
	rtgl_buffer_read(rtgl_get_current_context(), rtgl_buffer_from_handle(buffer), range, data);
}

u08* rtBufferMap(rt_buffer buffer, rt_buffer_range range) {
	rtgl_begin_errorable_operation();
	return rtgl_buffer_map(rtgl_get_current_context(), rtgl_buffer_from_handle(buffer), range);
}

void rtBufferUnmap(rt_buffer buffer) {
	rtgl_begin_errorable_operation();
	rtgl_buffer_unmap(rtgl_get_current_context(), rtgl_buffer_from_handle(buffer));
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTGL_DEFINE_RESOURCE_PRIVATE(buffer)

void rtgl_buffer_init(struct rtgl_context* ctx, struct rtgl_buffer* buffer) {
	rtgl_init_resource_base(ctx, RTGL_RESOURCE_BASE(buffer), RTGL_RESOURCE_BUFFER);
	buffer->memory_type = RT_DEVICE_MEMORY;
}

void rtgl_buffer_finish(struct rtgl_buffer* buffer) {
	rtgl_buffer_storage_release(buffer->storage);
	buffer->storage = NULL;
	while (buffer->reusable_storage) {
		struct rtgl_buffer_storage* storage = buffer->reusable_storage;
		buffer->reusable_storage = storage->next;
		storage->next = NULL;
		rtgl_buffer_storage_release(storage);
	}
	rtgl_finish_resource_base(RTGL_RESOURCE_BASE(buffer));
}

static struct rtgl_buffer_storage* rtgl_buffer_storage_create(struct rtgl_context* ctx, enum rt_memory_type memory_type, usize size) {
	struct rtgl_buffer_storage* storage = RTGL_ALLOC_RESOURCE(struct rtgl_buffer_storage);
	if (!storage) {
		return NULL;
	}
	storage->ctx = ctx;
	storage->memory_type = memory_type;
	storage->size = size;
	storage->ref_count = 1;
	storage->last_write = (rt_timepoint){ 0 };
	if (size) {
		storage->shadow_data = (u08*)calloc(size, 1);
		RTGL_CHECK_ALLOC(storage->shadow_data, size, "OpenGL buffer shadow data");
		if (!storage->shadow_data) {
			free(storage);
			return NULL;
		}
	}
	rtgl_execution_buffer_create(ctx, storage);
	rtgl_execution_buffer_data(ctx, storage, size, storage->shadow_data);
	return storage;
}

void rtgl_buffer_storage_retain(struct rtgl_buffer_storage* storage) {
	if (storage) {
		rt_atomic_inc(&storage->ref_count);
	}
}

void rtgl_buffer_storage_release(struct rtgl_buffer_storage* storage) {
	if (!storage || rt_atomic_dec(&storage->ref_count)) {
		return;
	}
	rtgl_execution_buffer_delete(storage->ctx, storage);
	free(storage->shadow_data);
	free(storage);
}

static void rtgl_buffer_recycle_storage(struct rtgl_buffer* buffer, struct rtgl_buffer_storage* storage) {
	if (!storage) {
		return;
	}
	storage->next = buffer->reusable_storage;
	buffer->reusable_storage = storage;
}

static struct rtgl_buffer_storage* rtgl_buffer_take_reusable_storage(struct rtgl_buffer* buffer, enum rt_memory_type memory_type, usize size) {
	struct rtgl_buffer_storage** link = &buffer->reusable_storage;
	while (*link) {
		struct rtgl_buffer_storage* storage = *link;
		if (storage->memory_type == memory_type && storage->size == size && rt_atomic_load(&storage->ref_count) == 1) {
			*link = storage->next;
			storage->next = NULL;
			return storage;
		}
		link = &storage->next;
	}
	return NULL;
}

static struct rtgl_buffer_storage* rtgl_buffer_copy_storage(struct rtgl_context* ctx, struct rtgl_buffer* buffer) {
	struct rtgl_buffer_storage* source = buffer->storage;
	rtgl_buffer_storage_wait(ctx, source);
	struct rtgl_buffer_storage* target = rtgl_buffer_take_reusable_storage(buffer, source->memory_type, source->size);
	if (!target) {
		target = rtgl_buffer_storage_create(ctx, source->memory_type, source->size);
	}
	if (!target) {
		return NULL;
	}
	if (source->size) {
		memcpy(target->shadow_data, source->shadow_data, source->size);
		rtgl_execution_buffer_copy(ctx, source, target);
	}
	rtgl_buffer_recycle_storage(buffer, source);
	buffer->storage = target;
	return target;
}

void rtgl_buffer_resize(struct rtgl_context* ctx, struct rtgl_buffer* buffer, enum rt_memory_type memory_type, usize size) {
	if (!buffer) {
		return;
	}
	buffer->memory_type = memory_type;
	rtgl_buffer_recycle_storage(buffer, buffer->storage);
	buffer->storage = rtgl_buffer_take_reusable_storage(buffer, memory_type, size);
	if (!buffer->storage) {
		buffer->storage = rtgl_buffer_storage_create(ctx, memory_type, size);
	}
}

struct rtgl_buffer_storage* rtgl_buffer_prepare_write(struct rtgl_context* ctx, struct rtgl_buffer* buffer) {
	if (!buffer || !buffer->storage) {
		return NULL;
	}
	if (rt_atomic_load(&buffer->storage->ref_count) > 1 && !rtgl_buffer_copy_storage(ctx, buffer)) {
		return NULL;
	}
	return buffer->storage;
}

void rtgl_buffer_subdata(struct rtgl_context* ctx, struct rtgl_buffer* buffer, rt_buffer_range range, const u08* data) {
	if (!buffer || !buffer->storage || !range.size) {
		return;
	}
	if (range.offset > buffer->storage->size || range.size > buffer->storage->size - range.offset) {
		rtgl_throwf(RT_IMPROPER_USAGE, "buffer upload range is out of bounds");
		return;
	}
	if (!rtgl_buffer_prepare_write(ctx, buffer)) {
		return;
	}
	memcpy(buffer->storage->shadow_data + range.offset, data, range.size);
	rtgl_execution_buffer_subdata(ctx, buffer->storage, range.offset, range.size, data);
}

void rtgl_buffer_read(struct rtgl_context* ctx, struct rtgl_buffer* buffer, rt_buffer_range range, u08* data) {
	if (!buffer || !buffer->storage || !range.size) {
		return;
	}
	if (range.offset > buffer->storage->size || range.size > buffer->storage->size - range.offset) {
		rtgl_throwf(RT_IMPROPER_USAGE, "buffer read range is out of bounds");
		return;
	}
	rtgl_buffer_storage_wait(ctx, buffer->storage);
	memcpy(data, buffer->storage->shadow_data + range.offset, range.size);
}

u08* rtgl_buffer_map(struct rtgl_context* ctx, struct rtgl_buffer* buffer, rt_buffer_range range) {
	if (!buffer || !buffer->storage || buffer->memory_type != RT_HOST_MEMORY || buffer->mapped || range.offset > buffer->storage->size || range.size > buffer->storage->size - range.offset) {
		return NULL;
	}
	if (!rtgl_buffer_prepare_write(ctx, buffer)) {
		return NULL;
	}
	rtgl_buffer_storage_wait(ctx, buffer->storage);
	buffer->mapped_range = range;
	buffer->mapped = true;
	return buffer->storage->shadow_data + range.offset;
}

void rtgl_buffer_unmap(struct rtgl_context* ctx, struct rtgl_buffer* buffer) {
	if (!buffer || !buffer->mapped || !buffer->storage) {
		return;
	}
	rtgl_execution_buffer_subdata(ctx, buffer->storage, buffer->mapped_range.offset, buffer->mapped_range.size, buffer->storage->shadow_data + buffer->mapped_range.offset);
	buffer->mapped = false;
}

void rtgl_buffer_storage_wait(struct rtgl_context* ctx, struct rtgl_buffer_storage* storage) {
	rt_timepoint last_write;
	if (!ctx || !storage) {
		return;
	}
	rtgl_execution_lock(ctx);
	last_write = storage->last_write;
	rtgl_execution_unlock(ctx);
	rtgl_timepoint_wait(ctx, last_write);
}

void rtgl_buffer_storage_mark_write(struct rtgl_context* ctx, struct rtgl_buffer_storage* storage, rt_timepoint timepoint) {
	if (!ctx || !storage) {
		return;
	}
	rtgl_execution_lock(ctx);
	storage->last_write = timepoint;
	rtgl_execution_unlock(ctx);
}
