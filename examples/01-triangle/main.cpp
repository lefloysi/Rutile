#define RUTILE_IMPL
#include "cli.hpp"
#include "rt_ext_glfw.h"
#include "rt_ext_swapchain.h"
#include "rutile.h"

#include <GLFW/glfw3.h>
#include <rtsl/sdk/program.hpp>

#include <cstddef>

extern "C" const rtsl::ProgramBytes triangle_rtslp;

struct Vertex {
	f32 position[2];
	f32 color[3];
};

static const Vertex kVertices[] = {
	{ { 0.0f, -0.6f }, { 1.0f, 0.2f, 0.2f } },
	{ { 0.6f, 0.5f }, { 0.2f, 1.0f, 0.3f } },
	{ { -0.6f, 0.5f }, { 0.3f, 0.4f, 1.0f } },
};

static const rt_vertex_attribute kAttributes[] = {
	{ "position", offsetof(Vertex, position), RT_RG32_SFLOAT },
	{ "color", offsetof(Vertex, color), RT_RGB32_SFLOAT },
};
static const rt_vertex_layout kLayout = { sizeof(Vertex), kAttributes, 2 };

int main(int argc, char** argv) {
	const ExampleOptions options = parse_cli(argc, argv);
	rtLoadDevelopment(options.backend.c_str(), nullptr, 0);
	rtLoad_RT_EXT_SWAPCHAIN();
	rtLoad_RT_EXT_GLFW();
	const char* features[] = { RT_FEATURE_PRESENTATION };
	rtInit(features, 1);

	rt_graphics_program program = rtGraphicsProgramCreate();
	rtGraphicsProgramSource(program, triangle_rtslp.size, triangle_rtslp.data);
	rtGraphicsProgramLayout(program, &kLayout);
	rtGraphicsProgramFinalize(program);

	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(960, 540, "Rutile 01 Triangle", nullptr, nullptr);

	rt_swapchain swapchain = rtSwapchainCreate();
	rtSwapchainBindWindowGLFW(swapchain, window);

	rt_queue queue = rtQueueQuery(RT_QUEUE_GRAPHICS);

	rt_buffer vbo = rtBufferCreate();
	rtBufferData(vbo, RT_BUFFER_STATIC, RT_BUFFER_USAGE_VERTEX, sizeof(kVertices), kVertices);

	rt_command_buffer cmd = rtCommandBufferCreate();
	u32 rendered_frames = 0;

	while (!glfwWindowShouldClose(window) && (!options.frames || rendered_frames < options.frames)) {
		glfwPollEvents();

		rt_swapchain_acquire_result acquired = rtSwapchainAcquire(swapchain);
		if (!acquired.framebuffer) {
			continue;
		}

		rtQueueWait(queue, acquired.timepoint);
		rtCmdBegin(cmd, queue);
		rtCmdBeginRendering(cmd, acquired.framebuffer);
		rtCmdClearColor(cmd, 0, 0.08f, 0.09f, 0.12f, 1.0f);
		rtCmdUseGraphicsProgram(cmd, program);
		rtCmdBindVertexBuffer(cmd, vbo, 0);
		rtCmdDraw(cmd, 3, 0);
		rtCmdEndRendering(cmd);
		rtCmdEnd(cmd);

		rtSwapchainPresent(swapchain, rtQueueSubmit(queue, cmd));
		rendered_frames++;
	}

	rtCommandBufferDestroy(cmd);
	rtGraphicsProgramDestroy(program);
	rtBufferDestroy(vbo);
	rtSwapchainDestroy(swapchain);
	rtExit();
	rtUnload();
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
