#define RUTILE_IMPL
#include "cli.hpp"
#include "rt_ext_glfw.h"
#include "rt_ext_swapchain.h"
#include "rutile.h"

#include <GLFW/glfw3.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <rtsl/sdk/program.hpp>
#include <random>
#include <vector>

extern "C" const rtsl::ProgramBytes galaxy_rtslp;

constexpr const char* kFeatures[] = { RT_FEATURE_PRESENTATION };
constexpr u32 kStarCount = 26000;
constexpr u32 kPlanetCount = 520;
constexpr f32 kGalaxyRadius = 880.0f;

struct Vertex {
	f32 position[3];
	f32 color[4];
	f32 normal[3];
	f32 kind;
	f32 seed;
};

struct SceneUniform {
	f32 view_projection[16];
	f32 light_dir[4];
	f32 time;
	f32 padding[3];
};

struct Camera {
	glm::vec3 position = glm::vec3(0.0f, 96.0f, 300.0f);
	f32 yaw = -glm::radians(90.0f);
	f32 pitch = -0.23f;
};

std::atomic<rt_swapchain> Swapchain = RT_NULL_HANDLE;
std::atomic<u32> FramebufferWidth = 1600;
std::atomic<u32> FramebufferHeight = 900;
f32 MouseDx = 0.0f;
f32 MouseDy = 0.0f;

constexpr rt_vertex_attribute kVertexAttributes[] = {
	{ "position", offsetof(Vertex, position), RT_RGB32_SFLOAT },
	{ "color", offsetof(Vertex, color), RT_RGBA32_SFLOAT },
	{ "normal", offsetof(Vertex, normal), RT_RGB32_SFLOAT },
	{ "kind", offsetof(Vertex, kind), RT_R32_SFLOAT },
	{ "seed", offsetof(Vertex, seed), RT_R32_SFLOAT },
};

constexpr rt_vertex_layout kVertexLayout = { sizeof(Vertex), kVertexAttributes, 5 };

glm::vec3 camera_forward(const Camera& camera) {
	const f32 cp = glm::cos(camera.pitch);
	return glm::normalize(glm::vec3(glm::cos(camera.yaw) * cp, glm::sin(camera.pitch), glm::sin(camera.yaw) * cp));
}

void push_vertex(
	std::vector<Vertex>* vertices,
	const glm::vec3& position,
	const glm::vec4& color,
	const glm::vec3& normal,
	f32 kind,
	f32 seed
) {
	vertices->push_back({
		{ position.x, position.y, position.z },
		{ color.r, color.g, color.b, color.a },
		{ normal.x, normal.y, normal.z },
		kind,
		seed,
	});
}

void push_star(std::vector<Vertex>* vertices, const glm::vec3& center, f32 radius, const glm::vec4& color, f32 seed) {
	const glm::vec3 p[] = {
		glm::vec3(0.0f, radius, 0.0f),
		glm::vec3(radius, 0.0f, 0.0f),
		glm::vec3(0.0f, 0.0f, radius),
		glm::vec3(-radius, 0.0f, 0.0f),
		glm::vec3(0.0f, 0.0f, -radius),
		glm::vec3(0.0f, -radius, 0.0f),
	};
	const i32 faces[][3] = {
		{ 0, 1, 2 },
		{ 0, 2, 3 },
		{ 0, 3, 4 },
		{ 0, 4, 1 },
		{ 5, 2, 1 },
		{ 5, 3, 2 },
		{ 5, 4, 3 },
		{ 5, 1, 4 },
	};
	for (const auto& face : faces) {
		const glm::vec3 a = p[face[0]];
		const glm::vec3 b = p[face[1]];
		const glm::vec3 c = p[face[2]];
		const glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
		push_vertex(vertices, center + a, color, n, 0.0f, seed);
		push_vertex(vertices, center + b, color, n, 0.0f, seed);
		push_vertex(vertices, center + c, color, n, 0.0f, seed);
	}
}

void push_planet(std::vector<Vertex>* vertices, const glm::vec3& center, f32 radius, const glm::vec4& color, f32 seed) {
	constexpr i32 stacks = 8;
	constexpr i32 slices = 12;
	for (i32 y = 0; y < stacks; y++) {
		const f32 v0 = (f32)y / (f32)stacks;
		const f32 v1 = (f32)(y + 1) / (f32)stacks;
		const f32 phi0 = v0 * glm::pi<f32>();
		const f32 phi1 = v1 * glm::pi<f32>();
		for (i32 x = 0; x < slices; x++) {
			const f32 u0 = (f32)x / (f32)slices;
			const f32 u1 = (f32)(x + 1) / (f32)slices;
			const f32 theta0 = u0 * glm::two_pi<f32>();
			const f32 theta1 = u1 * glm::two_pi<f32>();
			const glm::vec3 n00(glm::sin(phi0) * glm::cos(theta0), glm::cos(phi0), glm::sin(phi0) * glm::sin(theta0));
			const glm::vec3 n10(glm::sin(phi0) * glm::cos(theta1), glm::cos(phi0), glm::sin(phi0) * glm::sin(theta1));
			const glm::vec3 n11(glm::sin(phi1) * glm::cos(theta1), glm::cos(phi1), glm::sin(phi1) * glm::sin(theta1));
			const glm::vec3 n01(glm::sin(phi1) * glm::cos(theta0), glm::cos(phi1), glm::sin(phi1) * glm::sin(theta0));
			push_vertex(vertices, center + n00 * radius, color, n00, 1.0f, seed);
			push_vertex(vertices, center + n11 * radius, color, n11, 1.0f, seed);
			push_vertex(vertices, center + n10 * radius, color, n10, 1.0f, seed);
			push_vertex(vertices, center + n00 * radius, color, n00, 1.0f, seed);
			push_vertex(vertices, center + n01 * radius, color, n01, 1.0f, seed);
			push_vertex(vertices, center + n11 * radius, color, n11, 1.0f, seed);
		}
	}
}

f32 rand_range(std::mt19937& rng, f32 min_value, f32 max_value) {
	std::uniform_real_distribution<f32> dist(min_value, max_value);
	return dist(rng);
}

glm::vec3 star_color(f32 heat) {
	const glm::vec3 cool(0.58f, 0.73f, 1.0f);
	const glm::vec3 white(1.0f, 0.98f, 0.88f);
	const glm::vec3 warm(1.0f, 0.56f, 0.32f);
	return heat < 0.58f ? glm::mix(cool, white, heat / 0.58f) : glm::mix(white, warm, (heat - 0.58f) / 0.42f);
}

glm::vec3 spiral_position(std::mt19937& rng, f32 radius_bias, f32 arm_jitter, f32 height) {
	const f32 radius = std::pow(rand_range(rng, 0.0f, 1.0f), radius_bias) * kGalaxyRadius;
	const i32 arm = (i32)rand_range(rng, 0.0f, 4.0f);
	const f32 arm_angle = (f32)arm * glm::half_pi<f32>();
	const f32 angle = arm_angle + radius * 0.018f + rand_range(rng, -arm_jitter, arm_jitter) * (1.0f + radius / kGalaxyRadius);
	const f32 flatten = 1.0f - radius / (kGalaxyRadius * 1.35f);
	const f32 y = rand_range(rng, -height, height) * glm::max(0.16f, flatten);
	return glm::vec3(glm::cos(angle) * radius, y, glm::sin(angle) * radius);
}

std::vector<Vertex> build_galaxy() {
	std::mt19937 rng(0x51A7E11u);
	std::vector<Vertex> vertices;
	vertices.reserve(kStarCount * 24 + kPlanetCount * 8 * 12 * 6);

	for (u32 i = 0; i < kStarCount; i++) {
		const bool core_star = rand_range(rng, 0.0f, 1.0f) < 0.16f;
		glm::vec3 center = core_star ? glm::vec3(rand_range(rng, -82.0f, 82.0f), rand_range(rng, -18.0f, 18.0f), rand_range(rng, -82.0f, 82.0f)) : spiral_position(rng, 0.48f, 0.34f, 44.0f);
		if (core_star) {
			const f32 pull = rand_range(rng, 0.0f, 1.0f);
			center *= pull * pull;
		}
		const f32 giant = rand_range(rng, 0.0f, 1.0f) < 0.02f ? rand_range(rng, 1.5f, 2.8f) : 1.0f;
		const f32 radius = rand_range(rng, 0.10f, 0.42f) * giant;
		const f32 alpha = core_star ? rand_range(rng, 0.66f, 1.0f) : rand_range(rng, 0.42f, 0.92f);
		push_star(&vertices, center, radius, glm::vec4(star_color(rand_range(rng, 0.0f, 1.0f)), alpha), rand_range(rng, 0.0f, 1.0f));
	}

	for (u32 i = 0; i < kPlanetCount; i++) {
		const glm::vec3 center = spiral_position(rng, 0.55f, 0.28f, 36.0f);
		const f32 palette = rand_range(rng, 0.0f, 1.0f);
		glm::vec3 color = glm::mix(glm::vec3(0.22f, 0.36f, 0.72f), glm::vec3(0.72f, 0.50f, 0.25f), palette);
		if (palette > 0.64f) {
			color = glm::mix(color, glm::vec3(0.70f, 0.82f, 0.68f), 0.30f);
		}
		const f32 radius = rand_range(rng, 0.34f, 1.45f);
		push_planet(&vertices, center, radius, glm::vec4(color, 1.0f), rand_range(rng, 0.0f, 1.0f));
	}

	return vertices;
}

glm::mat4 camera_matrix(const Camera& camera, f32 aspect) {
	const glm::vec3 forward = camera_forward(camera);
	const glm::mat4 view = glm::lookAt(camera.position, camera.position + forward, glm::vec3(0, 1, 0));
	const glm::mat4 projection = glm::perspective(glm::radians(68.0f), aspect, 0.08f, 3600.0f);
	return projection * view;
}

void write_scene_uniform(SceneUniform* uniform, const Camera& camera, f32 aspect, f32 time) {
	const glm::mat4 transform = camera_matrix(camera, aspect);
	const glm::vec3 light_dir = glm::normalize(glm::vec3(-0.42f, 0.58f, 0.70f));
	std::memcpy(uniform->view_projection, glm::value_ptr(transform), sizeof(uniform->view_projection));
	uniform->light_dir[0] = light_dir.x;
	uniform->light_dir[1] = light_dir.y;
	uniform->light_dir[2] = light_dir.z;
	uniform->light_dir[3] = 0.0f;
	uniform->time = time;
	uniform->padding[0] = 0.0f;
	uniform->padding[1] = 0.0f;
	uniform->padding[2] = 0.0f;
}

void recreate_depth_buffer(rt_queue queue, rt_texture* depth_texture, rt_texture_view* depth_view, u32 width, u32 height) {
	(void)queue;
	if (!*depth_texture) {
		*depth_texture = rtTextureCreate();
	}
	rtTextureData(*depth_texture, RT_TEXTURE_2D, 0, width, height, 1, RT_D32_SFLOAT, NULL);
	if (!*depth_view) {
		*depth_view = rtTextureViewCreate();
		rtTextureViewBind(*depth_view, *depth_texture);
	}
}

void framebuffer_resized(GLFWwindow* window, int width, int height) {
	(void)window;
	if (width > 0 && height > 0) {
		FramebufferWidth.store((u32)width, std::memory_order_release);
		FramebufferHeight.store((u32)height, std::memory_order_release);
	}
	rt_swapchain swapchain = Swapchain.load(std::memory_order_acquire);
	if (swapchain && width > 0 && height > 0) {
		rtSwapchainResize(swapchain, (u32)width, (u32)height);
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
	const f32 sensitivity = 0.0022f;
	camera->yaw += MouseDx * sensitivity;
	camera->pitch -= MouseDy * sensitivity;
	camera->pitch = glm::clamp(camera->pitch, -1.48f, 1.48f);
	MouseDx = 0.0f;
	MouseDy = 0.0f;

	const glm::vec3 forward = camera_forward(*camera);
	const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
	const f32 speed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 260.0f : 82.0f;
	glm::vec3 velocity(0.0f);
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
		velocity += forward;
	}
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
		velocity -= forward;
	}
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
		velocity += right;
	}
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
		velocity -= right;
	}
	if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
		velocity.y += 1.0f;
	}
	if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
		velocity.y -= 1.0f;
	}
	if (glm::dot(velocity, velocity) > 0.0f) {
		camera->position += glm::normalize(velocity) * speed * dt;
	}
}

int main(int argc, char** argv) {
	const ExampleOptions options = parse_cli(argc, argv);
	rtLoad(options.backend.c_str(), nullptr, 0);
	rtInit(kFeatures, 1);
	rtLoad_RT_EXT_SWAPCHAIN();
	rtLoad_RT_EXT_GLFW();
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(1600, 900, "Rutile 04 Galaxy", nullptr, nullptr);

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

	std::vector<Vertex> vertices = build_galaxy();
	rt_buffer vertex_buffer = rtBufferCreate();
	rtBufferData(vertex_buffer, RT_BUFFER_STATIC, RT_BUFFER_USAGE_VERTEX, (u64)(vertices.size() * sizeof(vertices[0])), vertices.data());

	SceneUniform scene = {};
	rt_buffer scene_buffer = rtBufferCreate();
	rtBufferData(scene_buffer, RT_BUFFER_DYNAMIC, RT_BUFFER_USAGE_UNIFORM, sizeof(scene), &scene);

	rt_graphics_program graphics_program = rtGraphicsProgramCreate();
	rtGraphicsProgramLayout(graphics_program, &kVertexLayout);
	rtGraphicsProgramSource(graphics_program, galaxy_rtslp.size, galaxy_rtslp.data);
	rtGraphicsProgramRasterState(graphics_program, RT_CULL_NONE, RT_FRONT_FACE_CCW, RT_FILL_SOLID);
	rtGraphicsProgramFinalize(graphics_program);
	rt_uniform_location scene_location = rtGraphicsProgramUniformLocation(graphics_program, "scene");

	rt_command_buffer cmd = rtCommandBufferCreate();
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
	u32 rendered_frames = 0;

	while (!glfwWindowShouldClose(window) && (!options.frames || rendered_frames < options.frames)) {
		glfwPollEvents();
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
			glfwSetWindowShouldClose(window, GLFW_TRUE);
		}

		auto now = std::chrono::steady_clock::now();
		const std::chrono::duration<f32> delta = now - previous_time;
		const std::chrono::duration<f32> elapsed = now - start_time;
		previous_time = now;
		update_camera(window, &camera, delta.count());

		const u32 current_width = FramebufferWidth.load(std::memory_order_acquire);
		const u32 current_height = FramebufferHeight.load(std::memory_order_acquire);
		const bool depth_size_changed = current_width != depth_width || current_height != depth_height;
		if (current_width && current_height && depth_size_changed) {
			rtTimepointWait(last_rendered);
			depth_width = current_width;
			depth_height = current_height;
			recreate_depth_buffer(queue, &depth_texture, &depth_view, depth_width, depth_height);
		}

		const f32 aspect = current_height ? (f32)current_width / (f32)current_height : 1.0f;
		write_scene_uniform(&scene, camera, aspect, elapsed.count());
		rtBufferSubdata(scene_buffer, 0, sizeof(scene), &scene);

		rt_swapchain_acquire_result acquired = rtSwapchainAcquire(swapchain);
		if (!acquired.framebuffer) {
			continue;
		}

		rtQueueWait(queue, acquired.timepoint);
		if (depth_view) {
			rtFramebufferDepthView(acquired.framebuffer, depth_view);
		}
		rtCmdBegin(cmd, queue);
		rtCmdBeginRendering(cmd, acquired.framebuffer);
		rtCmdClearColor(cmd, 0, 0.004f, 0.006f, 0.014f, 1.0f);
		if (depth_view) {
			rtCmdClearDepth(cmd, 1.0f);
		}
		rtCmdUseGraphicsProgram(cmd, graphics_program);
		rtCmdBindVertexBuffer(cmd, vertex_buffer, 0);
		rtCmdUniformBuffer(cmd, scene_location, scene_buffer, 0, sizeof(scene));
		rtCmdDraw(cmd, (u32)vertices.size(), 0);
		rtCmdEndRendering(cmd);
		rtCmdEnd(cmd);

		rt_timepoint rendered = rtQueueSubmit(queue, cmd);
		last_rendered = rendered;
		if (depth_view) {
			rtFramebufferDepthView(acquired.framebuffer, RT_NULL_HANDLE);
		}
		rtSwapchainPresent(swapchain, rendered);
		rendered_frames++;

		fps_frames++;
		const std::chrono::duration<f32> fps_delta = std::chrono::steady_clock::now() - fps_time;
		if (fps_delta.count() >= 0.5f) {
			char title[128];
			const f32 fps = (f32)fps_frames / fps_delta.count();
			std::snprintf(title, sizeof(title), "Rutile 04 Galaxy - %.0f FPS - %u mesh stars, %u planets", fps, kStarCount, kPlanetCount);
			glfwSetWindowTitle(window, title);
			fps_time = std::chrono::steady_clock::now();
			fps_frames = 0;
		}
	}

	rtTimepointWait(last_rendered);
	rtCommandBufferDestroy(cmd);
	rtGraphicsProgramDestroy(graphics_program);
	rtTextureViewDestroy(depth_view);
	rtTextureDestroy(depth_texture);
	rtBufferDestroy(scene_buffer);
	rtBufferDestroy(vertex_buffer);
	Swapchain.store(RT_NULL_HANDLE, std::memory_order_release);
	rtSwapchainDestroy(swapchain);
	glfwDestroyWindow(window);
	glfwTerminate();
	rtExit();
	rtUnload();
	return 0;
}
