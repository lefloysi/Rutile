#include "resource/swapchain.hpp"
#include "context.hpp"
#include "error.hpp"

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

rt_swapchain rtSwapchainCreate(void) {
	struct rtdx_swapchain* swapchain = rtdx_swapchain_create(rtdx_get_current_context());
	return rtdx_swapchain_to_handle(swapchain);
}

void rtSwapchainDestroy(rt_swapchain swapchain) {
	rtdx_swapchain_destroy(rtdx_get_current_context(), rtdx_swapchain_from_handle(swapchain));
}

void rtSwapchainResize(rt_swapchain swapchain, u32 width, u32 height) {
	rtdx_swapchain_resize(
		rtdx_get_current_context(),
		rtdx_swapchain_from_handle(swapchain),
		width,
		height
	);
}

rt_swapchain_acquire_result rtSwapchainAcquire(rt_swapchain swapchain) {
	return rtdx_swapchain_acquire(
		rtdx_get_current_context(),
		rtdx_swapchain_from_handle(swapchain)
	);
}

void rtSwapchainPresent(rt_swapchain swapchain, rt_timepoint rendered) {
	struct rtdx_timepoint timepoint = { rtdx_queue_from_handle(rendered.queue), rendered.value };
	rtdx_swapchain_present(
		rtdx_get_current_context(),
		rtdx_swapchain_from_handle(swapchain),
		timepoint
	);
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTDX_DEFINE_RESOURCE_PRIVATE(swapchain)

static bool rtdx_swapchain_create_framebuffers(struct rtdx_context* ctx, struct rtdx_swapchain* swapchain);
static bool rtdx_swapchain_prepare_present_command(struct rtdx_context* ctx, struct rtdx_swapchain_frame* frame);
static bool rtdx_swapchain_submit_present_transition(struct rtdx_context* ctx, struct rtdx_swapchain* swapchain, struct rtdx_timepoint rendered);

void rtdx_swapchain_init(struct rtdx_context* ctx, struct rtdx_swapchain* swapchain) {
	rtdx_init_resource_base(ctx, RTDX_RESOURCE_BASE(swapchain), rtdx_resource_type::swapchain);
	swapchain->dxgi_format = DXGI_FORMAT_B8G8R8A8_UNORM;
	swapchain->vsync = false;
	InitializeCriticalSection(&swapchain->frame_lock);
	InitializeConditionVariable(&swapchain->frame_condition);
}

static void rtdx_swapchain_lock(struct rtdx_swapchain* swapchain) {
	EnterCriticalSection(&swapchain->frame_lock);
}

static void rtdx_swapchain_unlock(struct rtdx_swapchain* swapchain) {
	LeaveCriticalSection(&swapchain->frame_lock);
}

static void rtdx_swapchain_lock_unacquired(struct rtdx_swapchain* swapchain) {
	rtdx_swapchain_lock(swapchain);
	while (swapchain->frame_acquired) {
		SleepConditionVariableCS(&swapchain->frame_condition, &swapchain->frame_lock, INFINITE);
	}
}

static void rtdx_swapchain_mark_unacquired(struct rtdx_swapchain* swapchain) {
	rtdx_swapchain_lock(swapchain);
	swapchain->frame_acquired = false;
	WakeAllConditionVariable(&swapchain->frame_condition);
	rtdx_swapchain_unlock(swapchain);
}

static void rtdx_swapchain_finish_sync(struct rtdx_swapchain* swapchain) {
	DeleteCriticalSection(&swapchain->frame_lock);
}

void rtdx_swapchain_wait_frame(struct rtdx_context* ctx, struct rtdx_swapchain_frame* frame) {
	if (!frame) {
		return;
	}
	if (frame->has_present_timepoint) {
		struct rtdx_timepoint timepoint = { frame->present_queue, frame->present_value };
		rtdx_timepoint_wait(ctx, timepoint);
		frame->present_queue = NULL;
		frame->present_value = 0;
		frame->has_present_timepoint = false;
	}
}

static void rtdx_swapchain_destroy_present_command(struct rtdx_swapchain_frame* frame) {
	rtdx_release(&frame->present_command_list);
	rtdx_release(&frame->present_allocator);
}

static void rtdx_swapchain_destroy_framebuffers(struct rtdx_context* ctx, struct rtdx_swapchain* swapchain) {
	for (u32 i = 0; i < RTDX_MAX_FRAMES_IN_FLIGHT; i++) {
		rtdx_swapchain_destroy_present_command(&swapchain->frames[i]);
		if (swapchain->framebuffers[i]) {
			rtdx_framebuffer_destroy(ctx, swapchain->framebuffers[i]);
			swapchain->framebuffers[i] = NULL;
		}
		// The texture/view wrappers may still be retained by a user command buffer that recorded
		// into the previous back buffer. ResizeBuffers requires zero outstanding references to the
		// swapchain's ID3D12Resource objects, so drop the back-buffer pointer directly here — the
		// wrappers' ref-counted cleanup will then find a NULL resource and skip the release.
		if (swapchain->texture_views[i]) {
			swapchain->texture_views[i]->d3d_resource = NULL;
			rtdx_texture_view_destroy(ctx, swapchain->texture_views[i]);
			swapchain->texture_views[i] = NULL;
		}
		if (swapchain->textures[i]) {
			if (swapchain->textures[i]->active) {
				rtdx_release(&swapchain->textures[i]->active->d3d_resource);
			}
			rtdx_texture_destroy(ctx, swapchain->textures[i]);
			swapchain->textures[i] = NULL;
		}
	}
}

bool rtdx_swapchain_resize(struct rtdx_context* ctx, struct rtdx_swapchain* swapchain, u32 width, u32 height) {
	if (!swapchain || !swapchain->dxgi_swapchain) {
		rtdx_throwf(RT_IMPROPER_USAGE, "swapchain resize requires a valid swapchain");
		return false;
	}
	if (width == 0 || height == 0) {
		return false;
	}
	if (width == swapchain->width && height == swapchain->height) {
		return true;
	}

	rtdx_swapchain_lock_unacquired(swapchain);
	for (u32 i = 0; i < RTDX_MAX_FRAMES_IN_FLIGHT; i++) {
		rtdx_swapchain_wait_frame(ctx, &swapchain->frames[i]);
	}

	rtdx_swapchain_destroy_framebuffers(ctx, swapchain);
	rtdx_release(&swapchain->rtv_heap);

	HRESULT result = swapchain->dxgi_swapchain->ResizeBuffers(
		RTDX_MAX_FRAMES_IN_FLIGHT,
		width,
		height,
		swapchain->dxgi_format,
		DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT | (ctx->allow_tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0)
	);
	if (FAILED(result)) {
		rtdx_swapchain_unlock(swapchain);
		rtdx_throwf(
			rtdx_error_from_hresult(result),
			"IDXGISwapChain3::ResizeBuffers failed: %s (0x%08x)",
			rtdx_hresult_name(result),
			(u32)result
		);
		return false;
	}

	swapchain->width = width;
	swapchain->height = height;
	swapchain->current_image_index = swapchain->dxgi_swapchain->GetCurrentBackBufferIndex();
	bool ok = rtdx_swapchain_create_framebuffers(ctx, swapchain);
	rtdx_swapchain_unlock(swapchain);
	return ok;
}

void rtdx_swapchain_set_vsync(struct rtdx_context* ctx, struct rtdx_swapchain* swapchain, bool enabled) {
	(void)ctx;
	if (!swapchain) {
		return;
	}
	swapchain->vsync = enabled;
}

void rtdx_swapchain_finish(struct rtdx_context* ctx, struct rtdx_swapchain* swapchain) {
	rtdx_swapchain_lock(swapchain);
	swapchain->frame_acquired = false;
	for (u32 i = 0; i < RTDX_MAX_FRAMES_IN_FLIGHT; ++i) {
		rtdx_swapchain_wait_frame(ctx, &swapchain->frames[i]);
	}
	rtdx_swapchain_destroy_framebuffers(ctx, swapchain);
	if (swapchain->frame_latency_object) {
		CloseHandle(swapchain->frame_latency_object);
		swapchain->frame_latency_object = NULL;
	}
	rtdx_release(&swapchain->rtv_heap);
	rtdx_release(&swapchain->dxgi_swapchain);
	rtdx_swapchain_unlock(swapchain);
	rtdx_swapchain_finish_sync(swapchain);
	rtdx_finish_resource_base(ctx, RTDX_RESOURCE_BASE(swapchain));
}

static bool rtdx_swapchain_create_framebuffers(struct rtdx_context* ctx, struct rtdx_swapchain* swapchain) {
	rtClearError();
	D3D12_DESCRIPTOR_HEAP_DESC heap_info = {};
	heap_info.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heap_info.NumDescriptors = RTDX_MAX_FRAMES_IN_FLIGHT;
	heap_info.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	heap_info.NodeMask = 0;

	HRESULT result = ctx->d3d_device->CreateDescriptorHeap(&heap_info, IID_PPV_ARGS(&swapchain->rtv_heap));
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "CreateDescriptorHeap(RTV) failed: 0x%08x", (u32)result);
		return false;
	}

	swapchain->rtv_descriptor_size = ctx->d3d_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	D3D12_CPU_DESCRIPTOR_HANDLE rtv = swapchain->rtv_heap->GetCPUDescriptorHandleForHeapStart();
	for (u32 i = 0; i < RTDX_MAX_FRAMES_IN_FLIGHT; i++) {
		ID3D12Resource* image = NULL;
		result = swapchain->dxgi_swapchain->GetBuffer(i, IID_PPV_ARGS(&image));
		if (FAILED(result)) {
			rtdx_throwf(rtdx_error_from_hresult(result), "IDXGISwapChain3::GetBuffer failed: 0x%08x", (u32)result);
			return false;
		}

		D3D12_RENDER_TARGET_VIEW_DESC rtv_info = {};
		rtv_info.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
		rtv_info.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtv_info.Texture2D.MipSlice = 0;
		rtv_info.Texture2D.PlaneSlice = 0;

		ctx->d3d_device->CreateRenderTargetView(image, &rtv_info, rtv);
		swapchain->textures[i] = rtdx_texture_create_for_swapchain_image(ctx, image, rtv_info.Format, swapchain->width, swapchain->height);
		image->Release();
		if (!swapchain->textures[i]) {
			rtdx_swapchain_destroy_framebuffers(ctx, swapchain);
			return false;
		}

		swapchain->texture_views[i] = rtdx_texture_view_create_for_swapchain(ctx, swapchain->textures[i], rtv);
		if (!swapchain->texture_views[i]) {
			rtdx_swapchain_destroy_framebuffers(ctx, swapchain);
			return false;
		}

		swapchain->framebuffers[i] = rtdx_framebuffer_create(ctx);
		if (!swapchain->framebuffers[i]) {
			rtdx_swapchain_destroy_framebuffers(ctx, swapchain);
			return false;
		}

		rtdx_framebuffer_set_color_view(ctx, swapchain->framebuffers[i], 0, swapchain->texture_views[i]);
		if (rtError() != RT_SUCCESS) {
			rtdx_swapchain_destroy_framebuffers(ctx, swapchain);
			return false;
		}

		rtv.ptr += swapchain->rtv_descriptor_size;
	}

	return true;
}

bool rtdx_swapchain_create_for_hwnd(struct rtdx_context* ctx, struct rtdx_swapchain* swapchain, HWND hwnd, u32 width, u32 height) {
	rtClearError();
	swapchain->hwnd = hwnd;
	swapchain->width = width;
	swapchain->height = height;

	DXGI_SWAP_CHAIN_DESC1 swapchain_info = {};
	swapchain_info.Width = width;
	swapchain_info.Height = height;
	swapchain_info.Format = swapchain->dxgi_format;
	swapchain_info.Stereo = FALSE;
	swapchain_info.SampleDesc.Count = 1;
	swapchain_info.SampleDesc.Quality = 0;
	swapchain_info.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapchain_info.BufferCount = RTDX_MAX_FRAMES_IN_FLIGHT;
	swapchain_info.Scaling = DXGI_SCALING_STRETCH;
	swapchain_info.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapchain_info.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapchain_info.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT | (ctx->allow_tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);

	struct rtdx_queue* queue = rtdx_queue_query(ctx, RT_QUEUE_GRAPHICS);
	IDXGISwapChain1* swapchain1 = NULL;
	HRESULT result = ctx->dxgi_factory->CreateSwapChainForHwnd(
		queue->d3d_queue,
		hwnd,
		&swapchain_info,
		NULL,
		NULL,
		&swapchain1
	);
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "CreateSwapChainForHwnd failed: 0x%08x", (u32)result);
		return false;
	}

	ctx->dxgi_factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
	result = swapchain1->QueryInterface(IID_PPV_ARGS(&swapchain->dxgi_swapchain));
	swapchain1->Release();
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "IDXGISwapChain3 QueryInterface failed: 0x%08x", (u32)result);
		return false;
	}

	result = swapchain->dxgi_swapchain->SetMaximumFrameLatency(16);
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "IDXGISwapChain3::SetMaximumFrameLatency failed: 0x%08x", (u32)result);
		return false;
	}
	swapchain->frame_latency_object = swapchain->dxgi_swapchain->GetFrameLatencyWaitableObject();

	if (!rtdx_swapchain_create_framebuffers(ctx, swapchain)) {
		return false;
	}

	return true;
}

rt_swapchain_acquire_result rtdx_swapchain_acquire(struct rtdx_context* ctx, struct rtdx_swapchain* swapchain) {
	rt_swapchain_acquire_result empty = { RT_NULL_HANDLE, { RT_NULL_HANDLE, 0 } };
	if (!swapchain || !swapchain->dxgi_swapchain) {
		rtdx_throwf(RT_IMPROPER_USAGE, "swapchain acquire requires a valid swapchain");
		return empty;
	}

	rtdx_swapchain_lock_unacquired(swapchain);
	swapchain->current_image_index = swapchain->dxgi_swapchain->GetCurrentBackBufferIndex();
	rtdx_swapchain_wait_frame(ctx, &swapchain->frames[swapchain->current_image_index]);
	if (!swapchain->framebuffers[swapchain->current_image_index]) {
		rtdx_throwf(RT_INITIALIZATION_FAILED, "swapchain framebuffer %u is unavailable", swapchain->current_image_index);
		rtdx_swapchain_unlock(swapchain);
		return empty;
	}
	rt_swapchain_acquire_result acquire = {
		rtdx_framebuffer_to_handle(swapchain->framebuffers[swapchain->current_image_index]),
		{ RT_NULL_HANDLE, 0 },
	};
	swapchain->frame_acquired = true;
	rtdx_swapchain_unlock(swapchain);
	return acquire;
}

static bool rtdx_swapchain_prepare_present_command(struct rtdx_context* ctx, struct rtdx_swapchain_frame* frame) {
	if (frame->present_allocator && frame->present_command_list) {
		return true;
	}

	HRESULT result = ctx->d3d_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frame->present_allocator));
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "CreateCommandAllocator(present) failed: 0x%08x", (u32)result);
		return false;
	}

	result = ctx->d3d_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frame->present_allocator, NULL, IID_PPV_ARGS(&frame->present_command_list));
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "CreateCommandList(present) failed: 0x%08x", (u32)result);
		rtdx_swapchain_destroy_present_command(frame);
		return false;
	}
	frame->present_command_list->Close();
	return true;
}

static bool rtdx_swapchain_submit_present_transition(struct rtdx_context* ctx, struct rtdx_swapchain* swapchain, struct rtdx_timepoint rendered) {
	struct rtdx_swapchain_frame* frame = &swapchain->frames[swapchain->current_image_index];
	struct rtdx_texture_view* view = swapchain->texture_views[swapchain->current_image_index];
	if (!rtdx_swapchain_prepare_present_command(ctx, frame)) {
		return false;
	}

	HRESULT result = frame->present_allocator->Reset();
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "ID3D12CommandAllocator::Reset(present) failed: 0x%08x", (u32)result);
		return false;
	}

	result = frame->present_command_list->Reset(frame->present_allocator, NULL);
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "ID3D12GraphicsCommandList::Reset(present) failed: 0x%08x", (u32)result);
		return false;
	}

	if (view->state != D3D12_RESOURCE_STATE_PRESENT) {
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = view->d3d_resource;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = view->state;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		frame->present_command_list->ResourceBarrier(1, &barrier);
		view->state = D3D12_RESOURCE_STATE_PRESENT;
	}

	result = frame->present_command_list->Close();
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "ID3D12GraphicsCommandList::Close(present) failed: 0x%08x", (u32)result);
		return false;
	}

	result = rendered.queue->d3d_queue->Wait(rendered.queue->d3d_fence, rendered.value);
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "ID3D12CommandQueue::Wait(present) failed: 0x%08x", (u32)result);
		return false;
	}

	ID3D12CommandList* lists[] = { frame->present_command_list };
	rendered.queue->d3d_queue->ExecuteCommandLists(1, lists);
	return true;
}

void rtdx_swapchain_present(struct rtdx_context* ctx, struct rtdx_swapchain* swapchain, struct rtdx_timepoint rendered) {
	struct rtdx_swapchain_frame* frame = &swapchain->frames[swapchain->current_image_index];
	if (!rendered.queue || rendered.value == 0) {
		rtdx_throwf(RT_IMPROPER_USAGE, "swapchain present requires a render timepoint");
		rtdx_swapchain_mark_unacquired(swapchain);
		return;
	}

	if (!rtdx_swapchain_submit_present_transition(ctx, swapchain, rendered)) {
		rtdx_swapchain_mark_unacquired(swapchain);
		return;
	}

	const UINT sync_interval = swapchain->vsync ? 1u : 0u;
	const UINT present_flags = !swapchain->vsync && ctx->allow_tearing ? DXGI_PRESENT_ALLOW_TEARING : 0u;
	HRESULT result = swapchain->dxgi_swapchain->Present(sync_interval, present_flags);
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "IDXGISwapChain::Present failed: 0x%08x", (u32)result);
		rtdx_swapchain_mark_unacquired(swapchain);
		return;
	}

	struct rtdx_timepoint present_done = rtdx_queue_signal(ctx, rendered.queue);
	if (rtError() != RT_SUCCESS) {
		rtdx_swapchain_mark_unacquired(swapchain);
		return;
	}
	frame->present_queue = present_done.queue;
	frame->present_value = present_done.value;
	frame->has_present_timepoint = present_done.value != 0;
	rtdx_swapchain_mark_unacquired(swapchain);
}
