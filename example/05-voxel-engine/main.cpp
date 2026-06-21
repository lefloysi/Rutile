#define RUTILE_IMPL
#include "world.h"
#include "rutile.h"
#include "rt_ext_glfw.h"
#include "rt_ext_swapchain.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>

constexpr const char* kDefaultBackendName = "rt-vulkan";
constexpr const char* kLayers[] = { "RT_VALIDATION" };
constexpr const char* kFeatures[] = { RT_FEATURE_PRESENTATION };

struct TransformUniform {
	f32 transform[16];
	f32 time;
	f32 padding[3];
};

typedef TransformUniform WaterUniform;

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
	{ 0, offsetof(Vertex, position), RT_RGB32_SFLOAT },
	{ 1, offsetof(Vertex, color), RT_RGB32_SFLOAT },
	{ 2, offsetof(Vertex, normal), RT_RGB32_SFLOAT },
	{ 3, offsetof(Vertex, ao), RT_R32_SFLOAT },
	{ 4, offsetof(Vertex, pixel_uv), RT_RG32_SFLOAT },
	{ 5, offsetof(Vertex, edge_mask), RT_R32_SFLOAT },
	{ 6, offsetof(Vertex, corner_mask), RT_R32_SFLOAT },
};

constexpr rt_vertex_layout kVertexLayout = { sizeof(Vertex), kVertexAttributes, 7 };

constexpr const char* kVertexShader = R"(
#version 460
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in float in_ao;
layout(location = 4) in vec2 in_pixel_uv;
layout(location = 5) in float in_edge_mask;
layout(location = 6) in float in_corner_mask;
layout(location = 0) flat out vec3 color;
layout(location = 1) flat out vec3 normal;
layout(location = 2) out float ao;
layout(location = 3) out vec2 pixel_uv;
layout(location = 4) flat out int edge_mask;
layout(location = 5) flat out int corner_mask;

layout(set = 0, binding = 0) uniform Transform {
	mat4 transform;
} u_transform;

void main() {
	gl_Position = u_transform.transform * vec4(in_position, 1.0);
	color = in_color;
	normal = in_normal;
	ao = in_ao;
	pixel_uv = in_pixel_uv;
	edge_mask = int(in_edge_mask + 0.5);
	corner_mask = int(in_corner_mask + 0.5);
}
)";

constexpr const char* kFragmentShader = R"(
#version 460
layout(location = 0) flat in vec3 color;
layout(location = 1) flat in vec3 normal;
layout(location = 2) in float ao;
layout(location = 3) in vec2 pixel_uv;
layout(location = 4) flat in int edge_mask;
layout(location = 5) flat in int corner_mask;
layout(location = 0) out vec4 out_color;

const float BlockPixelSize = 8.0;
const float BlockEdgePixelShade = 0.82;

bool mask_bit(int mask, int bit) {
	return (mask & bit) != 0;
}

bool masked_edge_pixel(ivec2 p) {
	return
		(p.x == 0 && mask_bit(edge_mask, 1)) ||
		(p.x == 7 && mask_bit(edge_mask, 2)) ||
		(p.y == 0 && mask_bit(edge_mask, 4)) ||
		(p.y == 7 && mask_bit(edge_mask, 8));
}

bool masked_corner_pixel(ivec2 p) {
	return
		(p.x == 0 && p.y == 0 && mask_bit(corner_mask, 1)) ||
		(p.x == 7 && p.y == 0 && mask_bit(corner_mask, 2)) ||
		(p.x == 7 && p.y == 7 && mask_bit(corner_mask, 4)) ||
		(p.x == 0 && p.y == 7 && mask_bit(corner_mask, 8));
}

void main() {
	ivec2 p = clamp(ivec2(floor(pixel_uv * BlockPixelSize)), ivec2(0), ivec2(7));
	bool outline_pixel = masked_edge_pixel(p) || masked_corner_pixel(p);
	vec3 pixel_color = color * (outline_pixel ? BlockEdgePixelShade : 1.0);
	vec3 n = normalize(normal);
	vec3 light_dir = normalize(vec3(-0.35, 0.85, -0.45));
	vec3 sky_dir = vec3(0.0, 1.0, 0.0);
	float diffuse = max(dot(n, light_dir), 0.0);
	float skylight = clamp(dot(n, sky_dir) * 0.5 + 0.5, 0.0, 1.0);
	float bounce = max(dot(n, normalize(vec3(0.4, 0.35, 0.7))), 0.0) * 0.12;
	vec3 ambient = mix(vec3(0.16, 0.18, 0.22), vec3(0.30, 0.35, 0.42), skylight);
	vec3 sunlight = vec3(1.0, 0.94, 0.82) * diffuse;
	vec3 lit = pixel_color * (ambient + sunlight + bounce) * clamp(ao, 0.35, 1.0);
	out_color = vec4(lit, 1.0);
}
)";

constexpr const char* kWaterVertexShader = R"(
#version 460
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in float in_ao;
layout(location = 4) in vec2 in_pixel_uv;
layout(location = 5) in float in_edge_mask;
layout(location = 6) in float in_corner_mask;
layout(location = 0) out vec3 color;
layout(location = 1) out vec3 normal;
layout(location = 2) out vec3 world_position;

layout(set = 0, binding = 0) uniform Transform {
	mat4 transform;
	float time;
} u_transform;

void main() {
	vec3 position = in_position;
	float wave_a = sin(position.x * 1.45 + u_transform.time * 1.8);
	float wave_b = cos(position.z * 1.20 + u_transform.time * 1.35);
	position.y += (wave_a + wave_b) * 0.055;
	gl_Position = u_transform.transform * vec4(position, 1.0);
	color = in_color;
	normal = normalize(vec3(-wave_a * 0.08, 1.0, -wave_b * 0.08));
	world_position = position;
}
)";

constexpr const char* kWaterFragmentShader = R"(
#version 460
layout(location = 0) in vec3 color;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 world_position;
layout(location = 0) out vec4 out_color;

void main() {
	vec3 n = normalize(normal);
	vec3 light_dir = normalize(vec3(-0.35, 0.85, -0.45));
	float diffuse = max(dot(n, light_dir), 0.0);
	float sparkle = pow(max(dot(n, normalize(vec3(-0.2, 0.95, 0.15))), 0.0), 56.0);
	float ripple = sin(world_position.x * 4.0 + world_position.z * 3.0) * 0.5 + 0.5;
	float fresnel = pow(1.0 - clamp(dot(n, normalize(vec3(0.0, 0.72, 0.68))), 0.0, 1.0), 3.0);
	vec3 ambient = vec3(0.08, 0.16, 0.24);
	vec3 deep_water = color * vec3(0.55, 0.75, 0.95);
	vec3 bright_water = vec3(0.24, 0.55, 0.78);
	vec3 water = mix(deep_water, bright_water, ripple * 0.18 + fresnel * 0.32);
	water *= ambient + vec3(0.35, 0.55, 0.72) * diffuse;
	water += vec3(0.58, 0.78, 0.92) * sparkle;
	out_color = vec4(clamp(water, vec3(0.0), vec3(1.0)), 0.72);
}
)";

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

void write_transform(TransformUniform* uniform, const glm::mat4& transform) {
	std::memcpy(uniform->transform, glm::value_ptr(transform), sizeof(uniform->transform));
	uniform->time = 0.0f;
	uniform->padding[0] = 0.0f;
	uniform->padding[1] = 0.0f;
	uniform->padding[2] = 0.0f;
}

void write_water_transform(WaterUniform* uniform, const glm::mat4& transform, f32 time) {
	std::memcpy(uniform->transform, glm::value_ptr(transform), sizeof(uniform->transform));
	uniform->time = time;
	uniform->padding[0] = 0.0f;
	uniform->padding[1] = 0.0f;
	uniform->padding[2] = 0.0f;
}

void recreate_depth_buffer(rt_queue queue, rt_texture* depth_texture, rt_texture_view* depth_view, u32 width, u32 height) {
	if (*depth_view) { rtTextureViewDestroy(*depth_view); }
	if (*depth_texture) { rtTextureDestroy(*depth_texture); }
	*depth_texture = rtTextureCreate();
	rtTimepointWait(rtTextureData(queue, *depth_texture, RT_TEXTURE_2D, 0, width, height, 1, RT_D32_SFLOAT, NULL));
	*depth_view = rtTextureViewCreate(*depth_texture);
	if (!*depth_view) {
		rtTextureDestroy(*depth_texture);
		*depth_texture = RT_NULL_HANDLE;
	}
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

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) { velocity += flat_forward; }
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) { velocity -= flat_forward; }
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { velocity += right; }
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { velocity -= right; }
	if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) { velocity.y += 1.0f; }
	if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) { velocity.y -= 1.0f; }
	if (glm::dot(velocity, velocity) > 0.0f) {
		camera->position += glm::normalize(velocity) * speed * dt;
	}
}

int main(int argc, char** argv) {
	const char* backend_name = argc > 1 ? argv[1] : kDefaultBackendName;
	rtLoad(backend_name, kLayers, 1);
	rtLoad_RT_EXT_SWAPCHAIN();
	rtLoad_RT_EXT_GLFW();

	rtInit(kFeatures, 1);

	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(1280, 720, "Rutile 05 Voxel Engine", nullptr, nullptr);

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

	TransformUniform transform = {};
	rt_buffer transform_buffer = rtBufferCreate();
	rtBufferData(transform_buffer, RT_BUFFER_DYNAMIC, RT_BUFFER_USAGE_UNIFORM, sizeof(transform), &transform);

	WaterUniform water_transform = {};
	rt_buffer water_transform_buffer = rtBufferCreate();
	rtBufferData(water_transform_buffer, RT_BUFFER_DYNAMIC, RT_BUFFER_USAGE_UNIFORM, sizeof(water_transform), &water_transform);

	rt_graphics_program graphics_program = rtGraphicsProgramCreate();
	rtGraphicsProgramVertexLayout(graphics_program, &kVertexLayout);
	rtGraphicsProgramVertexShader(graphics_program, std::strlen(kVertexShader), kVertexShader);
	rtGraphicsProgramFragmentShader(graphics_program, std::strlen(kFragmentShader), kFragmentShader);
	rtGraphicsProgramRasterState(graphics_program, RT_CULL_BACK, RT_FRONT_FACE_CCW, RT_FILL_SOLID);
	rtGraphicsProgramLink(graphics_program);
	rt_uniform_location transform_location = rtGraphicsProgramUniformLocation(graphics_program, "Transform");

	rt_graphics_program water_program = rtGraphicsProgramCreate();
	rtGraphicsProgramVertexLayout(water_program, &kVertexLayout);
	rtGraphicsProgramVertexShader(water_program, std::strlen(kWaterVertexShader), kWaterVertexShader);
	rtGraphicsProgramFragmentShader(water_program, std::strlen(kWaterFragmentShader), kWaterFragmentShader);
	rtGraphicsProgramRasterState(water_program, RT_CULL_BACK, RT_FRONT_FACE_CCW, RT_FILL_SOLID);
	rtGraphicsProgramBlendState(water_program, true, RT_BLEND_SRC_ALPHA, RT_BLEND_ONE_MINUS_SRC_ALPHA, RT_BLEND_OP_ADD, RT_BLEND_ONE, RT_BLEND_ONE_MINUS_SRC_ALPHA, RT_BLEND_OP_ADD);
	rtGraphicsProgramLink(water_program);
	rt_uniform_location water_transform_location = rtGraphicsProgramUniformLocation(water_program, "Transform");

	rt_command_buffer cmd = rtCmdCreate();
	rt_texture depth_texture = RT_NULL_HANDLE;
	rt_texture_view depth_view = RT_NULL_HANDLE;
	u32 depth_width = FramebufferWidth.load(std::memory_order_acquire);
	u32 depth_height = FramebufferHeight.load(std::memory_order_acquire);
	recreate_depth_buffer(queue, &depth_texture, &depth_view, depth_width, depth_height);

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
			recreate_depth_buffer(queue, &depth_texture, &depth_view, depth_width, depth_height);
			continue;
		}

		rt_swapchain_acquire_result acquired = rtSwapchainAcquire(swapchain);
		if (!acquired.framebuffer) { continue; }

		const f32 aspect = current_height ? (f32)current_width / (f32)current_height : 1.0f;
		const glm::mat4 view_projection = camera_matrix(camera, aspect);
		const std::chrono::duration<f32> elapsed = now - start_time;
		write_transform(&transform, view_projection);
		write_water_transform(&water_transform, view_projection, elapsed.count());
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
			std::snprintf(title, sizeof(title), "Rutile 05 Voxel Engine - %.0f FPS", fps);
			glfwSetWindowTitle(window, title);
			fps_time = fps_now;
			fps_frames = 0;
		}
	}

	rtCmdDestroy(cmd);
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
