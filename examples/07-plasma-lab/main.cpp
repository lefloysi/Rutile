#define RUTILE_IMPL
#include "rt_ext_compute.h"
#include "rt_ext_glfw.h"
#include "rt_ext_swapchain.h"
#include "rutile.h"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>

constexpr const char* kDefaultBackendName = "rt-vulkan";
constexpr const char* kFeatures[] = {RT_FEATURE_PRESENTATION};
constexpr u32 kTextureWidth = 1024;
constexpr u32 kTextureHeight = 1024;

std::atomic<rt_swapchain> Swapchain = RT_NULL_HANDLE;
std::atomic<u32> FramebufferWidth = 1280;
std::atomic<u32> FramebufferHeight = 720;
f32 MouseX = 0.5f;
f32 MouseY = 0.5f;
bool MouseDown = false;

struct PlasmaParams {
	f32 mouse[4];
	f32 texture[4];
	f32 controls[4];
};

constexpr const char* kComputeShader = R"(
#version 460
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(set = 0, binding = 0, rgba8) uniform writeonly image2D PlasmaImage;

layout(set = 0, binding = 2) readonly buffer Params {
	vec4 mouse;
	vec4 texture_size_time;
	vec4 controls;
} params;

float hash(vec2 p) {
	p = fract(p * vec2(123.34, 456.21));
	p += dot(p, p + 45.32);
	return fract(p.x * p.y);
}

float filament(vec2 p, float t, float seed) {
	vec2 q = p;
	q.x += sin(q.y * 9.0 + t * (0.7 + seed)) * 0.08;
	q.y += cos(q.x * 7.0 - t * (0.9 + seed * 0.4)) * 0.08;
	float ridge = abs(sin(q.x * 15.0 + sin(q.y * 8.0 + t) * 2.2));
	ridge *= abs(cos(q.y * 13.0 - cos(q.x * 5.0 - t) * 2.0));
	return pow(1.0 - ridge, 3.8);
}

void main() {
	ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
	ivec2 size = imageSize(PlasmaImage);
	if (pixel.x >= size.x || pixel.y >= size.y) {
		return;
	}

	vec2 uv = (vec2(pixel) + vec2(0.5)) / vec2(size);
	vec2 aspect_uv = (uv - 0.5) * vec2(float(size.x) / float(size.y), 1.0);
	float t = params.texture_size_time.z;
	float pulse = params.controls.x;
	float mouse_force = params.controls.y;
	vec2 mouse = params.mouse.xy;
	mouse.y = 1.0 - mouse.y;
	vec2 mouse_uv = (mouse - 0.5) * vec2(float(size.x) / float(size.y), 1.0);

	float d = length(aspect_uv);
	float m = length(aspect_uv - mouse_uv);
	float swirl = atan(aspect_uv.y, aspect_uv.x) + sin(d * 12.0 - t * 0.85) * 0.45;
	float nebula = 0.0;
	nebula += filament(aspect_uv * 1.4 + vec2(sin(t * 0.21), cos(t * 0.18)) * 0.16, t, 0.2);
	nebula += filament(vec2(cos(swirl), sin(swirl)) * d * 1.8 + aspect_uv * 0.55, t * 1.2, 0.7) * 0.85;
	nebula += smoothstep(0.62, 0.04, d) * 0.55;
	nebula += smoothstep(0.30, 0.0, m) * mouse_force * (0.7 + 0.3 * sin(t * 7.0));

	float scan = sin((uv.y + sin(uv.x * 11.0 + t * 0.7) * 0.01) * 900.0) * 0.012;
	float grain = hash(vec2(pixel) + floor(t * 24.0)) * 0.035;
	float energy = clamp(nebula * pulse + grain + scan, 0.0, 1.0);

	vec3 deep = vec3(0.015, 0.025, 0.065);
	vec3 blue = vec3(0.08, 0.36, 0.88);
	vec3 teal = vec3(0.10, 0.82, 0.76);
	vec3 ember = vec3(1.0, 0.43, 0.16);
	vec3 white = vec3(1.0, 0.92, 0.72);
	vec3 color = mix(deep, blue, smoothstep(0.05, 0.42, energy));
	color = mix(color, teal, smoothstep(0.25, 0.70, energy) * 0.55);
	color = mix(color, ember, smoothstep(0.52, 0.88, energy) * (0.35 + mouse_force * 0.25));
	color = mix(color, white, smoothstep(0.78, 1.0, energy));
	color += vec3(0.015, 0.02, 0.035) * pow(1.0 - d, 2.0);

	imageStore(PlasmaImage, pixel, vec4(color, 1.0));
}
)";

constexpr const char* kVertexShader = R"(
#version 460
layout(location = 0) out vec2 uv;

void main() {
	vec2 positions[3] = vec2[](
		vec2(-1.0, -1.0),
		vec2(3.0, -1.0),
		vec2(-1.0, 3.0)
	);
	vec2 position = positions[gl_VertexIndex];
	uv = position * 0.5 + 0.5;
	gl_Position = vec4(position, 0.0, 1.0);
}
)";

constexpr const char* kFragmentShader = R"(
#version 460
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D Plasma;

void main() {
	vec3 color = texture(Plasma, uv).rgb;
	float vignette = smoothstep(1.15, 0.24, length(uv - 0.5));
	color *= 0.72 + vignette * 0.42;
	out_color = vec4(color, 1.0);
}
)";

bool check_rt_error(const char* step) {
	if (rtError() == RT_SUCCESS) {
		return true;
	}
	std::cerr << step << " failed: " << rtErrorMessage() << "\n";
	rtClearError();
	return false;
}

u32 ceil_div_u32(u32 value, u32 divisor) {
	return (value + divisor - 1u) / divisor;
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
	const u32 width = max(FramebufferWidth.load(std::memory_order_acquire), 1u);
	const u32 height = max(FramebufferHeight.load(std::memory_order_acquire), 1u);
	MouseX = std::clamp((f32)x / (f32)width, 0.0f, 1.0f);
	MouseY = std::clamp((f32)y / (f32)height, 0.0f, 1.0f);
}

void mouse_button(GLFWwindow* window, int button, int action, int) {
	(void)window;
	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		MouseDown = action == GLFW_PRESS;
	}
}

void write_params(PlasmaParams* params, f32 time) {
	params->mouse[0] = MouseX;
	params->mouse[1] = MouseY;
	params->mouse[2] = MouseDown ? 1.0f : 0.0f;
	params->mouse[3] = 0.0f;
	params->texture[0] = (f32)kTextureWidth;
	params->texture[1] = (f32)kTextureHeight;
	params->texture[2] = time;
	params->texture[3] = 0.0f;
	params->controls[0] = 0.92f + 0.20f * std::sin(time * 0.73f);
	params->controls[1] = MouseDown ? 1.0f : 0.35f;
	params->controls[2] = 0.0f;
	params->controls[3] = 0.0f;
}

int main(int argc, char** argv) {
	const char* backend_name = argc > 1 ? argv[1] : kDefaultBackendName;
	if (rtLoad(backend_name, nullptr, 0) != RT_SUCCESS) {
		std::cerr << "rtLoad failed: " << rtErrorMessage() << "\n";
		return 1;
	}
	if (!rtLoad_RT_EXT_COMPUTE() || !rtLoad_RT_EXT_SWAPCHAIN() || !rtLoad_RT_EXT_GLFW()) {
		std::cerr << "required compute/swapchain/GLFW extensions are not available\n";
		rtUnload();
		return 1;
	}

	std::cout << "backend: " << rtGetName() << "\n";
	rtInit(kFeatures, 1);
	if (!check_rt_error("rtInit")) {
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
	GLFWwindow* window = glfwCreateWindow(1280, 720, "Rutile 07 Plasma Lab", nullptr, nullptr);
	if (!window) {
		std::cerr << "glfwCreateWindow failed\n";
		glfwTerminate();
		rtExit();
		rtUnload();
		return 1;
	}

	glfwSetFramebufferSizeCallback(window, framebuffer_resized);
	glfwSetCursorPosCallback(window, cursor_moved);
	glfwSetMouseButtonCallback(window, mouse_button);

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
	if (!queue) {
		std::cerr << "no graphics queue is available\n";
		Swapchain.store(RT_NULL_HANDLE, std::memory_order_release);
		rtSwapchainDestroy(swapchain);
		glfwDestroyWindow(window);
		glfwTerminate();
		rtExit();
		rtUnload();
		return 1;
	}

	rt_texture plasma_texture = rtTextureCreate();
	rtTextureData(queue, plasma_texture, RT_TEXTURE_2D, 0, kTextureWidth, kTextureHeight, 1, RT_RGBA8_UNORM, nullptr);
	rt_texture_view plasma_view = rtTextureViewCreate(plasma_texture);
	rtTextureViewFilter(plasma_view, RT_FILTER_LINEAR, RT_FILTER_LINEAR, RT_MIP_FILTER_NONE);
	rtTextureViewAddress(plasma_view, RT_ADDRESS_CLAMP, RT_ADDRESS_CLAMP, RT_ADDRESS_CLAMP);

	PlasmaParams params = {};
	rt_buffer params_buffer = rtBufferCreate();
	rtBufferData(params_buffer, RT_BUFFER_DYNAMIC, RT_BUFFER_USAGE_STORAGE, sizeof(params), &params);

	rt_compute_program compute_program = rtComputeProgramCreate();
	rtComputeProgramShader(compute_program, std::strlen(kComputeShader), kComputeShader);
	rtComputeProgramLink(compute_program);
	if (!check_rt_error("link compute program")) {
		rtComputeProgramDestroy(compute_program);
		rtBufferDestroy(params_buffer);
		rtTextureViewDestroy(plasma_view);
		rtTextureDestroy(plasma_texture);
		Swapchain.store(RT_NULL_HANDLE, std::memory_order_release);
		rtSwapchainDestroy(swapchain);
		glfwDestroyWindow(window);
		glfwTerminate();
		rtExit();
		rtUnload();
		return 1;
	}

	rt_graphics_program graphics_program = rtGraphicsProgramCreate();
	rtGraphicsProgramVertexLayout(graphics_program, nullptr);
	rtGraphicsProgramVertexShader(graphics_program, std::strlen(kVertexShader), kVertexShader);
	rtGraphicsProgramFragmentShader(graphics_program, std::strlen(kFragmentShader), kFragmentShader);
	rtGraphicsProgramLink(graphics_program);
	rt_uniform_location plasma_location = rtGraphicsProgramUniformLocation(graphics_program, "Plasma");
	if (!check_rt_error("link graphics program")) {
		rtGraphicsProgramDestroy(graphics_program);
		rtComputeProgramDestroy(compute_program);
		rtBufferDestroy(params_buffer);
		rtTextureViewDestroy(plasma_view);
		rtTextureDestroy(plasma_texture);
		Swapchain.store(RT_NULL_HANDLE, std::memory_order_release);
		rtSwapchainDestroy(swapchain);
		glfwDestroyWindow(window);
		glfwTerminate();
		rtExit();
		rtUnload();
		return 1;
	}

	rt_command_buffer cmd = rtCommandBufferCreate();
	auto start_time = std::chrono::steady_clock::now();
	auto fps_time = start_time;
	u32 fps_frames = 0;
	rt_timepoint last_rendered = {RT_NULL_HANDLE, 0};

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
			glfwSetWindowShouldClose(window, GLFW_TRUE);
		}

		const auto now = std::chrono::steady_clock::now();
		const std::chrono::duration<f32> elapsed = now - start_time;
		write_params(&params, elapsed.count());
		rtBufferSubdata(params_buffer, 0, sizeof(params), &params);

		rt_swapchain_acquire_result acquired = rtSwapchainAcquire(swapchain);
		if (!acquired.framebuffer) {
			continue;
		}

		rtQueueWait(queue, acquired.timepoint);
		rtCmdBegin(cmd, queue);
		rtCmdUseComputeProgram(cmd, compute_program);
		rtCmdStorageTexture(cmd, 0, plasma_view);
		rtCmdStorageBuffer(cmd, 2, params_buffer, 0, sizeof(params));
		rtCmdDispatch(cmd, ceil_div_u32(kTextureWidth, 16u), ceil_div_u32(kTextureHeight, 16u), 1);
		rtCmdComputeBarrier(cmd);
		rtCmdBeginRendering(cmd, acquired.framebuffer);
		rtCmdClearColor(cmd, 0, 0.0f, 0.0f, 0.0f, 1.0f);
		rtCmdUseGraphicsProgram(cmd, graphics_program);
		rtCmdUniformTexture(cmd, plasma_location, plasma_view);
		rtCmdDraw(cmd, 3, 0);
		rtCmdEndRendering(cmd);
		rtCmdEnd(cmd);

		rt_timepoint rendered = rtQueueSubmit(queue, cmd);
		last_rendered = rendered;
		rtSwapchainPresent(swapchain, rendered);

		fps_frames++;
		const std::chrono::duration<f32> fps_delta = std::chrono::steady_clock::now() - fps_time;
		if (fps_delta.count() >= 0.5f) {
			char title[128];
			const f32 fps = (f32)fps_frames / fps_delta.count();
			std::snprintf(title, sizeof(title), "Rutile 07 Plasma Lab - %.0f FPS - drag to excite the field", fps);
			glfwSetWindowTitle(window, title);
			fps_time = std::chrono::steady_clock::now();
			fps_frames = 0;
		}
	}

	rtTimepointWait(last_rendered);
	rtCommandBufferDestroy(cmd);
	rtGraphicsProgramDestroy(graphics_program);
	rtComputeProgramDestroy(compute_program);
	rtBufferDestroy(params_buffer);
	rtTextureViewDestroy(plasma_view);
	rtTextureDestroy(plasma_texture);
	Swapchain.store(RT_NULL_HANDLE, std::memory_order_release);
	rtSwapchainDestroy(swapchain);
	glfwDestroyWindow(window);
	glfwTerminate();
	rtExit();
	rtUnload();
	return 0;
}
