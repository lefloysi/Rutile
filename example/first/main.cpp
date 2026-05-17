#define RUTILE_IMPL
#include "rutile.h"
#include "rt_ext_glfw.h"
#include "rt_ext_swapchain.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

constexpr const char* kDefaultBackendName = "rt-vulkan";
constexpr const char* kLayers[] = { "RT_VALIDATION", "RT_LOGGING_LAYER" };
constexpr const char* kFeatures[] = { RT_FEATURE_PRESENTATION };

struct Vertex {
	f32 position[3];
	f32 color[3];
	f32 uv[2];
};

struct TransformUniform {
	f32 transform[16];
};

struct CameraState {
	glm::vec3 position = glm::vec3(0.0f, 0.0f, 2.35f);
	f32 yaw = -glm::radians(90.0f);
	f32 pitch = 0.0f;
};

struct InputState {
	bool forward = false;
	bool backward = false;
	bool left = false;
	bool right = false;
	bool up = false;
	bool down = false;
	f32 mouse_dx = 0.0f;
	f32 mouse_dy = 0.0f;
};

struct AppState {
	std::atomic<rt_swapchain> swapchain = RT_NULL_HANDLE;
	std::atomic<u32> framebuffer_width = 1280;
	std::atomic<u32> framebuffer_height = 720;
	std::mutex input_mutex;
	InputState input;
	std::atomic_bool running = true;
	std::atomic_bool render_failed = false;
};

constexpr Vertex kVertices[] = {
	{ { -0.65f, -0.65f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f } },
	{ { 0.65f, -0.65f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } },
	{ { 0.65f, 0.65f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f } },
	{ { -0.65f, -0.65f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f } },
	{ { 0.65f, 0.65f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f } },
	{ { -0.65f, 0.65f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f } },

	{ { -0.65f, -0.65f, 0.0f }, { 0.55f, 0.9f, 1.0f }, { 0.0f, 1.0f } },
	{ { 0.65f, -0.65f, 0.0f }, { 0.55f, 0.9f, 1.0f }, { 1.0f, 1.0f } },
	{ { 0.65f, 0.65f, 0.0f }, { 0.55f, 0.9f, 1.0f }, { 1.0f, 0.0f } },
	{ { -0.65f, -0.65f, 0.0f }, { 0.55f, 0.9f, 1.0f }, { 0.0f, 1.0f } },
	{ { 0.65f, 0.65f, 0.0f }, { 0.55f, 0.9f, 1.0f }, { 1.0f, 0.0f } },
	{ { -0.65f, 0.65f, 0.0f }, { 0.55f, 0.9f, 1.0f }, { 0.0f, 0.0f } },
};

constexpr u32 kQuadVertexCount = 6;
constexpr u32 kMovingQuadFirstVertex = 0;
constexpr u32 kStaticQuadFirstVertex = 6;

constexpr rt_vertex_attribute kVertexAttributes[] = {
	{ 0, offsetof(Vertex, position), RT_RGB32_SFLOAT },
	{ 1, offsetof(Vertex, color), RT_RGB32_SFLOAT },
	{ 2, offsetof(Vertex, uv), RT_RG32_SFLOAT },
};

constexpr rt_vertex_layout kVertexLayout = { sizeof(Vertex), kVertexAttributes, 3,
};

constexpr const char* kVertexShader = R"(
#version 460
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;
layout(location = 2) in vec2 in_uv;
layout(location = 0) out vec3 color;
layout(location = 1) out vec2 uv;

layout(set = 0, binding = 0) uniform Transform {
	mat4 transform;
} u_transform;

void main() {
	gl_Position = u_transform.transform * vec4(in_position, 1.0);
	color = in_color;
	uv = in_uv;
}
)";

constexpr const char* kFragmentShader = R"(
#version 460
layout(location = 0) in vec3 color;
layout(location = 1) in vec2 uv;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 1) uniform sampler2D Image;

void main() {
	out_color = texture(Image, uv) * vec4(color, 1.0);
}
)";

constexpr const char* kTexturePath = "rutile.png";

void framebuffer_resized(GLFWwindow* window, int width, int height) {
	AppState* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
	if (state && width > 0 && height > 0) {
		state->framebuffer_width.store(static_cast<u32>(width), std::memory_order_release);
		state->framebuffer_height.store(static_cast<u32>(height), std::memory_order_release);
	}

	rt_swapchain swapchain = state ? state->swapchain.load(std::memory_order_acquire) : RT_NULL_HANDLE;
	if (!swapchain || width <= 0 || height <= 0) {
		return;
	}

	rtSwapchainResize(swapchain, static_cast<u32>(width), static_cast<u32>(height));
}

void cursor_moved(GLFWwindow* window, double x, double y) {
	AppState* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
	if (!state) { return; }

	static double previous_x = x;
	static double previous_y = y;
	const f32 dx = static_cast<f32>(x - previous_x);
	const f32 dy = static_cast<f32>(y - previous_y);
	previous_x = x;
	previous_y = y;

	std::lock_guard<std::mutex> lock(state->input_mutex);
	state->input.mouse_dx += dx;
	state->input.mouse_dy += dy;
}

bool key_down(GLFWwindow* window, int key) {
	return glfwGetKey(window, key) == GLFW_PRESS;
}

void update_camera_input(GLFWwindow* window, AppState* state) {
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	}

	std::lock_guard<std::mutex> lock(state->input_mutex);
	state->input.forward = key_down(window, GLFW_KEY_W);
	state->input.backward = key_down(window, GLFW_KEY_S);
	state->input.left = key_down(window, GLFW_KEY_A);
	state->input.right = key_down(window, GLFW_KEY_D);
	state->input.up = key_down(window, GLFW_KEY_E) || key_down(window, GLFW_KEY_SPACE);
	state->input.down = key_down(window, GLFW_KEY_Q) || key_down(window, GLFW_KEY_LEFT_CONTROL);
}

InputState consume_input(AppState* state) {
	std::lock_guard<std::mutex> lock(state->input_mutex);
	InputState input = state->input;
	state->input.mouse_dx = 0.0f;
	state->input.mouse_dy = 0.0f;
	return input;
}

glm::vec3 camera_forward(const CameraState& camera) {
	const f32 cp = glm::cos(camera.pitch);
	return glm::normalize(glm::vec3(glm::cos(camera.yaw) * cp, glm::sin(camera.pitch), glm::sin(camera.yaw) * cp));
}

void update_camera(CameraState* camera, const InputState& input, f32 delta_seconds) {
	const f32 move_speed = 2.2f;
	const f32 mouse_sensitivity = 0.0022f;

	camera->yaw += input.mouse_dx * mouse_sensitivity;
	camera->pitch -= input.mouse_dy * mouse_sensitivity;
	camera->pitch = glm::clamp(camera->pitch, glm::radians(-82.0f), glm::radians(82.0f));

	const glm::vec3 forward = camera_forward(*camera);
	const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
	glm::vec3 motion = glm::vec3(0.0f);
	if (input.forward) { motion += forward; }
	if (input.backward) { motion -= forward; }
	if (input.right) { motion += right; }
	if (input.left) { motion -= right; }
	if (input.up) { motion.y += 1.0f; }
	if (input.down) { motion.y -= 1.0f; }
	if (glm::length(motion) > 0.0f) {
		camera->position += glm::normalize(motion) * move_speed * delta_seconds;
	}
}

glm::mat4 camera_view_projection(f32 aspect, const CameraState& camera) {
	const glm::vec3 target = camera.position + camera_forward(camera);
	const glm::mat4 view = glm::lookAt(camera.position, target, glm::vec3(0.0f, 1.0f, 0.0f));
	const glm::mat4 projection = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 10.0f);
	return projection * view;
}

TransformUniform transform_from_matrix(const glm::mat4& matrix) {
	TransformUniform uniform = {};
	std::memcpy(uniform.transform, glm::value_ptr(matrix), sizeof(uniform.transform));
	return uniform;
}

TransformUniform moving_transform(double time_seconds, const glm::mat4& view_projection) {
	const f32 t = static_cast<f32>(time_seconds);
	const f32 angle = t * 1.35f;
	glm::mat4 model = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 1.0f, 0.0f));
	model = glm::rotate(model, angle * 0.35f, glm::vec3(1.0f, 0.0f, 0.0f));
	return transform_from_matrix(view_projection * model);
}

TransformUniform static_transform(const glm::mat4& view_projection) {
	glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(0.24f, 0.0f, -0.35f));
	model = glm::scale(model, glm::vec3(0.82f));
	return transform_from_matrix(view_projection * model);
}

void recreate_depth_buffer(
	rt_queue queue,
	rt_texture* depth_texture,
	rt_texture_view* depth_view,
	u32 width,
	u32 height) {
	if (*depth_view) { rtTextureViewDestroy(*depth_view); }
	if (*depth_texture) { rtTextureDestroy(*depth_texture); }
	*depth_texture = rtTextureCreate();
	rtTextureData(queue, *depth_texture, RT_TEXTURE_2D, 0, width, height, 1, RT_D32_SFLOAT, NULL);
	*depth_view = rtTextureViewCreate(*depth_texture);
	if (!*depth_view) {
		rtTextureDestroy(*depth_texture);
		*depth_texture = RT_NULL_HANDLE;
	}
}

f32 camera_aspect(AppState* state) {
	const u32 width = state->framebuffer_width.load(std::memory_order_acquire);
	const u32 height = state->framebuffer_height.load(std::memory_order_acquire);
	return height ? (f32)width / (f32)height : 1.0f;
}

double render_time_seconds(std::chrono::steady_clock::time_point start_time) {
	using seconds = std::chrono::duration<double>;
	return seconds(std::chrono::steady_clock::now() - start_time).count();
}

void render_thread_main(AppState* state) {
	rt_swapchain swapchain = state->swapchain.load(std::memory_order_acquire);
	rt_queue queue = rtQueueQuery(RT_QUEUE_GRAPHICS);

	int texture_width = 0;
	int texture_height = 0;
	stbi_uc* texture_pixels = stbi_load(kTexturePath, &texture_width, &texture_height, nullptr, 4);
	if (!texture_pixels) {
		std::cerr << "failed to load " << kTexturePath << "\n";
		state->render_failed.store(true, std::memory_order_release);
		state->running.store(false, std::memory_order_release);
		return;
	}

	rt_buffer vertex_buffer = rtBufferCreate();
	rtBufferData(vertex_buffer, RT_BUFFER_STATIC, RT_BUFFER_USAGE_VERTEX, sizeof(kVertices), kVertices);

	CameraState camera;
	glm::mat4 view_projection = camera_view_projection(camera_aspect(state), camera);
	TransformUniform moving_uniform = moving_transform(0.0, view_projection);
	TransformUniform static_uniform = static_transform(view_projection);
	rt_buffer moving_transform_buffer = rtBufferCreate();
	rt_buffer static_transform_buffer = rtBufferCreate();
	const u64 transform_size = sizeof(TransformUniform);
	rtBufferData(moving_transform_buffer, RT_BUFFER_DYNAMIC, RT_BUFFER_USAGE_UNIFORM, transform_size, &moving_uniform);
	rtBufferData(static_transform_buffer, RT_BUFFER_DYNAMIC, RT_BUFFER_USAGE_UNIFORM, transform_size, &static_uniform);

	rt_texture texture = rtTextureCreate();
	rtTextureData(queue, texture, RT_TEXTURE_2D, 0, static_cast<u32>(texture_width),
		static_cast<u32>(texture_height), 1, RT_RGBA8_UNORM, texture_pixels);
	stbi_image_free(texture_pixels);

	rt_texture_view texture_view = rtTextureViewCreate(texture);
	rtTextureViewFilter(texture_view, RT_FILTER_LINEAR, RT_FILTER_LINEAR, RT_MIP_FILTER_NONE);
	rtTextureViewAddress(texture_view, RT_ADDRESS_CLAMP, RT_ADDRESS_CLAMP, RT_ADDRESS_CLAMP);
	rtTextureViewAnisotropy(texture_view, 1);

	rt_graphics_program graphics_program = rtGraphicsProgramCreate();
	rtGraphicsProgramVertexLayout(graphics_program, &kVertexLayout);
	rtGraphicsProgramVertexShader(graphics_program, std::strlen(kVertexShader), kVertexShader);
	rtGraphicsProgramFragmentShader(graphics_program, std::strlen(kFragmentShader), kFragmentShader);
	rtGraphicsProgramLink(graphics_program);
	rt_uniform_location transform_location = rtGraphicsProgramUniformLocation(graphics_program, "Transform");
	rt_uniform_location image_location = rtGraphicsProgramUniformLocation(graphics_program, "Image");

	rt_command_buffer cmd = rtCmdCreate();
	rt_texture depth_texture = RT_NULL_HANDLE;
	rt_texture_view depth_view = RT_NULL_HANDLE;
	u32 depth_width = state->framebuffer_width.load(std::memory_order_acquire);
	u32 depth_height = state->framebuffer_height.load(std::memory_order_acquire);
	recreate_depth_buffer(queue, &depth_texture, &depth_view, depth_width, depth_height);
	auto start_time = std::chrono::steady_clock::now();
	auto previous_time = start_time;
	rt_timepoint last_rendered = { RT_NULL_HANDLE, 0 };

	while (state->running.load(std::memory_order_acquire)) {
		auto now = std::chrono::steady_clock::now();
		const std::chrono::duration<f32> delta = now - previous_time;
		previous_time = now;
		update_camera(&camera, consume_input(state), delta.count());

		view_projection = camera_view_projection(camera_aspect(state), camera);
		moving_uniform = moving_transform(render_time_seconds(start_time), view_projection);
		static_uniform = static_transform(view_projection);
		rtBufferSubdata(moving_transform_buffer, 0, transform_size, &moving_uniform);
		rtBufferSubdata(static_transform_buffer, 0, transform_size, &static_uniform);
		const u32 current_width = state->framebuffer_width.load(std::memory_order_acquire);
		const u32 current_height = state->framebuffer_height.load(std::memory_order_acquire);
		const bool depth_size_changed = current_width != depth_width || current_height != depth_height;
		if (current_width && current_height && depth_size_changed) {
			rtTimepointWait(last_rendered);
			depth_width = current_width;
			depth_height = current_height;
			recreate_depth_buffer(queue, &depth_texture, &depth_view, depth_width, depth_height);
		}

		rt_swapchain_acquire_result acquired = rtSwapchainAcquire(swapchain);
		if (!acquired.framebuffer) {
			continue;
		}

		rtQueueWait(queue, acquired.timepoint);
		if (depth_view) { rtFramebufferDepthView(acquired.framebuffer, depth_view); }
		rtCmdBegin(cmd, queue);
		rtCmdBeginRendering(cmd, acquired.framebuffer);
		rtCmdClearColor(cmd, 0, 0.5f, 0.5f, 0.5f, 0.5f);
		if (depth_view) { rtCmdClearDepth(cmd, 1.0f); }

		rtCmdUseGraphicsProgram(cmd, graphics_program);
		rtCmdUniformTexture(cmd, image_location, texture_view);
		rtCmdBindVertexBuffer(cmd, vertex_buffer, 0);
		rtCmdUniformBuffer(cmd, transform_location, moving_transform_buffer, 0, transform_size);
		rtCmdDraw(cmd, kQuadVertexCount, kMovingQuadFirstVertex);
		rtCmdUniformBuffer(cmd, transform_location, static_transform_buffer, 0, transform_size);
		rtCmdDraw(cmd, kQuadVertexCount, kStaticQuadFirstVertex);

		rtCmdEndRendering(cmd);
		rtCmdEnd(cmd);

		rt_timepoint rendered = rtQueueSubmit(queue, cmd);
		last_rendered = rendered;
		if (depth_view) { rtFramebufferDepthView(acquired.framebuffer, RT_NULL_HANDLE); }
		rtSwapchainPresent(swapchain, rendered);
	
	}

	state->running.store(false, std::memory_order_release);
	rtTimepointWait(last_rendered);

	rtCmdDestroy(cmd);
	rtGraphicsProgramDestroy(graphics_program);
	rtTextureViewDestroy(depth_view);
	rtTextureDestroy(depth_texture);
	rtTextureViewDestroy(texture_view);
	rtTextureDestroy(texture);
	rtBufferDestroy(static_transform_buffer);
	rtBufferDestroy(moving_transform_buffer);
	rtBufferDestroy(vertex_buffer);
}


int main(int argc, char** argv) {
	const char* backend_name = argc > 1 ? argv[1] : kDefaultBackendName;
	if (rtLoad(backend_name, kLayers, 1) != RT_SUCCESS) {
		std::cerr << "rtLoad failed\n";
		return 1;
	}
	if (!rtLoad_RT_EXT_SWAPCHAIN() || !rtLoad_RT_EXT_GLFW()) {
		std::cerr << "required swapchain/GLFW extensions are not available\n";
		rtUnload();
		return 1;
	}
	std::cout << "backend: " << rtGetName() << "\n";
	rtInit(kFeatures, 1);
	if (rtError() != RT_SUCCESS) {
		std::cerr << "rtInit failed: " << rtErrorMessage() << "\n";
		rtUnload();
		return 1;
	}

	if (!glfwInit()) {
		std::cerr << "glfwInit failed\n";
		rtExit();
		rtUnload();
		return 1;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(1280, 720, "Rutile", nullptr, nullptr);
	if (!window) {
		std::cerr << "glfwCreateWindow failed\n";
		rtExit();
		rtUnload();
		glfwTerminate();
		return 1;
	}
	glfwSwapInterval(0);
	AppState state;
	int framebuffer_width = 0;
	int framebuffer_height = 0;
	glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
	if (framebuffer_width > 0 && framebuffer_height > 0) {
		state.framebuffer_width.store(static_cast<u32>(framebuffer_width), std::memory_order_release);
		state.framebuffer_height.store(static_cast<u32>(framebuffer_height), std::memory_order_release);
	}

	rt_swapchain swapchain = rtSwapchainCreate();
	rtSwapchainBindWindowGLFW(swapchain, window);
	state.swapchain.store(swapchain, std::memory_order_release);
	glfwSetWindowUserPointer(window, &state);
	glfwSetFramebufferSizeCallback(window, framebuffer_resized);
	glfwSetCursorPosCallback(window, cursor_moved);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	if (glfwRawMouseMotionSupported()) {
		glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
	}

	std::thread render_thread(render_thread_main, &state);
	while (!glfwWindowShouldClose(window) && state.running.load(std::memory_order_acquire)) {
		update_camera_input(window, &state);
		glfwWaitEventsTimeout(1.0 / 120.0);
	}
	state.running.store(false, std::memory_order_release);
	render_thread.join();

	state.swapchain.store(RT_NULL_HANDLE, std::memory_order_release);
	rtSwapchainDestroy(swapchain);
	rtExit();
	rtUnload();

	glfwDestroyWindow(window);
	glfwTerminate();
	return state.render_failed.load(std::memory_order_acquire) ? 1 : 0;
}
