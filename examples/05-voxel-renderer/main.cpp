#define RUTILE_IMPL
#include "cli.hpp"
#include "rt_ext_glfw.h"
#include "rt_ext_swapchain.h"
#include "rutile.h"
#include "world.h"

#include <GLFW/glfw3.h>
#include <rtsl/sdk/program.hpp>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

constexpr const char* kLayers[] = { "RT_VALIDATION_LAYER" };
constexpr const char* kFeatures[] = { RT_FEATURE_PRESENTATION };

extern "C" const rtsl::ProgramBytes terrain_rtslp;
extern "C" const rtsl::ProgramBytes water_rtslp;

struct Camera {
	glm::vec3 position = glm::vec3(0.0f, 13.0f, 18.0f);
	f32 yaw = -glm::radians(90.0f);
	f32 pitch = -0.32f;
};

std::atomic<rt_swapchain> Swapchain = RT_NULL_HANDLE;
std::atomic<u32> FramebufferWidth = 1280;
std::atomic<u32> FramebufferHeight = 720;
std::atomic<bool> ResizePending = false;
std::atomic<bool> ResizeInProgress = false;
f32 MouseDx = 0.0f;
f32 MouseDy = 0.0f;

constexpr rt_vertex_attribute kVertexAttributes[] = {
	{ "position", offsetof(Vertex, position), RT_RGB32_SFLOAT },
	{ "color", offsetof(Vertex, color), RT_RGB32_SFLOAT },
	{ "normal", offsetof(Vertex, normal), RT_RGB32_SFLOAT },
	{ "ao", offsetof(Vertex, ao), RT_R32_SFLOAT },
	{ "pixel_uv", offsetof(Vertex, pixel_uv), RT_RG32_SFLOAT },
	{ "edge_mask", offsetof(Vertex, edge_mask), RT_R32_SFLOAT },
	{ "corner_mask", offsetof(Vertex, corner_mask), RT_R32_SFLOAT },
};

constexpr rt_vertex_layout kVertexLayout = { sizeof(Vertex), kVertexAttributes, 7 };

glm::vec3 camera_forward(const Camera& camera) {
	const f32 cp = glm::cos(camera.pitch);
	return glm::normalize(glm::vec3(glm::cos(camera.yaw) * cp, glm::sin(camera.pitch), glm::sin(camera.yaw) * cp));
}

glm::mat4 camera_matrix(const Camera& camera, f32 aspect) {
	const glm::vec3 forward = camera_forward(camera);
	const glm::mat4 view = glm::lookAt(camera.position, camera.position + forward, glm::vec3(0, 1, 0));
	const glm::mat4 projection = glm::perspective(glm::radians(70.0f), aspect, 0.05f, 180.0f);
	return projection * view;
}

void resize_depth_buffer(rt_texture depth_texture, rt_texture_view depth_view, u32 width, u32 height) {
	rtTimepointWait(rtTextureData(depth_texture, RT_TEXTURE_2D, 0, width, height, 1, RT_D32_SFLOAT, NULL));
	rtTextureViewBind(depth_view, depth_texture);
}

void framebuffer_resized(GLFWwindow* window, int width, int height) {
	(void)window;
	if (ResizeInProgress.load(std::memory_order_acquire)) {
		return;
	}
	if (width > 0 && height > 0) {
		FramebufferWidth.store((u32)width, std::memory_order_release);
		FramebufferHeight.store((u32)height, std::memory_order_release);
		ResizePending.store(true, std::memory_order_release);
	}
}

void cursor_moved(GLFWwindow* window, double x, double y) {
	(void)window;

	static double previous_x = x;
	static double previous_y = y;
	MouseDx += (f32)(x - previous_x);
	MouseDy += (f32)(y - previous_y);
	previous_x = x;
	previous_y = y;
}

void update_camera(GLFWwindow* window, Camera* camera, f32 dt) {
	const f32 sensitivity = 0.0024f;
	camera->yaw += MouseDx * sensitivity;
	camera->pitch -= MouseDy * sensitivity;
	camera->pitch = glm::clamp(camera->pitch, -1.45f, 1.45f);
	MouseDx = 0.0f;
	MouseDy = 0.0f;

	const glm::vec3 forward = camera_forward(*camera);
	const glm::vec3 flat_forward = glm::normalize(glm::vec3(forward.x, 0.0f, forward.z));
	const glm::vec3 right = glm::normalize(glm::cross(flat_forward, glm::vec3(0, 1, 0)));
	const f32 speed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 18.0f : 8.0f;
	glm::vec3 velocity(0.0f);

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
		velocity += flat_forward;
	}
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
		velocity -= flat_forward;
	}
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
		velocity += right;
	}
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
		velocity -= right;
	}
	if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
		velocity.y += 1.0f;
	}
	if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
		velocity.y -= 1.0f;
	}
	if (glm::dot(velocity, velocity) > 0.0f) {
		camera->position += glm::normalize(velocity) * speed * dt;
	}
}

int main(int argc, char** argv) {
	const ExampleOptions options = parse_cli(argc, argv);
	rtLoad(options.backend.c_str(), kLayers, 1);
	rtLoad_RT_EXT_SWAPCHAIN();
	rtLoad_RT_EXT_GLFW();

	rtInit(kFeatures, 1);

	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(1280, 720, "Rutile 05 Voxel Renderer", nullptr, nullptr);

	glfwSetFramebufferSizeCallback(window, framebuffer_resized);
	glfwSetCursorPosCallback(window, cursor_moved);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	if (glfwRawMouseMotionSupported()) {
		glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
	}

	int framebuffer_width = 0;
	int framebuffer_height = 0;
	glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
	if (framebuffer_width > 0 && framebuffer_height > 0) {
		FramebufferWidth.store((u32)framebuffer_width, std::memory_order_release);
		FramebufferHeight.store((u32)framebuffer_height, std::memory_order_release);
	}

	rt_swapchain swapchain = rtSwapchainCreate();
	rtSwapchainBindWindowGLFW(swapchain, window);
	Swapchain.store(swapchain, std::memory_order_release);
	rt_queue queue = rtQueueQuery(RT_QUEUE_GRAPHICS);

	std::vector<Vertex> vertices = build_world_mesh();
	rt_buffer vertex_buffer = rtBufferCreate();
	const u64 vertex_size = (u64)(vertices.size() * sizeof(vertices[0]));
	rtBufferData(vertex_buffer, RT_BUFFER_STATIC, RT_BUFFER_USAGE_VERTEX, vertex_size, vertices.data());

	std::vector<Vertex> water_vertices = build_water_mesh();
	rt_buffer water_vertex_buffer = rtBufferCreate();
	const u64 water_vertex_size = (u64)(water_vertices.size() * sizeof(water_vertices[0]));
	rtBufferData(water_vertex_buffer, RT_BUFFER_STATIC, RT_BUFFER_USAGE_VERTEX, water_vertex_size, water_vertices.data());

	glm::mat4 transform{ 1.0f };
	rt_buffer transform_buffer = rtBufferCreate();
	rtBufferData(transform_buffer, RT_BUFFER_DYNAMIC, RT_BUFFER_USAGE_UNIFORM, sizeof(transform), &transform);

	glm::mat4 water_transform{ 1.0f };
	rt_buffer water_transform_buffer = rtBufferCreate();
	rtBufferData(water_transform_buffer, RT_BUFFER_DYNAMIC, RT_BUFFER_USAGE_UNIFORM, sizeof(water_transform), &water_transform);

	rt_graphics_program graphics_program = rtGraphicsProgramCreate();
	rtGraphicsProgramLayout(graphics_program, &kVertexLayout);
	rtGraphicsProgramSource(graphics_program, terrain_rtslp.size, terrain_rtslp.data);
	rtGraphicsProgramRasterState(graphics_program, RT_CULL_BACK, RT_FRONT_FACE_CCW, RT_FILL_SOLID);
	rtGraphicsProgramFinalize(graphics_program);
	rt_uniform_location transform_location = rtGraphicsProgramUniformLocation(graphics_program, "scene");

	rt_graphics_program water_program = rtGraphicsProgramCreate();
	rtGraphicsProgramLayout(water_program, &kVertexLayout);
	rtGraphicsProgramSource(water_program, water_rtslp.size, water_rtslp.data);
	rtGraphicsProgramRasterState(water_program, RT_CULL_BACK, RT_FRONT_FACE_CCW, RT_FILL_SOLID);
	rtGraphicsProgramBlendState(water_program, true, RT_BLEND_SRC_ALPHA, RT_BLEND_ONE_MINUS_SRC_ALPHA, RT_BLEND_OP_ADD, RT_BLEND_ONE, RT_BLEND_ONE_MINUS_SRC_ALPHA, RT_BLEND_OP_ADD);
	rtGraphicsProgramFinalize(water_program);
	rt_uniform_location water_transform_location = rtGraphicsProgramUniformLocation(water_program, "scene");

	rt_command_buffer cmd = rtCommandBufferCreate();
	rt_texture depth_texture = rtTextureCreate();
	rt_texture_view depth_view = rtTextureViewCreate();
	u32 depth_width = FramebufferWidth.load(std::memory_order_acquire);
	u32 depth_height = FramebufferHeight.load(std::memory_order_acquire);
	resize_depth_buffer(depth_texture, depth_view, depth_width, depth_height);

	Camera camera;
	auto start_time = std::chrono::steady_clock::now();
	auto previous_time = start_time;
	auto fps_time = start_time;
	u32 fps_frames = 0;
	rt_timepoint last_rendered = { RT_NULL_HANDLE, 0 };

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
			glfwSetWindowShouldClose(window, GLFW_TRUE);
		}

		auto now = std::chrono::steady_clock::now();
		const std::chrono::duration<f32> delta = now - previous_time;
		previous_time = now;
		update_camera(window, &camera, delta.count());

		const u32 current_width = FramebufferWidth.load(std::memory_order_acquire);
		const u32 current_height = FramebufferHeight.load(std::memory_order_acquire);
		const bool depth_size_changed = current_width != depth_width || current_height != depth_height;
		const bool resize_pending = ResizePending.load(std::memory_order_acquire);
		if (current_width && current_height && (resize_pending || depth_size_changed)) {
			rtTimepointWait(last_rendered);
			depth_width = current_width;
			depth_height = current_height;
			ResizePending.store(false, std::memory_order_release);
			ResizeInProgress.store(true, std::memory_order_release);
			rtSwapchainResize(swapchain, depth_width, depth_height);
			ResizeInProgress.store(false, std::memory_order_release);
			resize_depth_buffer(depth_texture, depth_view, depth_width, depth_height);
			continue;
		}

		rt_swapchain_acquire_result acquired = rtSwapchainAcquire(swapchain);
		if (!acquired.framebuffer) {
			continue;
		}

		const f32 aspect = current_height ? (f32)current_width / (f32)current_height : 1.0f;
		const glm::mat4 view_projection = camera_matrix(camera, aspect);
		const std::chrono::duration<f32> elapsed = now - start_time;
		transform = view_projection;
		water_transform = view_projection * glm::translate(
			glm::mat4{ 1.0f },
			glm::vec3{ 0.0f, glm::sin(elapsed.count() * 1.8f) * 0.035f, 0.0f }
		);
		rtBufferSubdata(transform_buffer, 0, sizeof(transform), &transform);
		rtBufferSubdata(water_transform_buffer, 0, sizeof(water_transform), &water_transform);

		rtFramebufferDepthView(acquired.framebuffer, depth_view);
		rtCmdBegin(cmd, queue);
		rtCmdBeginRendering(cmd, acquired.framebuffer);
		rtCmdClearColor(cmd, 0, 0.54f, 0.72f, 0.94f, 1.0f);
		rtCmdClearDepth(cmd, 1.0f);
		rtCmdUseGraphicsProgram(cmd, graphics_program);
		rtCmdBindVertexBuffer(cmd, vertex_buffer, 0);
		rtCmdUniformBuffer(cmd, transform_location, transform_buffer, 0, sizeof(transform));
		rtCmdDraw(cmd, (u32)vertices.size(), 0);
		rtCmdUseGraphicsProgram(cmd, water_program);
		rtCmdBindVertexBuffer(cmd, water_vertex_buffer, 0);
		rtCmdUniformBuffer(cmd, water_transform_location, water_transform_buffer, 0, sizeof(water_transform));
		rtCmdDraw(cmd, (u32)water_vertices.size(), 0);
		rtCmdEndRendering(cmd);
		rtCmdEnd(cmd);

		rt_timepoint rendered = rtQueueSubmit(queue, cmd);
		last_rendered = rendered;
		rtFramebufferDepthView(acquired.framebuffer, RT_NULL_HANDLE);
		rtSwapchainPresent(swapchain, rendered);

		fps_frames++;
		const auto fps_now = std::chrono::steady_clock::now();
		const std::chrono::duration<f32> fps_delta = fps_now - fps_time;
		if (fps_delta.count() >= 0.5f) {
			char title[96];
			const f32 fps = (f32)fps_frames / fps_delta.count();
			std::snprintf(title, sizeof(title), "Rutile 05 Voxel Renderer - %.0f FPS", fps);
			glfwSetWindowTitle(window, title);
			fps_time = fps_now;
			fps_frames = 0;
		}
	}

	rtTimepointWait(last_rendered);
	rtCommandBufferDestroy(cmd);
	rtGraphicsProgramDestroy(water_program);
	rtGraphicsProgramDestroy(graphics_program);
	rtBufferDestroy(water_transform_buffer);
	rtBufferDestroy(transform_buffer);
	rtBufferDestroy(water_vertex_buffer);
	rtBufferDestroy(vertex_buffer);
	rtTextureViewDestroy(depth_view);
	rtTextureDestroy(depth_texture);
	rtSwapchainDestroy(swapchain);
	Swapchain.store(RT_NULL_HANDLE, std::memory_order_release);
	glfwDestroyWindow(window);
	glfwTerminate();
	rtExit();
	rtUnload();
	return 0;
}
