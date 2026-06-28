#define RUTILE_IMPL
#include "rt_ext_glfw.h"
#include "rt_ext_swapchain.h"
#include "rutile.h"

#include <GLFW/glfw3.h>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <iterator>
#include <windows.h>
#include <vector>

struct Vertex {
	f32 position[2];
	f32 color[3];
};

static const Vertex kVertices[] = {
	{{0.0f, -0.6f}, {1.0f, 0.2f, 0.2f}},
	{{0.6f, 0.5f}, {0.2f, 1.0f, 0.3f}},
	{{-0.6f, 0.5f}, {0.3f, 0.4f, 1.0f}},
};

static const rt_vertex_attribute kAttributes[] = {
	{"position", offsetof(Vertex, position), RT_RG32_SFLOAT},
	{"color", offsetof(Vertex, color), RT_RGB32_SFLOAT},
};
static const rt_vertex_layout kLayout = {sizeof(Vertex), kAttributes, 2};

static std::vector<char> read_binary_file(const char* path) {
	std::ifstream file(path, std::ios::binary);
	if (!file) {
		std::fprintf(stderr, "failed to open %s\n", path);
		return {};
	}

	return std::vector<char>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

static std::filesystem::path executable_dir(const char* argv0) {
	std::wstring buffer;
	buffer.resize(32768);
	const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
	if (length > 0 && length < buffer.size()) {
		buffer.resize(length);
		return std::filesystem::path(buffer).parent_path();
	}

	std::filesystem::path path = argv0 ? std::filesystem::path(argv0) : std::filesystem::path();
	return path.has_parent_path() ? path.parent_path() : std::filesystem::current_path();
}

static void rt_output(const char* message, void*) {
	if (message) {
		std::fputs(message, stdout);
		std::fflush(stdout);
	}
}

static bool check_rt(const char* step) {
	const enum rt_error error = rtError();
	if (error == RT_SUCCESS) {
		return true;
	}

	std::fprintf(stderr, "%s failed: %d %s\n", step, (int)error, rtErrorMessage());
	rtClearError();
	return false;
}

int main(int argc, char** argv) {
	if (rtLoadDevelopment("rt-vulkan", nullptr, 0) != RT_SUCCESS) {
		std::fprintf(stderr, "rtLoadDevelopment failed\n");
		return 1;
	}
	rtLoad_RT_EXT_SWAPCHAIN();
	rtLoad_RT_EXT_GLFW();
	const char* features[] = {RT_FEATURE_PRESENTATION};
	rtSetOutput(rt_output, nullptr);
	rtInit(features, 1);
	if (!check_rt("rtInit")) {
		return 1;
	}

	rt_graphics_program program = rtGraphicsProgramCreate();
	const std::filesystem::path asset_dir = executable_dir(argc > 0 ? argv[0] : nullptr);
	const std::filesystem::path triangle_path = asset_dir / "triangle.rtslp";
	const std::vector<char> triangle_program = read_binary_file(triangle_path.string().c_str());
	if (triangle_program.empty()) {
		std::fprintf(stderr, "failed to load %s\n", triangle_path.string().c_str());
		return 1;
	}
	rtGraphicsProgramSource(program, triangle_program.size(), triangle_program.data());
	rtGraphicsProgramLayout(program, &kLayout);
	rtGraphicsProgramLink(program);
	if (!check_rt("rtGraphicsProgramLink")) {
		return 1;
	}

	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(960, 540, "Rutile 00 Triangle", nullptr, nullptr);

	rt_swapchain swapchain = rtSwapchainCreate();
	rtSwapchainBindWindowGLFW(swapchain, window);
	if (!check_rt("rtSwapchainBindWindowGLFW")) {
		return 1;
	}

	rt_queue queue = rtQueueQuery(RT_QUEUE_GRAPHICS);
	if (!check_rt("rtQueueQuery")) {
		return 1;
	}

	rt_buffer vbo = rtBufferCreate();
	rtBufferData(vbo, RT_BUFFER_STATIC, RT_BUFFER_USAGE_VERTEX, sizeof(kVertices), kVertices);
	if (!check_rt("rtBufferData")) {
		return 1;
	}

	rt_command_buffer cmd = rtCommandBufferCreate();
	if (!check_rt("rtCommandBufferCreate")) {
		return 1;
	}

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		rt_swapchain_acquire_result acquired = rtSwapchainAcquire(swapchain);
		if (!check_rt("rtSwapchainAcquire")) {
			break;
		}
		if (!acquired.framebuffer) {
			continue;
		}

		rtQueueWait(queue, acquired.timepoint);
		if (!check_rt("rtQueueWait")) {
			break;
		}
		rtCmdBegin(cmd, queue);
		rtCmdBeginRendering(cmd, acquired.framebuffer);
		rtCmdClearColor(cmd, 0, 0.08f, 0.09f, 0.12f, 1.0f);
		rtCmdUseGraphicsProgram(cmd, program);
		rtCmdBindVertexBuffer(cmd, vbo, 0);
		rtCmdDraw(cmd, 3, 0);
		rtCmdEndRendering(cmd);
		rtCmdEnd(cmd);
		if (!check_rt("record commands")) {
			break;
		}

		rtSwapchainPresent(swapchain, rtQueueSubmit(queue, cmd));
		if (!check_rt("present")) {
			break;
		}
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
