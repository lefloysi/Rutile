extern "C" {
#include "resource/swapchain.h"
#include "error.h"
}
#include <cstdio>
#include <chrono>
#include <exception>
#include <future>
#include <memory>

namespace {
struct BuildResult {
	rtvk_swapchain_generation* generation;
	rt_error error;
	double elapsed_ms;
	char message[1024];
};
struct SwapchainBuild { std::future<BuildResult> result; };
}
extern "C" void* rtvk_swapchain_begin_build(rtvk_context* ctx, rtvk_swapchain* swapchain) {
	const auto old = swapchain->generation->vk_swapchain;
	const auto surface = swapchain->surface;
	const auto width = swapchain->requested_width, height = swapchain->requested_height;
	try {
		auto build = std::make_unique<SwapchainBuild>();
		build->result = std::async(std::launch::async, [=] {
			rtvk_begin_errorable_operation();
			BuildResult result{};
			const auto started = std::chrono::steady_clock::now();
			result.generation = rtvk_swapchain_generation_build(ctx, surface, width, height, old, swapchain);
			result.elapsed_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
			result.error = rtError();
			std::snprintf(result.message, sizeof(result.message), "%s", rtErrorMessage());
			return result;
		});
		return build.release();
	} catch (const std::exception& error) {
		rtvk_throwf(RT_PLATFORM_FAILURE, "starting swapchain preparation: %s", error.what());
		return nullptr;
	}
}
extern "C" rtvk_swapchain_generation* rtvk_swapchain_finish_build(void* value) {
	std::unique_ptr<SwapchainBuild> build{ static_cast<SwapchainBuild*>(value) };
	try {
		const auto result = build->result.get();
		rtvk_printf("[resize-vulkan] background preparation %.3f ms\n", result.elapsed_ms);
		if (result.error != RT_SUCCESS) { rtvk_throwf(result.error, "%s", result.message); }
		return result.generation;
	} catch (const std::exception& error) {
		rtvk_throwf(RT_PLATFORM_FAILURE, "finishing swapchain preparation: %s", error.what());
		return nullptr;
	}
}
