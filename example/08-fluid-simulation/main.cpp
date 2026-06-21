#define RUTILE_IMPL
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "rutile.h"
#include "rt_ext_glfw.h"
#include "rt_ext_swapchain.h"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <span>
#include <string_view>
#include <vector>

constexpr i32 grid_width = 128;
constexpr i32 grid_height = 72;
constexpr i32 pixels_per_cell = 10;
constexpr f32 brush_fill = 1.0f;
constexpr f32 brush_radius = 3.0f;


constexpr i32 canvas_width = grid_width * pixels_per_cell;
constexpr i32 canvas_height = grid_height * pixels_per_cell;

f32 mouse_x = 0.0f;
f32 mouse_y = 0.0f;
bool left_down = false;
struct Cell {
	f32 fill_amount = 0.0f;
};
struct {
	std::array<std::array<Cell, grid_width>, grid_height> cells = {};


} World;

constexpr std::string_view vertex_shader = R"(
#version 460
layout(location = 0) out vec2 uv;
void main() {
	vec2 positions[3] = vec2[](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
	vec2 position = positions[gl_VertexIndex];
	uv = position * 0.5 + 0.5;
	gl_Position = vec4(position, 0.0, 1.0);
}
)";

constexpr std::string_view fragment_shader = R"(
#version 460
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_color;
layout(set = 0, binding = 0) uniform sampler2D GridImage;
void main() {
	out_color = texture(GridImage, vec2(uv.x, 1.0 - uv.y));
}
)";

f32 clamp_f32(f32 value, f32 lo, f32 hi) { return value < lo ? lo : (value > hi ? hi : value); }
u32 byte_color(f32 value) { return static_cast<u32>(std::round(clamp_f32(value, 0.0f, 1.0f) * 255.0f)); }
u32 pack_color(f32 r, f32 g, f32 b) { return (byte_color(1.0f) << 24) | (byte_color(b) << 16) | (byte_color(g) << 8) | byte_color(r); }

void paint_cell() {
	const f32 center_x = mouse_x * static_cast<f32>(grid_width);
	const f32 center_y = mouse_y * static_cast<f32>(grid_height);
	const i32 min_x = std::max(0, static_cast<i32>(std::floor(center_x - brush_radius)));
	const i32 max_x = std::min(grid_width - 1, static_cast<i32>(std::ceil(center_x + brush_radius)));
	const i32 min_y = std::max(0, static_cast<i32>(std::floor(center_y - brush_radius)));
	const i32 max_y = std::min(grid_height - 1, static_cast<i32>(std::ceil(center_y + brush_radius)));

	for (i32 y = min_y; y <= max_y; y++) {
		for (i32 x = min_x; x <= max_x; x++) {
			const f32 dx = static_cast<f32>(x) + 0.5f - center_x;
			const f32 dy = static_cast<f32>(y) + 0.5f - center_y;
			if (dx * dx + dy * dy <= brush_radius * brush_radius) {
				World.cells[y][x].fill_amount = brush_fill;
			}
		}
	}
}

void update_grid() {


}

void render_grid(std::span<u32> pixels) {
	const u32 grid_color = pack_color(0.12f, 0.14f, 0.16f);
	for (i32 y = 0; y < canvas_height; y++) {
		for (i32 x = 0; x < canvas_width; x++) {
			const bool grid_line = x % pixels_per_cell == 0 || y % pixels_per_cell == 0;
			const i32 cell_x = std::min(x / pixels_per_cell, grid_width - 1);
			const i32 cell_y = std::min(y / pixels_per_cell, grid_height - 1);
			const f32 cell_fill = clamp_f32(World.cells[cell_y][cell_x].fill_amount, 0.0f, 1.0f);
			const f32 r = 0.030f * (1.0f - cell_fill) + 0.080f * cell_fill;
			const f32 g = 0.035f * (1.0f - cell_fill) + 0.520f * cell_fill;
			const f32 b = 0.042f * (1.0f - cell_fill) + 0.920f * cell_fill;
			pixels[(usize)y * canvas_width + (usize)x] = grid_line ? grid_color : pack_color(r, g, b);
		}
	}
}

void cursor_moved(GLFWwindow*, double x, double y) {
	mouse_x = clamp_f32(static_cast<f32>(x) / static_cast<f32>(canvas_width), 0.0f, 1.0f);
	mouse_y = clamp_f32(static_cast<f32>(y) / static_cast<f32>(canvas_height), 0.0f, 1.0f);
}

void mouse_button(GLFWwindow*, int button, int action, int) {
	if (button == GLFW_MOUSE_BUTTON_LEFT) left_down = action == GLFW_PRESS;
}

int main() {
	rtLoad("rt-vulkan", nullptr, 0);
	rtLoad_RT_EXT_SWAPCHAIN();
	rtLoad_RT_EXT_GLFW();

	const char* features[] = { RT_FEATURE_PRESENTATION };
	rtInit(features, 1);
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(canvas_width, canvas_height, "Rutile 08 Fluid Simulation", nullptr, nullptr);
	glfwSetCursorPosCallback(window, cursor_moved);
	glfwSetMouseButtonCallback(window, mouse_button);

	rt_swapchain swapchain = rtSwapchainCreate();
	rtSwapchainBindWindowGLFW(swapchain, window);

	rt_queue queue = rtQueueQuery(RT_QUEUE_GRAPHICS);

	std::vector<u32> pixels(canvas_width * canvas_height);
	render_grid(pixels);

	rt_texture grid_texture = rtTextureCreate();
	rtTimepointWait(rtTextureData(queue, grid_texture, RT_TEXTURE_2D, 0, canvas_width, canvas_height, 1, RT_RGBA8_UNORM, pixels.data()));
	rt_texture_view grid_view = rtTextureViewCreate(grid_texture);
	rtTextureViewFilter(grid_view, RT_FILTER_LINEAR, RT_FILTER_NEAREST, RT_MIP_FILTER_NONE);
	rtTextureViewAddress(grid_view, RT_ADDRESS_CLAMP, RT_ADDRESS_CLAMP, RT_ADDRESS_CLAMP);

	rt_graphics_program graphics_program = rtGraphicsProgramCreate();
	rtGraphicsProgramVertexLayout(graphics_program, nullptr);
	rtGraphicsProgramVertexShader(graphics_program, vertex_shader.size(), vertex_shader.data());
	rtGraphicsProgramFragmentShader(graphics_program, fragment_shader.size(), fragment_shader.data());
	rtGraphicsProgramLink(graphics_program);
	rt_uniform_location image_location = rtGraphicsProgramUniformLocation(graphics_program, "GridImage");

	rt_command_buffer cmd = rtCmdCreate();
	rt_timepoint last_rendered = { RT_NULL_HANDLE, 0 };

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
			for (auto& row : World.cells) {
				for (auto& cell : row) {
					cell.fill_amount = 0.0f;
				}
			}
		}

		if (left_down) {
			paint_cell();
		}
		update_grid();
		render_grid(pixels);

		rtTimepointWait(last_rendered);
		rtTimepointWait(rtTextureSubdata(queue, grid_texture, 0, 0, 0, 0, canvas_width, canvas_height, 1, pixels.data()));

		rt_swapchain_acquire_result acquired = rtSwapchainAcquire(swapchain);
		if (!acquired.framebuffer) continue;

		rtQueueWait(queue, acquired.timepoint);
		rtCmdBegin(cmd, queue);
		rtCmdBeginRendering(cmd, acquired.framebuffer);
		rtCmdClearColor(cmd, 0, 0.025f, 0.028f, 0.033f, 1.0f);
		rtCmdUseGraphicsProgram(cmd, graphics_program);
		rtCmdUniformTexture(cmd, image_location, grid_view);
		rtCmdDraw(cmd, 3, 0);
		rtCmdEndRendering(cmd);
		rtCmdEnd(cmd);

		last_rendered = rtQueueSubmit(queue, cmd);
		rtSwapchainPresent(swapchain, last_rendered);
	}

	rtTimepointWait(last_rendered);
	rtCmdDestroy(cmd);
	rtGraphicsProgramDestroy(graphics_program);
	rtTextureViewDestroy(grid_view);
	rtTextureDestroy(grid_texture);
	rtSwapchainDestroy(swapchain);
	glfwDestroyWindow(window);
	glfwTerminate();
	rtExit();
	rtUnload();
	return 0;
}
