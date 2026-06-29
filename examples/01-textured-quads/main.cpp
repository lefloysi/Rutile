#define RUTILE_IMPL
#include "rt_ext_glfw.h"
#include "rt_ext_swapchain.h"
#include "rutile.h"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <mutex>
#include <stb_image.h>
#include <thread>
#include <vector>
#include <windows.h>

#include "rtsl_embed.hpp"

constexpr const char* kDefaultBackendName = "rt-vk13";
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

std::atomic<rt_swapchain> Swapchain = RT_NULL_HANDLE;
std::atomic<u32> FramebufferWidth = 1280;
std::atomic<u32> FramebufferHeight = 720;
std::mutex InputMutex;
bool MoveForward = false;
bool MoveBackward = false;
bool MoveLeft = false;
bool MoveRight = false;
bool MoveUp = false;
bool MoveDown = false;
f32 MouseDx = 0.0f;
f32 MouseDy = 0.0f;
std::atomic<bool> Running = true;
std::atomic<bool> RenderFailed = false;

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
	{ "position", offsetof(Vertex, position), RT_RGB32_SFLOAT },
	{ "color", offsetof(Vertex, color), RT_RGB32_SFLOAT },
	{ "uv", offsetof(Vertex, uv), RT_RG32_SFLOAT },
};

constexpr rt_vertex_layout kVertexLayout = {
	sizeof(Vertex),
	kVertexAttributes,
	3,
};

constexpr const char* kTexturePath = "rutile.png";

void framebuffer_resized(GLFWwindow* window, int width, int height) {
	if (width > 0 && height > 0) {
		FramebufferWidth.store(static_cast<u32>(width), std::memory_order_release);
		FramebufferHeight.store(static_cast<u32>(height), std::memory_order_release);
	}

	rt_swapchain swapchain = Swapchain.load(std::memory_order_acquire);
	if (!swapchain || width <= 0 || height <= 0) {
		return;
	}

	rtSwapchainResize(swapchain, static_cast<u32>(width), static_cast<u32>(height));
}

void cursor_moved(GLFWwindow* window, double x, double y) {
	static double previous_x = x;
	static double previous_y = y;
	const f32 dx = static_cast<f32>(x - previous_x);
	const f32 dy = static_cast<f32>(y - previous_y);
	previous_x = x;
	previous_y = y;

	std::lock_guard<std::mutex> lock(InputMutex);
	MouseDx += dx;
	MouseDy += dy;
}

bool key_down(GLFWwindow* window, int key) {
	return glfwGetKey(window, key) == GLFW_PRESS;
}

void update_camera_input(GLFWwindow* window) {
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	}

	std::lock_guard<std::mutex> lock(InputMutex);
	MoveForward = key_down(window, GLFW_KEY_W);
	MoveBackward = key_down(window, GLFW_KEY_S);
	MoveLeft = key_down(window, GLFW_KEY_A);
	MoveRight = key_down(window, GLFW_KEY_D);
	MoveUp = key_down(window, GLFW_KEY_E) || key_down(window, GLFW_KEY_SPACE);
	MoveDown = key_down(window, GLFW_KEY_Q) || key_down(window, GLFW_KEY_LEFT_CONTROL);
}

glm::vec3 camera_forward(const CameraState& camera) {
	const f32 cp = glm::cos(camera.pitch);
	return glm::normalize(glm::vec3(glm::cos(camera.yaw) * cp, glm::sin(camera.pitch), glm::sin(camera.yaw) * cp));
}

void update_camera(CameraState* camera, f32 delta_seconds) {
	const f32 move_speed = 2.2f;
	const f32 mouse_sensitivity = 0.0022f;

	bool move_forward = false;
	bool move_backward = false;
	bool move_left = false;
	bool move_right = false;
	bool move_up = false;
	bool move_down = false;
	f32 mouse_dx = 0.0f;
	f32 mouse_dy = 0.0f;
	{
		std::lock_guard<std::mutex> lock(InputMutex);
		move_forward = MoveForward;
		move_backward = MoveBackward;
		move_left = MoveLeft;
		move_right = MoveRight;
		move_up = MoveUp;
		move_down = MoveDown;
		mouse_dx = MouseDx;
		mouse_dy = MouseDy;
		MouseDx = 0.0f;
		MouseDy = 0.0f;
	}

	camera->yaw += mouse_dx * mouse_sensitivity;
	camera->pitch -= mouse_dy * mouse_sensitivity;
	camera->pitch = glm::clamp(camera->pitch, glm::radians(-82.0f), glm::radians(82.0f));

	const glm::vec3 forward = camera_forward(*camera);
	const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
	glm::vec3 motion = glm::vec3(0.0f);
	if (move_forward) {
		motion += forward;
	}
	if (move_backward) {
		motion -= forward;
	}
	if (move_right) {
		motion += right;
	}
	if (move_left) {
		motion -= right;
	}
	if (move_up) {
		motion.y += 1.0f;
	}
	if (move_down) {
		motion.y -= 1.0f;
	}
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

f32 camera_aspect(void) {
	const u32 width = FramebufferWidth.load(std::memory_order_acquire);
	const u32 height = FramebufferHeight.load(std::memory_order_acquire);
	return height ? (f32)width / (f32)height : 1.0f;
}

double render_time_seconds(std::chrono::steady_clock::time_point start_time) {
	using seconds = std::chrono::duration<double>;
	return seconds(std::chrono::steady_clock::now() - start_time).count();
}

void render_thread_main(const std::filesystem::path& asset_dir) {
	rt_swapchain swapchain = Swapchain.load(std::memory_order_acquire);
	rt_queue queue = rtQueueQuery(RT_QUEUE_GRAPHICS);

	int texture_width = 0;
	int texture_height = 0;
	const std::filesystem::path texture_path = asset_dir / kTexturePath;
	stbi_uc* texture_pixels = stbi_load(texture_path.string().c_str(), &texture_width, &texture_height, nullptr, 4);
	if (!texture_pixels) {
		std::cerr << "failed to load " << texture_path.string() << "\n";
		RenderFailed.store(true, std::memory_order_release);
		Running.store(false, std::memory_order_release);
		return;
	}

	rt_buffer vertex_buffer = rtBufferCreate();
	rtBufferData(vertex_buffer, RT_BUFFER_STATIC, RT_BUFFER_USAGE_VERTEX, sizeof(kVertices), kVertices);

	CameraState camera;
	glm::mat4 view_projection = camera_view_projection(camera_aspect(), camera);
	TransformUniform moving_uniform = moving_transform(0.0, view_projection);
	TransformUniform static_uniform = static_transform(view_projection);
	rt_buffer moving_transform_buffer = rtBufferCreate();
	rt_buffer static_transform_buffer = rtBufferCreate();
	const u64 transform_size = sizeof(TransformUniform);
	rtBufferData(moving_transform_buffer, RT_BUFFER_DYNAMIC, RT_BUFFER_USAGE_UNIFORM, transform_size, &moving_uniform);
	rtBufferData(static_transform_buffer, RT_BUFFER_DYNAMIC, RT_BUFFER_USAGE_UNIFORM, transform_size, &static_uniform);

	rt_texture texture = rtTextureCreate();
	rtTextureData(texture, RT_TEXTURE_2D, 0, static_cast<u32>(texture_width), static_cast<u32>(texture_height), 1, RT_RGBA8_UNORM, texture_pixels);
	stbi_image_free(texture_pixels);

	rt_texture_view texture_view = rtTextureViewCreate();
	rtTextureViewBind(texture_view, texture);
	rtTextureViewFilter(texture_view, RT_FILTER_LINEAR, RT_FILTER_LINEAR, RT_MIP_FILTER_NONE);
	rtTextureViewAddress(texture_view, RT_ADDRESS_CLAMP, RT_ADDRESS_CLAMP, RT_ADDRESS_CLAMP);
	rtTextureViewAnisotropy(texture_view, 1);

	const std::vector<char> shader_program(
		reinterpret_cast<const char*>(rutile_01_textured_quads::textured_quads_rtslp),
		reinterpret_cast<const char*>(rutile_01_textured_quads::textured_quads_rtslp) + rutile_01_textured_quads::textured_quads_rtslp_size);

	rt_graphics_program graphics_program = rtGraphicsProgramCreate();
	rtGraphicsProgramSource(graphics_program, shader_program.size(), shader_program.data());
	rtGraphicsProgramLayout(graphics_program, &kVertexLayout);
	rtGraphicsProgramFinalize(graphics_program);
	if (rtError() != RT_SUCCESS) {
		std::cerr << "rtGraphicsProgramFinalize failed: " << rtErrorMessage() << "\n";
		RenderFailed.store(true, std::memory_order_release);
		Running.store(false, std::memory_order_release);
		rtGraphicsProgramDestroy(graphics_program);
		return;
	}
	rt_uniform_location transform_location = rtGraphicsProgramUniformLocation(graphics_program, "transform");
	rt_uniform_location image_location = rtGraphicsProgramUniformLocation(graphics_program, "texture");

	rt_command_buffer cmd = rtCommandBufferCreate();
	rt_texture depth_texture = rtTextureCreate();
	rtTextureData(depth_texture, RT_TEXTURE_2D, 0, FramebufferWidth.load(std::memory_order_acquire), FramebufferHeight.load(std::memory_order_acquire), 1, RT_D32_SFLOAT, NULL);
	rt_texture_view depth_view = rtTextureViewCreate();
	rtTextureViewBind(depth_view, depth_texture);
	u32 depth_width = FramebufferWidth.load(std::memory_order_acquire);
	u32 depth_height = FramebufferHeight.load(std::memory_order_acquire);
	auto start_time = std::chrono::steady_clock::now();
	auto previous_time = start_time;
	rt_timepoint last_rendered = { RT_NULL_HANDLE, 0 };

	while (Running.load(std::memory_order_acquire)) {
		auto now = std::chrono::steady_clock::now();
		const std::chrono::duration<f32> delta = now - previous_time;
		previous_time = now;
		update_camera(&camera, delta.count());

		view_projection = camera_view_projection(camera_aspect(), camera);
		moving_uniform = moving_transform(render_time_seconds(start_time), view_projection);
		static_uniform = static_transform(view_projection);
		rtBufferSubdata(moving_transform_buffer, 0, transform_size, &moving_uniform);
		rtBufferSubdata(static_transform_buffer, 0, transform_size, &static_uniform);
		const u32 current_width = FramebufferWidth.load(std::memory_order_acquire);
		const u32 current_height = FramebufferHeight.load(std::memory_order_acquire);
		const bool depth_size_changed = current_width != depth_width || current_height != depth_height;
		if (current_width && current_height && depth_size_changed) {
			rtTimepointWait(last_rendered);
			depth_width = current_width;
			depth_height = current_height;
			rtTextureData(depth_texture, RT_TEXTURE_2D, 0, depth_width, depth_height, 1, RT_D32_SFLOAT, NULL);
		}

		rt_swapchain_acquire_result acquired = rtSwapchainAcquire(swapchain);
		if (!acquired.framebuffer) {
			continue;
		}

		rtQueueWait(queue, acquired.timepoint);
		rtFramebufferDepthView(acquired.framebuffer, depth_view);
		rtCmdBegin(cmd, queue);
		rtCmdBeginRendering(cmd, acquired.framebuffer);
		rtCmdClearColor(cmd, 0, 0.5f, 0.5f, 0.5f, 0.5f);
		rtCmdClearDepth(cmd, 1.0f);

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
		rtFramebufferDepthView(acquired.framebuffer, RT_NULL_HANDLE);
		rtSwapchainPresent(swapchain, rendered);
	}

	Running.store(false, std::memory_order_release);
	rtTimepointWait(last_rendered);

	rtCommandBufferDestroy(cmd);
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
	const std::filesystem::path asset_dir = executable_dir(argc > 0 ? argv[0] : nullptr);
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
	GLFWwindow* window = glfwCreateWindow(1280, 720, "Rutile 01 Textured Quads", nullptr, nullptr);
	if (!window) {
		std::cerr << "glfwCreateWindow failed\n";
		rtExit();
		rtUnload();
		glfwTerminate();
		return 1;
	}
	glfwSwapInterval(0);
	int framebuffer_width = 0;
	int framebuffer_height = 0;
	glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
	if (framebuffer_width > 0 && framebuffer_height > 0) {
		FramebufferWidth.store(static_cast<u32>(framebuffer_width), std::memory_order_release);
		FramebufferHeight.store(static_cast<u32>(framebuffer_height), std::memory_order_release);
	}

	rt_swapchain swapchain = rtSwapchainCreate();
	rtSwapchainBindWindowGLFW(swapchain, window);
	if (rtError() != RT_SUCCESS) {
		std::cerr << "rtSwapchainBindWindowGLFW failed: " << rtErrorMessage() << "\n";
		Running.store(false, std::memory_order_release);
		rtSwapchainDestroy(swapchain);
		rtExit();
		rtUnload();
		glfwDestroyWindow(window);
		glfwTerminate();
		return 1;
	}
	Swapchain.store(swapchain, std::memory_order_release);
	glfwSetFramebufferSizeCallback(window, framebuffer_resized);
	glfwSetCursorPosCallback(window, cursor_moved);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	if (glfwRawMouseMotionSupported()) {
		glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
	}

	std::thread render_thread(render_thread_main, asset_dir);
	while (!glfwWindowShouldClose(window) && Running.load(std::memory_order_acquire)) {
		update_camera_input(window);
		glfwWaitEventsTimeout(1.0 / 120.0);
	}
	Running.store(false, std::memory_order_release);
	render_thread.join();

	Swapchain.store(RT_NULL_HANDLE, std::memory_order_release);
	rtSwapchainDestroy(swapchain);
	rtExit();
	rtUnload();

	glfwDestroyWindow(window);
	glfwTerminate();
	return RenderFailed.load(std::memory_order_acquire) ? 1 : 0;
}
