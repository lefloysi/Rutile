#include "resource/swapchain_win32.hpp"

#include "context.hpp"
#include "error.hpp"
#include "resource/queue.hpp"

bool rtdx_swapchain_create_for_hwnd(rtdx_context* ctx, rtdx_swapchain* swapchain, HWND hwnd, u32 width, u32 height) {
	if (!ctx || !swapchain || !hwnd) {
		rtdx_throwf(RT_IMPROPER_USAGE, "swapchain, context, and HWND must be valid");
		return false;
	}

	rtClearError();
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

	rtdx_queue* queue = rtdx_queue_query(ctx, RT_QUEUE_GRAPHICS);
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

	return rtdx_swapchain_create_framebuffers(ctx, swapchain);
}
