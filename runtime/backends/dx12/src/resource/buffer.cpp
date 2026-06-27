#include "buffer.h"
#include "context.h"
#include "error.h"
#include "resource/queue.h"

#include <stdlib.h>
#include <string.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

static enum rt_buffer_usage rtdx_default_buffer_usage(void) {
	return (enum rt_buffer_usage)(
		RT_BUFFER_USAGE_VERTEX |
		RT_BUFFER_USAGE_INDEX |
		RT_BUFFER_USAGE_UNIFORM |
		RT_BUFFER_USAGE_STORAGE |
		RT_BUFFER_USAGE_TRANSFER_SRC |
		RT_BUFFER_USAGE_TRANSFER_DST
	);
}

static bool rtdx_buffer_is_staging(enum rt_buffer_usage usage) {
	return (usage & RT_BUFFER_USAGE_STAGING) != 0;
}

static bool rtdx_buffer_uses_host_storage(enum rt_buffer_mode mode, enum rt_buffer_usage usage) {
	return mode == RT_BUFFER_DYNAMIC || rtdx_buffer_is_staging(usage);
}

static struct rtdx_queue *rtdx_buffer_upload_queue(struct rtdx_context *ctx) {
	struct rtdx_queue *queue = rtdx_queue_query(ctx, RT_QUEUE_TRANSFER);
	if (queue) {
		return queue;
	}
	return rtdx_queue_query(ctx, RT_QUEUE_GRAPHICS);
}

static D3D12_RESOURCE_STATES rtdx_buffer_gpu_state(enum rt_buffer_usage usage) {
	D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
	bool has_state = false;

	if (usage & RT_BUFFER_USAGE_VERTEX) {
		state = (D3D12_RESOURCE_STATES)(state | D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		has_state = true;
	}
	if (usage & RT_BUFFER_USAGE_UNIFORM) {
		state = (D3D12_RESOURCE_STATES)(state | D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		has_state = true;
	}
	if (usage & RT_BUFFER_USAGE_INDEX) {
		state = (D3D12_RESOURCE_STATES)(state | D3D12_RESOURCE_STATE_INDEX_BUFFER);
		has_state = true;
	}
	if (usage & RT_BUFFER_USAGE_STORAGE) {
		state = (D3D12_RESOURCE_STATES)(state | D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		has_state = true;
	}
	if (!has_state && (usage & RT_BUFFER_USAGE_TRANSFER_SRC)) {
		state = D3D12_RESOURCE_STATE_COPY_SOURCE;
		has_state = true;
	}
	if (!has_state && (usage & RT_BUFFER_USAGE_TRANSFER_DST)) {
		state = D3D12_RESOURCE_STATE_COPY_DEST;
		has_state = true;
	}
	if (!has_state) {
		state = D3D12_RESOURCE_STATE_COMMON;
	}
	return state;
}

rt_buffer rtBufferCreate(void) {
	struct rtdx_buffer *buffer = rtdx_buffer_create(rtdx_get_current_context());
	return rtdx_buffer_to_handle(buffer);
}

void rtBufferDestroy(rt_buffer buffer) {
	rtdx_buffer_destroy(rtdx_get_current_context(), rtdx_buffer_from_handle(buffer));
}

rt_timepoint rtBufferData(rt_buffer buffer, enum rt_buffer_mode mode, enum rt_buffer_usage usage, u64 size, const void *data) {
	struct rtdx_timepoint timepoint = rtdx_buffer_data(
		rtdx_get_current_context(),
		rtdx_buffer_from_handle(buffer),
		mode,
		usage,
		size,
		data
	);
	return rtdx_timepoint_to_public(timepoint);
}

rt_timepoint rtBufferSubdata(rt_buffer buffer, u64 offset, u64 size, const void *data) {
	struct rtdx_timepoint timepoint = rtdx_buffer_subdata(
		rtdx_get_current_context(),
		rtdx_buffer_from_handle(buffer),
		offset,
		size,
		data
	);
	return rtdx_timepoint_to_public(timepoint);
}

void rtBufferRead(rt_buffer buffer, u64 offset, u64 size, void *data) {
	rtdx_buffer_read(
		rtdx_get_current_context(),
		rtdx_buffer_from_handle(buffer),
		offset,
		size,
		data
	);
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTDX_DEFINE_RESOURCE_PRIVATE(buffer)

void rtdx_buffer_init(struct rtdx_context *ctx, struct rtdx_buffer *buffer) {
	rtdx_init_resource_base(ctx, RTDX_RESOURCE_BASE(buffer), RT_RESOURCE_BUFFER);
	buffer->mode = RT_BUFFER_DYNAMIC;
	buffer->usage = rtdx_default_buffer_usage();
}

static struct rtdx_buffer_storage *rtdx_buffer_storage_create(
	struct rtdx_context *ctx,
	u64 size,
	enum rt_buffer_mode mode,
	enum rt_buffer_usage usage
) {
	struct rtdx_buffer_storage *storage = (struct rtdx_buffer_storage *)calloc(1, sizeof(*storage));
	if (!storage) {
		rtdx_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate buffer storage metadata");
		return NULL;
	}

	storage->ctx = ctx;
	storage->ref_count = 1;
	storage->size = size;
	storage->mode = mode;
	storage->usage = usage;

	D3D12_HEAP_PROPERTIES heap = {};
	heap.Type = rtdx_buffer_uses_host_storage(mode, usage) ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT;
	heap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heap.CreationNodeMask = 1;
	heap.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Alignment = 0;
	desc.Width = size;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.Flags = D3D12_RESOURCE_FLAG_NONE;

	storage->state = rtdx_buffer_uses_host_storage(mode, usage) ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COPY_DEST;
	HRESULT result = ctx->d3d_device->CreateCommittedResource(
		&heap,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		storage->state,
		NULL,
		IID_PPV_ARGS(&storage->d3d_resource)
	);
	if (FAILED(result)) {
		free(storage);
		rtdx_throwf(rtdx_error_from_hresult(result), "CreateCommittedResource(buffer) failed: 0x%08x", (u32)result);
		return NULL;
	}

	if (!rtdx_buffer_uses_host_storage(mode, usage) && size) {
		storage->shadow_data = calloc(1, (usize)size);
		if (!storage->shadow_data) {
			rtdx_release(&storage->d3d_resource);
			free(storage);
			rtdx_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate static buffer shadow copy");
			return NULL;
		}
	}

	storage->vertex_view.BufferLocation = storage->d3d_resource->GetGPUVirtualAddress();
	storage->vertex_view.SizeInBytes = (UINT)size;
	storage->vertex_view.StrideInBytes = 0;
	return storage;
}

void rtdx_buffer_storage_retain(struct rtdx_buffer_storage *storage) {
	if (!storage) {
		return;
	}
	storage->ref_count++;
}

void rtdx_buffer_storage_release(struct rtdx_buffer_storage *storage) {
	if (!storage) {
		return;
	}
	if (--storage->ref_count != 0) {
		return;
	}
	free(storage->shadow_data);
	rtdx_release(&storage->d3d_resource);
	free(storage);
}

static bool rtdx_buffer_storage_write_host(struct rtdx_buffer_storage *storage, u64 offset, u64 size, const void *data) {
	if (!storage || !data || !size) {
		return true;
	}

	D3D12_RANGE read_range = {0, 0};
	void *mapped_data = NULL;
	HRESULT result = storage->d3d_resource->Map(0, &read_range, &mapped_data);
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "ID3D12Resource::Map failed: 0x%08x", (u32)result);
		return false;
	}
	memcpy((char *)mapped_data + offset, data, (usize)size);
	storage->d3d_resource->Unmap(0, NULL);
	return true;
}

static bool rtdx_buffer_storage_write_shadow(struct rtdx_buffer_storage *storage, u64 offset, u64 size, const void *data) {
	if (!storage || !size) {
		return true;
	}
	if (!storage->shadow_data) {
		rtdx_throwf(RT_PLATFORM_FAILURE, "static buffer shadow storage is missing");
		return false;
	}
	if (data) {
		memcpy((char *)storage->shadow_data + offset, data, (usize)size);
	}
	return true;
}

static void rtdx_buffer_storage_copy(struct rtdx_buffer_storage *dst, struct rtdx_buffer_storage *src) {
	if (!dst || !src) {
		return;
	}

	u64 copy_size = dst->size < src->size ? dst->size : src->size;
	if (!copy_size) {
		return;
	}

	if (dst->shadow_data && src->shadow_data) {
		memcpy(dst->shadow_data, src->shadow_data, (usize)copy_size);
		return;
	}

	D3D12_RANGE read_range = {0, (SIZE_T)copy_size};
	void *src_data = NULL;
	if (FAILED(src->d3d_resource->Map(0, &read_range, &src_data))) {
		return;
	}

	void *dst_data = NULL;
	if (FAILED(dst->d3d_resource->Map(0, NULL, &dst_data))) {
		src->d3d_resource->Unmap(0, NULL);
		return;
	}
	memcpy(dst_data, src_data, (usize)copy_size);
	dst->d3d_resource->Unmap(0, NULL);
	src->d3d_resource->Unmap(0, NULL);
}

static bool rtdx_buffer_upload_command(struct rtdx_context *ctx, struct rtdx_queue *queue) {
	if (queue->upload_allocator && queue->upload_command_list) {
		rtdx_timepoint_wait(ctx, {queue, queue->upload_fence_value});
		queue->upload_fence_value = 0;
		HRESULT result = queue->upload_allocator->Reset();
		if (FAILED(result)) {
			rtdx_throwf(rtdx_error_from_hresult(result), "ID3D12CommandAllocator::Reset failed: 0x%08x", (u32)result);
			return false;
		}
		result = queue->upload_command_list->Reset(queue->upload_allocator, NULL);
		if (FAILED(result)) {
			rtdx_throwf(rtdx_error_from_hresult(result), "ID3D12GraphicsCommandList::Reset failed: 0x%08x", (u32)result);
			return false;
		}
		return true;
	}

	HRESULT result = ctx->d3d_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&queue->upload_allocator));
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "CreateCommandAllocator failed: 0x%08x", (u32)result);
		return false;
	}

	result = ctx->d3d_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, queue->upload_allocator, NULL, IID_PPV_ARGS(&queue->upload_command_list));
	if (FAILED(result)) {
		rtdx_release(&queue->upload_allocator);
		rtdx_throwf(rtdx_error_from_hresult(result), "CreateCommandList failed: 0x%08x", (u32)result);
		return false;
	}
	return true;
}

static bool rtdx_buffer_upload_staging(struct rtdx_context *ctx, struct rtdx_queue *queue, u64 size) {
	if (queue->upload_buffer && queue->upload_buffer_size >= size) {
		rtdx_timepoint_wait(ctx, {queue, queue->upload_fence_value});
		queue->upload_fence_value = 0;
		return true;
	}

	rtdx_timepoint_wait(ctx, {queue, queue->upload_fence_value});
	queue->upload_fence_value = 0;
	rtdx_release(&queue->upload_buffer);
	queue->upload_buffer_size = 0;

	D3D12_HEAP_PROPERTIES heap = {};
	heap.Type = D3D12_HEAP_TYPE_UPLOAD;
	heap.CreationNodeMask = 1;
	heap.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Width = size;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	HRESULT result = ctx->d3d_device->CreateCommittedResource(
		&heap,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		NULL,
		IID_PPV_ARGS(&queue->upload_buffer)
	);
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "CreateCommittedResource(upload) failed: 0x%08x", (u32)result);
		return false;
	}
	queue->upload_buffer_size = size;
	return true;
}

static struct rtdx_timepoint rtdx_buffer_upload_static(
	struct rtdx_context *ctx,
	struct rtdx_queue *queue,
	struct rtdx_buffer_storage *storage,
	u64 offset,
	u64 size,
	const void *data
) {
	struct rtdx_timepoint timepoint = {queue, 0};
	if (!size) {
		return timepoint;
	}
	if (!queue) {
		rtdx_throwf(RT_IMPROPER_USAGE, "static buffer uploads require a valid queue");
		return timepoint;
	}

	rtdx_queue_collect(ctx, queue);

	if (!rtdx_buffer_upload_staging(ctx, queue, size)) {
		return timepoint;
	}

	void *mapped_data = NULL;
	HRESULT result = queue->upload_buffer->Map(0, NULL, &mapped_data);
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "ID3D12Resource::Map failed: 0x%08x", (u32)result);
		return timepoint;
	}
	memcpy(mapped_data, data, (usize)size);
	queue->upload_buffer->Unmap(0, NULL);

	if (!rtdx_buffer_upload_command(ctx, queue)) {
		return timepoint;
	}

	ID3D12GraphicsCommandList *command_list = queue->upload_command_list;

	if (storage->state != D3D12_RESOURCE_STATE_COPY_DEST) {
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = storage->d3d_resource;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = storage->state;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
		command_list->ResourceBarrier(1, &barrier);
	}

	command_list->CopyBufferRegion(storage->d3d_resource, offset, queue->upload_buffer, 0, size);

	D3D12_RESOURCE_STATES next_state = rtdx_buffer_gpu_state(storage->usage);
	if (next_state != D3D12_RESOURCE_STATE_COPY_DEST) {
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = storage->d3d_resource;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter = next_state;
		command_list->ResourceBarrier(1, &barrier);
	}

	result = command_list->Close();
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "ID3D12GraphicsCommandList::Close failed: 0x%08x", (u32)result);
		return timepoint;
	}

	ID3D12CommandList *lists[] = {command_list};
	queue->d3d_queue->ExecuteCommandLists(1, lists);

	u64 signal_value = queue->fence_value + 1;
	result = queue->d3d_queue->Signal(queue->d3d_fence, signal_value);
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "ID3D12CommandQueue::Signal failed: 0x%08x", (u32)result);
		return timepoint;
	}

	queue->fence_value = signal_value;
	timepoint.value = signal_value;
	storage->state = next_state;
	queue->upload_fence_value = signal_value;
	return timepoint;
}

static void rtdx_buffer_recycle_storage(struct rtdx_buffer *buffer, struct rtdx_buffer_storage *storage) {
	if (!storage) {
		return;
	}
	storage->next = buffer->reusable_storage;
	buffer->reusable_storage = storage;
}

static struct rtdx_buffer_storage *rtdx_buffer_take_reusable_storage(
	struct rtdx_buffer *buffer,
	u64 size,
	enum rt_buffer_mode mode,
	enum rt_buffer_usage usage
) {
	struct rtdx_buffer_storage **link = &buffer->reusable_storage;

	while (*link) {
		struct rtdx_buffer_storage *storage = *link;
		if (storage->size == size &&
			storage->mode == mode &&
			storage->usage == usage &&
			storage->ref_count == 1) {
			*link = storage->next;
			storage->next = NULL;
			return storage;
		}
		link = &storage->next;
	}
	return NULL;
}

void rtdx_buffer_finish(struct rtdx_context *ctx, struct rtdx_buffer *buffer) {
	rtdx_buffer_storage_release(buffer->storage);
	buffer->storage = NULL;

	struct rtdx_buffer_storage *storage = buffer->reusable_storage;
	while (storage) {
		struct rtdx_buffer_storage *next = storage->next;
		storage->next = NULL;
		rtdx_buffer_storage_release(storage);
		storage = next;
	}
	buffer->reusable_storage = NULL;
	rtdx_finish_resource_base(ctx, RTDX_RESOURCE_BASE(buffer));
}

struct rtdx_timepoint rtdx_buffer_data(struct rtdx_context *ctx, struct rtdx_buffer *buffer, enum rt_buffer_mode mode, enum rt_buffer_usage usage, u64 size, const void *data) {
	struct rtdx_timepoint timepoint = {NULL, 0};
	if (!buffer) {
		rtdx_throwf(RT_IMPROPER_USAGE, "buffer is NULL");
		return timepoint;
	}
	if (mode != RT_BUFFER_STATIC && mode != RT_BUFFER_DYNAMIC) {
		rtdx_throwf(RT_IMPROPER_USAGE, "unsupported buffer mode");
		return timepoint;
	}
	if (usage == RT_BUFFER_USAGE_NONE) {
		rtdx_throwf(RT_IMPROPER_USAGE, "buffer usage must not be empty");
		return timepoint;
	}

	struct rtdx_queue *queue = NULL;
	if (!rtdx_buffer_uses_host_storage(mode, usage) && size) {
		queue = rtdx_buffer_upload_queue(ctx);
		if (!queue) {
			rtdx_throwf(RT_IMPROPER_USAGE, "no queue is available for static buffer uploads");
			return timepoint;
		}
	}

	rtdx_queue_collect(ctx, queue);
	buffer->mode = mode;
	buffer->usage = usage;
	rtdx_buffer_recycle_storage(buffer, buffer->storage);
	buffer->storage = NULL;

	struct rtdx_buffer_storage *storage = rtdx_buffer_take_reusable_storage(buffer, size, buffer->mode, buffer->usage);
	if (!storage) {
		storage = rtdx_buffer_storage_create(ctx, size, buffer->mode, buffer->usage);
	}
	if (!storage) {
		return timepoint;
	}

	buffer->storage = storage;

	if (rtdx_buffer_uses_host_storage(storage->mode, storage->usage)) {
		if (data && size && !rtdx_buffer_storage_write_host(storage, 0, size, data)) {
			return timepoint;
		}
		return timepoint;
	}

	if (size && !rtdx_buffer_storage_write_shadow(storage, 0, size, data)) {
		return timepoint;
	}
	if (size) {
		const void *upload_data = storage->shadow_data;
		timepoint = rtdx_buffer_upload_static(ctx, queue, storage, 0, size, upload_data);
	}
	return timepoint;
}

struct rtdx_timepoint rtdx_buffer_subdata(struct rtdx_context *ctx, struct rtdx_buffer *buffer, u64 offset, u64 size, const void *data) {
	struct rtdx_timepoint timepoint = {NULL, 0};
	if (!buffer || !buffer->storage) {
		rtdx_throwf(RT_IMPROPER_USAGE, "buffer has no storage");
		return timepoint;
	}
	if (offset > buffer->storage->size || size > buffer->storage->size - offset) {
		rtdx_throwf(RT_IMPROPER_USAGE, "buffer upload range is out of bounds");
		return timepoint;
	}
	struct rtdx_queue *queue = NULL;
	if (!rtdx_buffer_uses_host_storage(buffer->storage->mode, buffer->storage->usage) && size && data) {
		queue = rtdx_buffer_upload_queue(ctx);
		if (!queue) {
			rtdx_throwf(RT_IMPROPER_USAGE, "no queue is available for static buffer uploads");
			return timepoint;
		}
	}
	rtdx_queue_collect(ctx, queue);
	if (!size || !data) {
		return timepoint;
	}

	bool replaced_storage = false;
	if (buffer->storage->ref_count > 1) {
		struct rtdx_buffer_storage *old_storage = buffer->storage;

		rtdx_buffer_recycle_storage(buffer, old_storage);
		buffer->storage = NULL;

		struct rtdx_buffer_storage *new_storage = rtdx_buffer_take_reusable_storage(buffer, old_storage->size, old_storage->mode, old_storage->usage);
		if (!new_storage) {
			new_storage = rtdx_buffer_storage_create(ctx, old_storage->size, old_storage->mode, old_storage->usage);
		}
		if (!new_storage) {
			return timepoint;
		}
		rtdx_buffer_storage_copy(new_storage, old_storage);
		buffer->storage = new_storage;
		replaced_storage = true;
	}

	if (rtdx_buffer_uses_host_storage(buffer->storage->mode, buffer->storage->usage)) {
		if (!rtdx_buffer_storage_write_host(buffer->storage, offset, size, data)) {
			return timepoint;
		}
		return timepoint;
	}

	if (!rtdx_buffer_storage_write_shadow(buffer->storage, offset, size, data)) {
		return timepoint;
	}
	if (replaced_storage) {
		return rtdx_buffer_upload_static(ctx, queue, buffer->storage, 0, buffer->storage->size, buffer->storage->shadow_data);
	}
	return rtdx_buffer_upload_static(ctx, queue, buffer->storage, offset, size, (const char *)buffer->storage->shadow_data + offset);
}

void rtdx_buffer_read(struct rtdx_context *ctx, struct rtdx_buffer *buffer, u64 offset, u64 size, void *data) {
	if (!buffer || !buffer->storage) {
		rtdx_throwf(RT_IMPROPER_USAGE, "buffer has no storage");
		return;
	}
	if (!data && size) {
		rtdx_throwf(RT_IMPROPER_USAGE, "buffer read destination is NULL");
		return;
	}
	if (offset > buffer->storage->size || size > buffer->storage->size - offset) {
		rtdx_throwf(RT_IMPROPER_USAGE, "buffer read range is out of bounds");
		return;
	}

	if (buffer->storage->shadow_data) {
		memcpy(data, (const char *)buffer->storage->shadow_data + offset, (usize)size);
		return;
	}

	D3D12_RANGE read_range = {(SIZE_T)offset, (SIZE_T)(offset + size)};
	void *mapped_data = NULL;
	HRESULT result = buffer->storage->d3d_resource->Map(0, &read_range, &mapped_data);
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "ID3D12Resource::Map failed: 0x%08x", (u32)result);
		return;
	}
	memcpy(data, (char *)mapped_data + offset, (usize)size);
	D3D12_RANGE write_range = {0, 0};
	buffer->storage->d3d_resource->Unmap(0, &write_range);
}
