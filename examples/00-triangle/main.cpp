#define RUTILE_IMPL
#include "rutile.h"
#include "rt_ext_glfw.h"
#include "rt_ext_swapchain.h"

#include <GLFW/glfw3.h>
#include <cstddef>
#include <cstring>
#include <cstdio>

struct Vertex {
	f32 position[2];
	f32 color[3];
};

static const Vertex kVertices[] = {
	{ {  0.0f, -0.6f }, { 1.0f, 0.2f, 0.2f } },
	{ {  0.6f,  0.5f }, { 0.2f, 1.0f, 0.3f } },
	{ { -0.6f,  0.5f }, { 0.3f, 0.4f, 1.0f } },
};

static const rt_vertex_attribute kAttributes[] = {
	{ 0, offsetof(Vertex, position), RT_RG32_SFLOAT },
	{ 1, offsetof(Vertex, color),    RT_RGB32_SFLOAT },
};
static const rt_vertex_layout kLayout = { sizeof(Vertex), kAttributes, 2 };

static const char* kVertexShader = R"(
#version 460
layout(location = 0) in vec2 in_position;
layout(location = 1) in vec3 in_color;
layout(location = 0) out vec3 v_color;
void main() {
	gl_Position = vec4(in_position, 0.0, 1.0);
	v_color = in_color;
}
)";

static const char* kFragmentShader = R"(
#version 460
layout(location = 0) in vec3 v_color;
layout(location = 0) out vec4 out_color;
void main() { out_color = vec4(v_color, 1.0); }
)";

int main(int argc, char** argv) {
	if (rtLoadDevelopment("rt-gl33", nullptr, 0) != RT_SUCCESS) { std::fprintf(stderr, "rtLoadDevelopment failed\n"); return 1; }
	rtLoad_RT_EXT_SWAPCHAIN();
	rtLoad_RT_EXT_GLFW();
	const char* features[] = { RT_FEATURE_PRESENTATION };
	rtInit(features, 1);

	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(960, 540, "Rutile 00 Triangle", nullptr, nullptr);

	rt_swapchain swapchain = rtSwapchainCreate();
	rtSwapchainBindWindowGLFW(swapchain, window);

	rt_queue queue = rtQueueQuery(RT_QUEUE_GRAPHICS);

	rt_buffer vbo = rtBufferCreate();
	rtBufferData(vbo, RT_BUFFER_STATIC, RT_BUFFER_USAGE_VERTEX, sizeof(kVertices), kVertices);

	rt_graphics_program program = rtGraphicsProgramCreate();
	rtGraphicsProgramVertexLayout(program, &kLayout);
	rtGraphicsProgramVertexShader(program, std::strlen(kVertexShader), kVertexShader);
	rtGraphicsProgramFragmentShader(program, std::strlen(kFragmentShader), kFragmentShader);
	rtGraphicsProgramLink(program);

	rt_command_buffer cmd = rtCommandBufferCreate();

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		rt_swapchain_acquire_result acquired = rtSwapchainAcquire(swapchain);
		if (!acquired.framebuffer) { continue; }

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
