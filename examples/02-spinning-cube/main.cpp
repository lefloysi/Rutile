#define RUTILE_IMPL
#include "cli.hpp"
#include "rt_ext_glfw.h"
#include "rt_ext_swapchain.h"
#include "rutile.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <rtsl/sdk/program.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <vector>

extern "C" const rtsl::ProgramBytes cube_rtslp;

struct Vertex {
	f32 position[3];
	f32 color[3];
	f32 normal[3];
};

struct Scene {
	f32 transform[16];
};

struct Camera {
	glm::vec3 position = glm::vec3(0.0f, 0.0f, 4.0f);
	f32 yaw = -glm::radians(90.0f);
	f32 pitch = 0.0f;
};

std::atomic<rt_swapchain> Swapchain = RT_NULL_HANDLE;
std::atomic<u32> FramebufferWidth = 1280;
std::atomic<u32> FramebufferHeight = 720;
f32 MouseDx = 0.0f;
f32 MouseDy = 0.0f;

constexpr rt_vertex_attribute kAttributes[] = {
	{ "position", offsetof(Vertex, position), RT_RGB32_SFLOAT },
	{ "color", offsetof(Vertex, color), RT_RGB32_SFLOAT },
	{ "normal", offsetof(Vertex, normal), RT_RGB32_SFLOAT },
};

constexpr rt_vertex_layout kLayout = { sizeof(Vertex), kAttributes, 3 };

std::vector<Vertex> make_cube() {
	struct Face {
		glm::vec3 normal;
		glm::vec3 color;
		std::array<glm::vec3, 4> corners;
	};
	const std::array<Face, 6> faces = {
		Face{ { 0, 0, 1 }, { 1.0f, 0.30f, 0.24f }, {{{ -1, -1, 1 }, { 1, -1, 1 }, { 1, 1, 1 }, { -1, 1, 1 }}} },
		Face{ { 0, 0, -1 }, { 0.25f, 0.52f, 1.0f }, {{{ 1, -1, -1 }, { -1, -1, -1 }, { -1, 1, -1 }, { 1, 1, -1 }}} },
		Face{ { 1, 0, 0 }, { 0.32f, 1.0f, 0.48f }, {{{ 1, -1, 1 }, { 1, -1, -1 }, { 1, 1, -1 }, { 1, 1, 1 }}} },
		Face{ { -1, 0, 0 }, { 0.78f, 0.34f, 1.0f }, {{{ -1, -1, -1 }, { -1, -1, 1 }, { -1, 1, 1 }, { -1, 1, -1 }}} },
		Face{ { 0, 1, 0 }, { 1.0f, 0.78f, 0.22f }, {{{ -1, 1, 1 }, { 1, 1, 1 }, { 1, 1, -1 }, { -1, 1, -1 }}} },
		Face{ { 0, -1, 0 }, { 0.20f, 0.88f, 0.94f }, {{{ -1, -1, -1 }, { 1, -1, -1 }, { 1, -1, 1 }, { -1, -1, 1 }}} },
	};
	std::vector<Vertex> vertices;
	vertices.reserve(36);
	for (const Face& face : faces) {
		for (const u32 index : { 0u, 1u, 2u, 0u, 2u, 3u }) {
			const glm::vec3& point = face.corners[index];
			vertices.push_back({
				{ point.x, point.y, point.z },
				{ face.color.r, face.color.g, face.color.b },
				{ face.normal.x, face.normal.y, face.normal.z },
			});
		}
	}
	return vertices;
}

void framebuffer_resized(GLFWwindow*, int width, int height) {
	if (width <= 0 || height <= 0) {
		return;
	}
	FramebufferWidth.store(static_cast<u32>(width), std::memory_order_release);
	FramebufferHeight.store(static_cast<u32>(height), std::memory_order_release);
	rtSwapchainResize(Swapchain.load(std::memory_order_acquire), static_cast<u32>(width), static_cast<u32>(height));
}

void cursor_moved(GLFWwindow*, double x, double y) {
	static double previous_x = x;
	static double previous_y = y;
	MouseDx += static_cast<f32>(x - previous_x);
	MouseDy += static_cast<f32>(y - previous_y);
	previous_x = x;
	previous_y = y;
}

glm::vec3 camera_forward(const Camera& camera) {
	const f32 pitch_cosine = glm::cos(camera.pitch);
	return glm::normalize(glm::vec3(glm::cos(camera.yaw) * pitch_cosine, glm::sin(camera.pitch), glm::sin(camera.yaw) * pitch_cosine));
}

void update_camera(GLFWwindow* window, Camera& camera, f32 delta) {
	camera.yaw += MouseDx * 0.0022f;
	camera.pitch = glm::clamp(camera.pitch - MouseDy * 0.0022f, glm::radians(-85.0f), glm::radians(85.0f));
	MouseDx = 0.0f;
	MouseDy = 0.0f;
	const glm::vec3 forward = camera_forward(camera);
	const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
	glm::vec3 motion(0.0f);
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
		motion += forward;
	}
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
		motion -= forward;
	}
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
		motion += right;
	}
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
		motion -= right;
	}
	if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
		motion.y += 1.0f;
	}
	if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
		motion.y -= 1.0f;
	}
	if (glm::dot(motion, motion) > 0.0f) {
		camera.position += glm::normalize(motion) * delta * 2.5f;
	}
}

int main(int argc, char** argv) {
	const ExampleOptions options = parse_cli(argc, argv);
	rtLoadDevelopment(options.backend.c_str(), nullptr, 0);
	rtLoad_RT_EXT_SWAPCHAIN();
	rtLoad_RT_EXT_GLFW();
	const char* features[] = { RT_FEATURE_PRESENTATION };
	rtInit(features, 1);

	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(1280, 720, "Rutile 02 Spinning Cube", nullptr, nullptr);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetCursorPosCallback(window, cursor_moved);
	glfwSetFramebufferSizeCallback(window, framebuffer_resized);

	rt_swapchain swapchain = rtSwapchainCreate();
	rtSwapchainBindWindowGLFW(swapchain, window);
	Swapchain.store(swapchain, std::memory_order_release);
	rt_queue queue = rtQueueQuery(RT_QUEUE_GRAPHICS);

	const std::vector<Vertex> vertices = make_cube();
	rt_buffer vertex_buffer = rtBufferCreate();
	rtBufferData(vertex_buffer, RT_BUFFER_STATIC, RT_BUFFER_USAGE_VERTEX, vertices.size() * sizeof(Vertex), vertices.data());

	Scene scene = {};
	rt_buffer scene_buffer = rtBufferCreate();
	rtBufferData(scene_buffer, RT_BUFFER_DYNAMIC, RT_BUFFER_USAGE_UNIFORM, sizeof(scene), &scene);
	rt_graphics_program program = rtGraphicsProgramCreate();
	rtGraphicsProgramSource(program, cube_rtslp.size, cube_rtslp.data);
	rtGraphicsProgramLayout(program, &kLayout);
	rtGraphicsProgramRasterState(program, RT_CULL_NONE, RT_FRONT_FACE_CCW, RT_FILL_SOLID);
	rtGraphicsProgramFinalize(program);
	rt_uniform_location scene_location = rtGraphicsProgramUniformLocation(program, "scene");

	rt_texture depth = rtTextureCreate();
	rtTextureData(depth, RT_TEXTURE_2D, 0, 1280, 720, 1, RT_D32_SFLOAT, nullptr);
	rt_texture_view depth_view = rtTextureViewCreate();
	rtTextureViewBind(depth_view, depth);
	u32 depth_width = 1280;
	u32 depth_height = 720;
	rt_command_buffer command_buffer = rtCommandBufferCreate();
	rt_timepoint last_rendered = {};
	Camera camera;
	const auto start = std::chrono::steady_clock::now();
	auto previous = start;
	u32 rendered_frames = 0;

	while (!glfwWindowShouldClose(window) && (!options.frames || rendered_frames < options.frames)) {
		glfwPollEvents();
		const auto now = std::chrono::steady_clock::now();
		const f32 delta = std::chrono::duration<f32>(now - previous).count();
		const f32 time = std::chrono::duration<f32>(now - start).count();
		previous = now;
		update_camera(window, camera, delta);
		const u32 width = FramebufferWidth.load(std::memory_order_acquire);
		const u32 height = FramebufferHeight.load(std::memory_order_acquire);
		if (width != depth_width || height != depth_height) {
			rtTimepointWait(last_rendered);
			depth_width = width;
			depth_height = height;
			rtTextureData(depth, RT_TEXTURE_2D, 0, width, height, 1, RT_D32_SFLOAT, nullptr);
		}
		const glm::mat4 view = glm::lookAt(camera.position, camera.position + camera_forward(camera), glm::vec3(0, 1, 0));
		const glm::mat4 projection = glm::perspective(glm::radians(60.0f), static_cast<f32>(width) / static_cast<f32>(height), 0.1f, 100.0f);
		const glm::mat4 model = glm::rotate(glm::rotate(glm::mat4(1.0f), time * 0.72f, glm::vec3(0, 1, 0)), time * 0.31f, glm::vec3(1, 0, 0));
		const glm::mat4 transform = projection * view * model;
		std::memcpy(scene.transform, glm::value_ptr(transform), sizeof(scene.transform));
		rtBufferSubdata(scene_buffer, 0, sizeof(scene), &scene);

		rt_swapchain_acquire_result acquired = rtSwapchainAcquire(swapchain);
		if (!acquired.framebuffer) {
			continue;
		}
		rtQueueWait(queue, acquired.timepoint);
		rtFramebufferDepthView(acquired.framebuffer, depth_view);
		rtCmdBegin(command_buffer, queue);
		rtCmdBeginRendering(command_buffer, acquired.framebuffer);
		rtCmdClearColor(command_buffer, 0, 0.035f, 0.045f, 0.075f, 1.0f);
		rtCmdClearDepth(command_buffer, 1.0f);
		rtCmdUseGraphicsProgram(command_buffer, program);
		rtCmdUniformBuffer(command_buffer, scene_location, scene_buffer, 0, sizeof(scene));
		rtCmdBindVertexBuffer(command_buffer, vertex_buffer, 0);
		rtCmdDraw(command_buffer, static_cast<u32>(vertices.size()), 0);
		rtCmdEndRendering(command_buffer);
		rtCmdEnd(command_buffer);
		last_rendered = rtQueueSubmit(queue, command_buffer);
		rtFramebufferDepthView(acquired.framebuffer, RT_NULL_HANDLE);
		rtSwapchainPresent(swapchain, last_rendered);
		rendered_frames++;
	}

	rtTimepointWait(last_rendered);
	rtCommandBufferDestroy(command_buffer);
	rtTextureViewDestroy(depth_view);
	rtTextureDestroy(depth);
	rtGraphicsProgramDestroy(program);
	rtBufferDestroy(scene_buffer);
	rtBufferDestroy(vertex_buffer);
	rtSwapchainDestroy(swapchain);
	glfwDestroyWindow(window);
	glfwTerminate();
	rtExit();
	rtUnload();
	return 0;
}
