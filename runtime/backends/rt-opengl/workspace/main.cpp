#define RUTILE_IMPL
#include "rt_ext_glfw.h"
#include "rt_ext_swapchain.h"
#include "rutile.h"

#include <GLFW/glfw3.h>

#include <cstring>
#include <cstdio>

int main() {
	rtLoadDevelopment("rt-opengl", nullptr, 0);

	const char* features[] = { RT_FEATURE_PRESENTATION };
	rtInit(features, 1);
	rtLoad_RT_EXT_SWAPCHAIN();
	rtLoad_RT_EXT_GLFW();

	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(640, 360, "rt-opengl workspace", nullptr, nullptr);

	rt_swapchain swapchain = rtSwapchainCreate();
	rtSwapchainBindWindowGLFW(swapchain, window);
	rt_queue queue = rtQueueQuery(RT_QUEUE_GRAPHICS);

	u32 upload[] = { 1, 2, 3, 4 };
	u32 update[] = { 20, 30 };
	u32 expected[] = { 1, 20, 30, 4 };
	u32 download[sizeof(expected) / sizeof(expected[0])] = {};

	rt_buffer buffer = rtBufferCreate();
	rtBufferData(buffer, RT_BUFFER_DYNAMIC, (rt_buffer_usage)(RT_BUFFER_USAGE_STAGING | RT_BUFFER_USAGE_TRANSFER_SRC | RT_BUFFER_USAGE_TRANSFER_DST), sizeof(upload), upload);
	rtBufferSubdata(buffer, sizeof(u32), sizeof(update), update);
	rtBufferRead(buffer, 0, sizeof(download), download);
		
	if (std::memcmp(download, expected, sizeof(expected)) != 0) {
		std::fprintf(stderr, "rt-opengl-workspace: buffer readback mismatch: {%u, %u, %u, %u}\n", download[0], download[1], download[2], download[3]);
		rtBufferDestroy(buffer);
		rtSwapchainDestroy(swapchain);
		rtExit();
		rtUnload();
		glfwDestroyWindow(window);
		glfwTerminate();
		return 1;
	}

	rt_command_buffer command_buffer = rtCommandBufferCreate();
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
			glfwSetWindowShouldClose(window, GLFW_TRUE);
		}
		if (glfwWindowShouldClose(window)) {
			break;
		}

		rt_swapchain_acquire_result acquired = rtSwapchainAcquire(swapchain);
		rtCmdBegin(command_buffer, queue);
		rtCmdBeginRendering(command_buffer, acquired.framebuffer);
		rtCmdClearColor(command_buffer, 0, 0.02f, 0.11f, 0.18f, 1.0f);
		rtCmdEndRendering(command_buffer);
		rtCmdEnd(command_buffer);

		rt_timepoint cleared = rtQueueSubmit(queue, command_buffer);
		rtSwapchainPresent(swapchain, cleared);
		rtQueueFlush(queue);
	}
	rtQueueFlush(queue);

	std::printf("rt-opengl-workspace: initialized %s, verified buffer upload/readback, and cleared the swapchain\n", rtGetName());

	rtCommandBufferDestroy(command_buffer);
	rtBufferDestroy(buffer);
	rtSwapchainDestroy(swapchain);
	rtExit();
	rtUnload();
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
