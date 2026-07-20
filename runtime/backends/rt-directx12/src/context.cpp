#include "context.hpp"
#include "error.hpp"
#include "resource/queue.hpp"

#include <chrono>
#include <new>
#include <vector>

rtdx_context* current_context = nullptr;

rtdx_context* rtdx_get_current_context() {
	return current_context;
}

static u64 rtdx_now_ns() {
	using clock = std::chrono::steady_clock;
	return static_cast<u64>(std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now().time_since_epoch()).count());
}

static void rtdx_log_startup_time(u64 start_ns) {
	u64 elapsed_ns = rtdx_now_ns() - start_ns;
	rtdx_printf("rt-directx12: initialized in %.3f ms\n", static_cast<double>(elapsed_ns) / 1000000.0);
}

static bool rtdx_context_create_factory(rtdx_context* ctx) {
	UINT flags = 0;
#if defined(RTDX_ENABLE_D3D12_VALIDATION)
	ID3D12Debug* debug = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
		debug->EnableDebugLayer();
		debug->Release();
		flags |= DXGI_CREATE_FACTORY_DEBUG;
	}
#endif

	HRESULT result = CreateDXGIFactory2(flags, IID_PPV_ARGS(&ctx->dxgi_factory));
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "CreateDXGIFactory2 failed: 0x%08x", static_cast<u32>(result));
		return false;
	}
	return true;
}

static void rtdx_context_query_present_features(rtdx_context* ctx) {
	IDXGIFactory5* factory5 = nullptr;
	if (FAILED(ctx->dxgi_factory->QueryInterface(IID_PPV_ARGS(&factory5)))) {
		return;
	}

	ctx->allow_tearing = false;
	factory5->Release();
}

static bool rtdx_context_pick_adapter(rtdx_context* ctx) {
	for (UINT i = 0;; i++) {
		IDXGIAdapter1* adapter = nullptr;
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
			rtdx_throwf(rtdx_error_from_hresult(result), "EnumAdapterByGpuPreference failed: 0x%08x", static_cast<u32>(result));
			return false;
		}

		adapter->GetDesc1(&desc);
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
			adapter->Release();
			continue;
		}

		if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr))) {
			ctx->dxgi_adapter = adapter;
			return true;
		}

		adapter->Release();
	}

	rtdx_throwf(RT_INCOMPATIBLE_DRIVER, "no compatible D3D12 adapter found");
	return false;
}

static bool rtdx_context_create_device(rtdx_context* ctx) {
	HRESULT result = D3D12CreateDevice(ctx->dxgi_adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&ctx->d3d_device));
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "D3D12CreateDevice failed: 0x%08x", static_cast<u32>(result));
		return false;
	}

	ctx->queues = new (std::nothrow) rtdx_queue*[1]{};
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

static void rtdx_context_destroy_queues(rtdx_context* ctx) {
	for (u32 i = 0; i < ctx->queue_count; i++) {
		rtdx_queue_destroy(ctx, ctx->queues[i]);
	}
	delete[] ctx->queues;
	ctx->queues = nullptr;
	ctx->queue_count = 0;
}

rtdx_context* rtdx_create_context(rtdx_context_flags flags) {
	auto* result = new (std::nothrow) rtdx_context{};
	if (!result) {
		rtdx_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate DirectX 12 context");
		return nullptr;
	}

	result->flags = flags;
	rtdx_context_init(result);
	if (rtError() != RT_SUCCESS) {
		rtdx_context_destroy(result);
		return nullptr;
	}

	return result;
}

void rtdx_context_init(rtdx_context* ctx) {
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

void rtdx_context_finish(rtdx_context* ctx) {
	if (!ctx) {
		return;
	}

	ctx->shutting_down = true;
	rtdx_context_destroy_queues(ctx);
	rtdx_release(&ctx->d3d_device);
	rtdx_release(&ctx->dxgi_adapter);
	rtdx_release(&ctx->dxgi_factory);
}

void rtdx_context_destroy(rtdx_context* ctx) {
	if (!ctx) {
		return;
	}

	rtdx_context_finish(ctx);
	delete ctx;
}

void rtdx_context_report_validation(rtdx_context* ctx) {
#if defined(RTDX_ENABLE_D3D12_VALIDATION)
	ID3D12InfoQueue* messages = nullptr;
	if (!ctx || !ctx->d3d_device || FAILED(ctx->d3d_device->QueryInterface(IID_PPV_ARGS(&messages)))) {
		return;
	}
	const UINT64 count = messages->GetNumStoredMessagesAllowedByRetrievalFilter();
	for (UINT64 index = 0; index < count; ++index) {
		SIZE_T size = 0;
		messages->GetMessage(index, nullptr, &size);
		std::vector<unsigned char> storage(size);
		auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
		if (SUCCEEDED(messages->GetMessage(index, message, &size))) {
			rtdx_printf("D3D12 validation: %s\n", message->pDescription);
		}
	}
	messages->ClearStoredMessages();
	messages->Release();
#else
	(void)ctx;
#endif
}
