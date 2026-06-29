#include "context.h"
#include "error.h"
#include "resource/queue.h"

#include <chrono>
#include <stdlib.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtdx_context* current_context = NULL;

struct rtdx_context* rtdx_get_current_context(void) {
	return current_context;
}

static u64 rtdx_now_ns(void) {
	using clock = std::chrono::steady_clock;
	return (u64)std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now().time_since_epoch()).count();
}

static void rtdx_log_startup_time(u64 start_ns) {
	u64 elapsed_ns = rtdx_now_ns() - start_ns;
	rtdx_printf("rt-dx12: initialized in %.3f ms\n", (double)elapsed_ns / 1000000.0);
}

static bool rtdx_context_create_factory(struct rtdx_context* ctx) {
	UINT flags = 0;
#if defined(RTDX_ENABLE_D3D12_VALIDATION)
	ID3D12Debug* debug = NULL;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
		debug->EnableDebugLayer();
		debug->Release();
		flags |= DXGI_CREATE_FACTORY_DEBUG;
	}
#endif

	HRESULT result = CreateDXGIFactory2(flags, IID_PPV_ARGS(&ctx->dxgi_factory));
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "CreateDXGIFactory2 failed: 0x%08x", (u32)result);
		return false;
	}
	return true;
}

static void rtdx_context_query_present_features(struct rtdx_context* ctx) {
	IDXGIFactory5* factory5 = NULL;
	if (FAILED(ctx->dxgi_factory->QueryInterface(IID_PPV_ARGS(&factory5)))) {
		return;
	}

	ctx->allow_tearing = false;
	factory5->Release();
}

static bool rtdx_context_pick_adapter(struct rtdx_context* ctx) {
	for (UINT i = 0;; i++) {
		IDXGIAdapter1* adapter = NULL;
		DXGI_ADAPTER_DESC1 desc;
		HRESULT result = ctx->dxgi_factory->EnumAdapterByGpuPreference(
			i,
			DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
			IID_PPV_ARGS(&adapter)
		);
		if (result == DXGI_ERROR_NOT_FOUND) {
			break;
		}
		if (FAILED(result)) {
			rtdx_throwf(rtdx_error_from_hresult(result), "EnumAdapterByGpuPreference failed: 0x%08x", (u32)result);
			return false;
		}

		adapter->GetDesc1(&desc);
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
			adapter->Release();
			continue;
		}

		if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), NULL))) {
			ctx->dxgi_adapter = adapter;
			return true;
		}

		adapter->Release();
	}

	rtdx_throwf(RT_INCOMPATIBLE_DRIVER, "no compatible D3D12 adapter found");
	return false;
}

static bool rtdx_context_create_device(struct rtdx_context* ctx) {
	HRESULT result = D3D12CreateDevice(ctx->dxgi_adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&ctx->d3d_device));
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "D3D12CreateDevice failed: 0x%08x", (u32)result);
		return false;
	}

	ctx->queues = (struct rtdx_queue**)calloc(1, sizeof(*ctx->queues));
	if (!ctx->queues) {
		rtdx_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate D3D12 queue handles");
		return false;
	}

	ctx->queues[0] = rtdx_queue_create(ctx, RT_QUEUE_GRAPHICS);
	if (!ctx->queues[0]) {
		return false;
	}
	ctx->queue_count = 1;
	return true;
}

static void rtdx_context_destroy_queues(struct rtdx_context* ctx) {
	for (u32 i = 0; i < ctx->queue_count; i++) {
		rtdx_queue_destroy(ctx, ctx->queues[i]);
	}
	free(ctx->queues);
	ctx->queues = NULL;
	ctx->queue_count = 0;
}

struct rtdx_context* rtdx_create_context(rtdx_context_flags flags) {
	struct rtdx_context* result = (struct rtdx_context*)calloc(1, sizeof(struct rtdx_context));
	if (!result) {
		rtdx_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %zu bytes for DX12 context", sizeof(struct rtdx_context));
		return NULL;
	}

	result->flags = flags;
	rtdx_context_init(result);
	if (rtError() != RT_SUCCESS) {
		rtdx_context_destroy(result);
		return NULL;
	}

	return result;
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

void rtdx_context_init(struct rtdx_context* ctx) {
	u64 start_ns = rtdx_now_ns();
	if (!rtdx_context_create_factory(ctx)) {
		return;
	}
	rtdx_context_query_present_features(ctx);
	if (!rtdx_context_pick_adapter(ctx)) {
		return;
	}
	if (!rtdx_context_create_device(ctx)) {
		return;
	}

	rtdx_log_startup_time(start_ns);
}

void rtdx_context_finish(struct rtdx_context* ctx) {
	if (!ctx) {
		return;
	}

	ctx->shutting_down = true;
	rtdx_context_destroy_queues(ctx);
	rtdx_release(&ctx->d3d_device);
	rtdx_release(&ctx->dxgi_adapter);
	rtdx_release(&ctx->dxgi_factory);
}

void rtdx_context_destroy(struct rtdx_context* ctx) {
	if (!ctx) {
		return;
	}

	rtdx_context_finish(ctx);
	free(ctx);
}
