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

constexpr const char* kFeatures[] = { RT_FEATURE_PRESENTATION };
constexpr u32 kTextureWidth = 8192;
constexpr u32 kTextureHeight = 8192;
constexpr u32 kWindowWidth = 1280;
constexpr u32 kWindowHeight = 720;
constexpr f32 kBrushRadius = 18.0f;
constexpr u64 kFixedHeatOne = 1ull << 32;
constexpr f32 kScrollZoomSpeed = 0.55f;
constexpr f32 kPanSpeed = 2200.0f;
constexpr f32 kFastPanSpeed = 6200.0f;

std::atomic<rt_swapchain> Swapchain = RT_NULL_HANDLE;
std::atomic<u32> FramebufferWidth = kWindowWidth;
std::atomic<u32> FramebufferHeight = kWindowHeight;
f32 CursorX = kWindowWidth * 0.5f;
f32 CursorY = kWindowHeight * 0.5f;
f32 PreviousCursorX = kWindowWidth * 0.5f;
f32 PreviousCursorY = kWindowHeight * 0.5f;
f32 ScrollY = 0.0f;
f32 BrushPreviousX = kTextureWidth * 0.5f;
f32 BrushPreviousY = kTextureHeight * 0.5f;
bool Drawing = false;
bool BrushHasPrevious = false;

struct SimParams {
	f32 texture[4];
	f32 brush_a[4];
	f32 brush_b[4];
	f32 camera[4];
};

struct Camera {
	f32 x = kTextureWidth * 0.5f;
	f32 y = kTextureHeight * 0.5f;
	f32 zoom = 1.0f;
};

constexpr const char* kInitShader = R"(
#version 460
#extension GL_ARB_gpu_shader_int64 : require
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(set = 0, binding = 0) buffer HeatOut {
	uint64_t heat_out[];
};

layout(set = 0, binding = 2) readonly buffer Params {
	vec4 texture_size_time;
	vec4 brush_a;
	vec4 brush_b;
	vec4 camera;
} params;

void main() {
	ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
	ivec2 size = ivec2(params.texture_size_time.xy);
	if (pixel.x >= size.x || pixel.y >= size.y) {
		return;
	}

	heat_out[pixel.y * size.x + pixel.x] = uint64_t(0);
}
)";

constexpr const char* kStepShader = R"(
#version 460
#extension GL_ARB_gpu_shader_int64 : require
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(set = 0, binding = 0) readonly buffer HeatIn {
	uint64_t heat_in[];
};
layout(set = 0, binding = 1) buffer HeatOut {
	uint64_t heat_out[];
};

layout(set = 0, binding = 2) readonly buffer Params {
	vec4 texture_size_time;
	vec4 brush_a;
	vec4 brush_b;
	vec4 camera;
} params;

const uint64_t HEAT_ONE = uint64_t(4294967296ul);

uint64_t load_heat(ivec2 pixel) {
	ivec2 size = ivec2(params.texture_size_time.xy);
	pixel = clamp(pixel, ivec2(0), size - ivec2(1));
	return heat_in[pixel.y * size.x + pixel.x];
}

uint64_t brush_heat(vec2 pixel) {
	if (params.brush_b.z <= 0.0) {
		return uint64_t(0);
	}

	vec2 a = params.brush_a.xy;
	vec2 b = params.brush_b.xy;
	vec2 ab = b - a;
	float t = dot(pixel - a, ab) / max(dot(ab, ab), 0.0001);
	vec2 nearest = mix(a, b, clamp(t, 0.0, 1.0));
	float distance_to_stroke = length(pixel - nearest);
	float heat = params.brush_a.w * smoothstep(params.brush_a.z, 0.0, distance_to_stroke);
	return uint64_t(clamp(heat, 0.0, 1.0) * float(HEAT_ONE));
}

uint64_t diffuse_toward(uint64_t center, uint64_t target) {
	if (target >= center) {
		return center + ((target - center) * uint64_t(21)) / uint64_t(64);
	}
	return center - ((center - target) * uint64_t(21)) / uint64_t(64);
}

void main() {
	ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
	ivec2 size = ivec2(params.texture_size_time.xy);
	if (pixel.x >= size.x || pixel.y >= size.y) {
		return;
	}

	uint64_t center = load_heat(pixel);
	uint64_t n = load_heat(pixel + ivec2(0, -1));
	uint64_t s = load_heat(pixel + ivec2(0, 1));
	uint64_t e = load_heat(pixel + ivec2(1, 0));
	uint64_t w = load_heat(pixel + ivec2(-1, 0));
	uint64_t diagonal = (
		load_heat(pixel + ivec2(-1, -1)) +
		load_heat(pixel + ivec2(1, -1)) +
		load_heat(pixel + ivec2(-1, 1)) +
		load_heat(pixel + ivec2(1, 1))) / uint64_t(4);

	uint64_t neighbor_average = (n + s + e + w + diagonal) / uint64_t(5);
	uint64_t heat = diffuse_toward(center, neighbor_average);

	vec2 p = vec2(pixel) + vec2(0.5);
	heat = max(heat, brush_heat(p));
	heat_out[pixel.y * size.x + pixel.x] = min(heat, HEAT_ONE);
}
)";

constexpr const char* kColorShader = R"(
#version 460
#extension GL_ARB_gpu_shader_int64 : require
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(set = 0, binding = 0) readonly buffer HeatIn {
	uint64_t heat_in[];
};
layout(set = 0, binding = 1, rgba8) uniform writeonly image2D ColorOut;

layout(set = 0, binding = 2) readonly buffer Params {
	vec4 texture_size_time;
	vec4 brush_a;
	vec4 brush_b;
	vec4 camera;
} params;

const uint64_t HEAT_ONE = uint64_t(4294967296ul);

vec3 ramp_color(float t) {
	vec3 c0 = vec3(0.63, 0.91, 1.00);
	vec3 c1 = vec3(0.03, 0.20, 0.96);
	vec3 c2 = vec3(0.02, 0.68, 0.17);
	vec3 c3 = vec3(1.00, 0.95, 0.06);
	vec3 c4 = vec3(1.00, 0.48, 0.02);
	vec3 c5 = vec3(0.94, 0.03, 0.01);
	vec3 c6 = vec3(0.26, 0.00, 0.00);
	float x = clamp(t, 0.0, 1.0) * 6.0;
	float local = smoothstep(0.0, 1.0, fract(x));
	if (x < 1.0) { return mix(c0, c1, local); }
	if (x < 2.0) { return mix(c1, c2, local); }
	if (x < 3.0) { return mix(c2, c3, local); }
	if (x < 4.0) { return mix(c3, c4, local); }
	if (x < 5.0) { return mix(c4, c5, local); }
	return mix(c5, c6, smoothstep(0.0, 1.0, x - 5.0));
}

void main() {
	ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
	ivec2 size = imageSize(ColorOut);
	if (pixel.x >= size.x || pixel.y >= size.y) {
		return;
	}

	float heat = float(heat_in[pixel.y * size.x + pixel.x]) / float(HEAT_ONE);
	vec3 color = ramp_color(smoothstep(0.004, 0.92, heat));
	imageStore(ColorOut, pixel, vec4(color, 1.0));
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

layout(set = 0, binding = 0) uniform sampler2D ColorTexture;

layout(set = 0, binding = 1) uniform RenderParams {
	vec4 texture_size_time;
	vec4 brush_a;
	vec4 brush_b;
	vec4 camera;
} params;

void main() {
	vec2 texture_size = params.texture_size_time.xy;
	vec2 target = params.camera.xy;
	float zoom = params.camera.z;
	float aspect = params.camera.w;
	vec2 centered = (uv - 0.5) * vec2(aspect, 1.0);
	vec2 pixel = target + centered * texture_size.y / zoom;
	vec2 heat_uv = pixel / texture_size;

	if (any(lessThan(heat_uv, vec2(0.0))) || any(greaterThan(heat_uv, vec2(1.0)))) {
		out_color = vec4(0.005, 0.007, 0.010, 1.0);
		return;
	}

	vec3 color = texture(ColorTexture, heat_uv).rgb;
	float vignette = smoothstep(0.94, 0.18, length(uv - 0.5));
	color *= 0.74 + vignette * 0.34;

	float pixel_scale = zoom / texture_size.y;
	if (pixel_scale > 0.035) {
		vec2 cell = abs(fract(pixel) - 0.5);
		float line = smoothstep(0.485, 0.500, max(cell.x, cell.y));
		color = mix(color, vec3(0.0), line * 0.28);
	}

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

void cursor_moved(GLFWwindow* window, double x, double y) {
	(void)window;
	CursorX = (f32)x;
	CursorY = (f32)y;
	PreviousCursorX = CursorX;
	PreviousCursorY = CursorY;
}

void mouse_button(GLFWwindow* window, int button, int action, int) {
	(void)window;
	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		Drawing = action == GLFW_PRESS;
		if (action == GLFW_PRESS) {
			BrushHasPrevious = false;
		}
	}
}

void scroll_moved(GLFWwindow* window, double, double y) {
	(void)window;
	ScrollY += (f32)y;
}

f32 camera_aspect(void) {
	const u32 stored_width = FramebufferWidth.load(std::memory_order_acquire);
	const u32 stored_height = FramebufferHeight.load(std::memory_order_acquire);
	const u32 width = stored_width > 0u ? stored_width : 1u;
	const u32 height = stored_height > 0u ? stored_height : 1u;
	return (f32)width / (f32)height;
}

void clamp_camera(Camera* camera, f32 aspect) {
	(void)aspect;
	camera->zoom = std::clamp(camera->zoom, 1.0f, 96.0f);
}

void screen_to_heat_pixel(const Camera& camera, f32 cursor_x, f32 cursor_y, f32* out_x, f32* out_y) {
	const u32 loaded_width = FramebufferWidth.load(std::memory_order_acquire);
	const u32 loaded_height = FramebufferHeight.load(std::memory_order_acquire);
	const u32 width = loaded_width > 0u ? loaded_width : 1u;
	const u32 height = loaded_height > 0u ? loaded_height : 1u;
	const f32 aspect = (f32)width / (f32)height;
	const f32 centered_x = (cursor_x / (f32)width - 0.5f) * aspect;
	const f32 centered_y = 0.5f - cursor_y / (f32)height;
	*out_x = camera.x + centered_x * (f32)kTextureHeight / camera.zoom;
	*out_y = camera.y + centered_y * (f32)kTextureHeight / camera.zoom;
}

void update_camera(GLFWwindow* window, Camera* camera, Camera* target_camera, f32 dt) {
	const f32 aspect = camera_aspect();
	const u32 loaded_width = FramebufferWidth.load(std::memory_order_acquire);
	const u32 loaded_height = FramebufferHeight.load(std::memory_order_acquire);
	const u32 width = loaded_width > 0u ? loaded_width : 1u;
	const u32 height = loaded_height > 0u ? loaded_height : 1u;

	const f32 centered_x = (CursorX / (f32)width - 0.5f) * aspect;
	const f32 centered_y = 0.5f - CursorY / (f32)height;
	if (ScrollY != 0.0f) {
		const f32 old_zoom = target_camera->zoom;
		const f32 focus_x = target_camera->x + centered_x * (f32)kTextureHeight / old_zoom;
		const f32 focus_y = target_camera->y + centered_y * (f32)kTextureHeight / old_zoom;
		target_camera->zoom = std::clamp(old_zoom * std::exp(ScrollY * kScrollZoomSpeed), 1.0f, 96.0f);
		target_camera->x = focus_x - centered_x * (f32)kTextureHeight / target_camera->zoom;
		target_camera->y = focus_y - centered_y * (f32)kTextureHeight / target_camera->zoom;
		ScrollY = 0.0f;
	}

	const f32 key_speed = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? kFastPanSpeed : kPanSpeed) /
						  target_camera->zoom;
	f32 dx = 0.0f;
	f32 dy = 0.0f;
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
		dx -= 1.0f;
	}
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
		dx += 1.0f;
	}
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
		dy += 1.0f;
	}
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
		dy -= 1.0f;
	}
	if (dx != 0.0f || dy != 0.0f) {
		const f32 inv_len = 1.0f / std::sqrt(dx * dx + dy * dy);
		target_camera->x += dx * inv_len * key_speed * dt;
		target_camera->y += dy * inv_len * key_speed * dt;
	}

	if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
		target_camera->x = kTextureWidth * 0.5f;
		target_camera->y = kTextureHeight * 0.5f;
		target_camera->zoom = 1.0f;
	}

	clamp_camera(target_camera, aspect);
	camera->x = target_camera->x;
	camera->y = target_camera->y;
	const f32 follow = 1.0f - std::exp(-dt * 16.0f);
	camera->zoom += (target_camera->zoom - camera->zoom) * follow;
	clamp_camera(camera, aspect);
}

void write_params(SimParams* params, const Camera& camera, f32 time) {
	params->texture[0] = (f32)kTextureWidth;
	params->texture[1] = (f32)kTextureHeight;
	params->texture[2] = time;
	params->texture[3] = 0.0f;

	f32 brush_x = 0.0f;
	f32 brush_y = 0.0f;
	screen_to_heat_pixel(camera, CursorX, CursorY, &brush_x, &brush_y);
	if (Drawing) {
		if (!BrushHasPrevious) {
			BrushPreviousX = brush_x;
			BrushPreviousY = brush_y;
			BrushHasPrevious = true;
		}
		params->brush_a[0] = BrushPreviousX;
		params->brush_a[1] = BrushPreviousY;
		params->brush_a[2] = kBrushRadius;
		params->brush_a[3] = 1.0f;
		params->brush_b[0] = brush_x;
		params->brush_b[1] = brush_y;
		params->brush_b[2] = 1.0f;
		params->brush_b[3] = 0.0f;
		BrushPreviousX = brush_x;
		BrushPreviousY = brush_y;
	} else {
		BrushHasPrevious = false;
		params->brush_a[0] = brush_x;
		params->brush_a[1] = brush_y;
		params->brush_a[2] = kBrushRadius;
		params->brush_a[3] = 0.0f;
		params->brush_b[0] = brush_x;
		params->brush_b[1] = brush_y;
		params->brush_b[2] = 0.0f;
		params->brush_b[3] = 0.0f;
	}

	params->camera[0] = camera.x;
	params->camera[1] = camera.y;
	params->camera[2] = camera.zoom;
	params->camera[3] = camera_aspect();
}

rt_compute_program create_compute_program(const char* shader) {
	rt_compute_program program = rtComputeProgramCreate();
	rtComputeProgramShader(program, std::strlen(shader), shader);
	rtComputeProgramLink(program);
	return program;
}

int main(int argc, char* argv[]) {
	rtLoad("rt-vulkan", nullptr, 0);
	rtLoad_RT_EXT_COMPUTE();

	std::cout << "backend: " << rtGetName() << "\n";
	rtInit(kFeatures, 1);
	if (!check_rt_error("rtInit")) {
		rtUnload();
		return 1;
	}
	rtLoad_RT_EXT_SWAPCHAIN();
	rtLoad_RT_EXT_GLFW();
	if (!glfwInit()) {
		std::cerr << "glfwInit failed\n";
		rtExit();
		rtUnload();
		return 1;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	GLFWwindow* window = glfwCreateWindow(kWindowWidth, kWindowHeight, "Rutile 03 Heat Diffusion", nullptr, nullptr);
	if (!window) {
		std::cerr << "glfwCreateWindow failed\n";
		glfwTerminate();
		rtExit();
		rtUnload();
		return 1;
	}

	glfwSetCursorPosCallback(window, cursor_moved);
	glfwSetMouseButtonCallback(window, mouse_button);
	glfwSetScrollCallback(window, scroll_moved);

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

	const u64 heat_byte_size = (u64)kTextureWidth * kTextureHeight * sizeof(u64);
	rt_buffer heat_buffers[2] = { rtBufferCreate(), rtBufferCreate() };
	rtBufferData(heat_buffers[0], RT_BUFFER_STATIC, RT_BUFFER_USAGE_STORAGE, heat_byte_size, nullptr);
	rtBufferData(heat_buffers[1], RT_BUFFER_STATIC, RT_BUFFER_USAGE_STORAGE, heat_byte_size, nullptr);

	rt_texture color_texture = rtTextureCreate();
	rtTextureData(color_texture, RT_TEXTURE_2D, 0, kTextureWidth, kTextureHeight, 1, RT_RGBA8_UNORM, nullptr);
	rt_texture_view color_view = rtTextureViewCreate();
	rtTextureViewBind(color_view, color_texture);
	rtTextureViewFilter(color_view, RT_FILTER_LINEAR, RT_FILTER_LINEAR, RT_MIP_FILTER_NONE);
	rtTextureViewAddress(color_view, RT_ADDRESS_CLAMP, RT_ADDRESS_CLAMP, RT_ADDRESS_CLAMP);

	SimParams params = {};
	rt_buffer params_buffer = rtBufferCreate();
	rtBufferData(params_buffer, RT_BUFFER_DYNAMIC, (rt_buffer_usage)(RT_BUFFER_USAGE_STORAGE | RT_BUFFER_USAGE_UNIFORM), sizeof(params), &params);

	rt_compute_program init_program = create_compute_program(kInitShader);
	rt_compute_program step_program = create_compute_program(kStepShader);
	rt_compute_program color_program = create_compute_program(kColorShader);

	rt_graphics_program graphics_program = rtGraphicsProgramCreate();
	rtGraphicsProgramLayout(graphics_program, nullptr);
	rtGraphicsProgramVertexShader(graphics_program, std::strlen(kVertexShader), kVertexShader);
	rtGraphicsProgramFragmentShader(graphics_program, std::strlen(kFragmentShader), kFragmentShader);
	rtGraphicsProgramFinalize(graphics_program);

	rt_uniform_location color_location = rtGraphicsProgramUniformLocation(graphics_program, "ColorTexture");
	rt_uniform_location render_params_location = rtGraphicsProgramUniformLocation(graphics_program, "RenderParams");

	rt_command_buffer cmd = rtCommandBufferCreate();
	Camera camera;
	Camera target_camera;
	write_params(&params, camera, 0.0f);
	rtBufferSubdata(params_buffer, 0, sizeof(params), &params);
	rtCmdBegin(cmd, queue);
	rtCmdUseComputeProgram(cmd, init_program);
	rtCmdStorageBuffer(cmd, 0, heat_buffers[0], 0, heat_byte_size);
	rtCmdStorageBuffer(cmd, 2, params_buffer, 0, sizeof(params));
	rtCmdDispatch(cmd, ceil_div_u32(kTextureWidth, 16u), ceil_div_u32(kTextureHeight, 16u), 1);
	rtCmdComputeBarrier(cmd);
	rtCmdEnd(cmd);
	rt_timepoint initialized = rtQueueSubmit(queue, cmd);
	rtTimepointWait(initialized);

	auto start_time = std::chrono::steady_clock::now();
	auto previous_time = start_time;
	auto fps_time = start_time;
	u32 fps_frames = 0;
	u32 read_index = 0;
	u32 write_index = 1;
	rt_timepoint last_rendered = { RT_NULL_HANDLE, 0 };

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
			glfwSetWindowShouldClose(window, GLFW_TRUE);
		}

		const auto now = std::chrono::steady_clock::now();
		const std::chrono::duration<f32> elapsed = now - start_time;
		const std::chrono::duration<f32> delta = now - previous_time;
		previous_time = now;
		update_camera(window, &camera, &target_camera, delta.count());
		write_params(&params, camera, elapsed.count());
		rtBufferSubdata(params_buffer, 0, sizeof(params), &params);

		rt_swapchain_acquire_result acquired = rtSwapchainAcquire(swapchain);
		if (!acquired.framebuffer) {
			continue;
		}

		rtQueueWait(queue, acquired.timepoint);
		rtCmdBegin(cmd, queue);
		rtCmdUseComputeProgram(cmd, step_program);
		rtCmdStorageBuffer(cmd, 0, heat_buffers[read_index], 0, heat_byte_size);
		rtCmdStorageBuffer(cmd, 1, heat_buffers[write_index], 0, heat_byte_size);
		rtCmdStorageBuffer(cmd, 2, params_buffer, 0, sizeof(params));
		rtCmdDispatch(cmd, ceil_div_u32(kTextureWidth, 16u), ceil_div_u32(kTextureHeight, 16u), 1);
		rtCmdComputeBarrier(cmd);
		std::swap(read_index, write_index);
		rtCmdUseComputeProgram(cmd, color_program);
		rtCmdStorageBuffer(cmd, 0, heat_buffers[read_index], 0, heat_byte_size);
		rtCmdStorageTexture(cmd, 1, color_view);
		rtCmdStorageBuffer(cmd, 2, params_buffer, 0, sizeof(params));
		rtCmdDispatch(cmd, ceil_div_u32(kTextureWidth, 16u), ceil_div_u32(kTextureHeight, 16u), 1);
		rtCmdComputeBarrier(cmd);
		rtCmdBeginRendering(cmd, acquired.framebuffer);
		rtCmdClearColor(cmd, 0, 0.0f, 0.0f, 0.0f, 1.0f);
		rtCmdUseGraphicsProgram(cmd, graphics_program);
		rtCmdUniformTexture(cmd, color_location, color_view);
		rtCmdUniformBuffer(cmd, render_params_location, params_buffer, 0, sizeof(params));
		rtCmdDraw(cmd, 3, 0);
		rtCmdEndRendering(cmd);
		rtCmdEnd(cmd);

		rt_timepoint rendered = rtQueueSubmit(queue, cmd);
		last_rendered = rendered;
		rtSwapchainPresent(swapchain, rendered);

		fps_frames++;
		const std::chrono::duration<f32> fps_delta = std::chrono::steady_clock::now() - fps_time;
		if (fps_delta.count() >= 0.5f) {
			char title[160];
			const f32 fps = (f32)fps_frames / fps_delta.count();
			std::snprintf(title, sizeof(title), "Rutile 03 Heat Diffusion - %.0f FPS - %.1fx - left mouse draws", fps, params.camera[2]);
			glfwSetWindowTitle(window, title);
			fps_time = std::chrono::steady_clock::now();
			fps_frames = 0;
		}
	}

	rtTimepointWait(last_rendered);
	rtCommandBufferDestroy(cmd);
	rtGraphicsProgramDestroy(graphics_program);
	rtComputeProgramDestroy(color_program);
	rtComputeProgramDestroy(step_program);
	rtComputeProgramDestroy(init_program);
	rtBufferDestroy(params_buffer);
	rtTextureViewDestroy(color_view);
	rtTextureDestroy(color_texture);
	rtBufferDestroy(heat_buffers[1]);
	rtBufferDestroy(heat_buffers[0]);
	Swapchain.store(RT_NULL_HANDLE, std::memory_order_release);
	rtSwapchainDestroy(swapchain);
	glfwDestroyWindow(window);
	glfwTerminate();
	rtExit();
	rtUnload();
	return 0;
}
