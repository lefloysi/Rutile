#include "embedded_program.hpp"
#include "rutile.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>

extern "C" const rt_example_program reflected_program_data_rtslp;

namespace {

std::string validation_errors;

void record_output(const char* message, void*) {
	if (!message) {
		return;
	}
	std::cerr << message;
	if (std::string_view{ message }.find("VUID-") != std::string_view::npos) {
		validation_errors += message;
	}
}

bool expect_success(std::string_view operation) {
	const rt_error error = rtError();
	if (error == RT_SUCCESS) {
		return true;
	}

	std::cerr << operation << " failed: error=" << error << " message=\"" << rtErrorMessage() << "\"\n";
	rtClearError();
	return false;
}

bool reflected_program_data_upload() {
	rt_program program = rtProgramCreate();
	if (!program || !expect_success("rtProgramCreate")) {
		return false;
	}

	rtProgramSource(
		program,
		"main",
		reflected_program_data_rtslp.data,
		reflected_program_data_rtslp.size
	);
	if (!expect_success("rtProgramSource")) {
		rtProgramDestroy(program);
		return false;
	}

	rtProgramFinalize(program);
	if (!expect_success("rtProgramFinalize")) {
		rtProgramDestroy(program);
		return false;
	}

	rt_location ui_draw = rtProgramUniformLocation(program, "ui_draw");
	if (!ui_draw || !expect_success("rtProgramUniformLocation")) {
		std::cerr << "ui_draw did not produce a live reflected location\n";
		rtProgramDestroy(program);
		return false;
	}

	rt_command_buffer command_buffer = rtCommandBufferCreate();
	if (!command_buffer || !expect_success("rtCommandBufferCreate")) {
		rtProgramDestroy(program);
		return false;
	}

	std::array<u08, 32> ui_draw_data{};
	rtCommandBufferBegin(command_buffer);
	bool succeeded = expect_success("rtCommandBufferBegin");
	if (succeeded) {
		rtCmdUseProgram(command_buffer, program);
		succeeded = expect_success("rtCmdUseProgram");
	}
	if (succeeded) {
		rtCmdUniformData(command_buffer, ui_draw, ui_draw_data.data(), ui_draw_data.size());
		succeeded = expect_success("rtCmdUniformData");
	}
	if (succeeded) {
		const rt_location padded = rtProgramUniformLocation(program, "ui_layout");
		rtCmdUniformData(command_buffer, padded, ui_draw_data.data(), ui_draw_data.size());
		succeeded = expect_success("uploading padded vec2/vec2/scalar uniform");
	}
	if (succeeded) {
		rtCommandBufferEnd(command_buffer);
		succeeded = expect_success("rtCommandBufferEnd");
	}

	rtCommandBufferDestroy(command_buffer);
	rtProgramDestroy(program);
	return succeeded;
}

bool reflected_program_data_render(u32 draw_count) {
	struct Vertex {
		float position[2];
		float uv[2];
	};
	constexpr std::array<Vertex, 3> vertices{ {
		{ { -1.0f, -1.0f }, { 0.0f, 0.0f } },
		{ { 3.0f, -1.0f }, { 2.0f, 0.0f } },
		{ { -1.0f, 3.0f }, { 0.0f, 2.0f } },
	} };
	constexpr std::array<rt_vertex_attribute, 2> attributes{ {
		{ "position", offsetof(Vertex, position), RT_RG32_SFLOAT },
		{ "uv", offsetof(Vertex, uv), RT_RG32_SFLOAT },
	} };
	const rt_vertex_input input{ attributes.data(), attributes.size(), sizeof(Vertex), RT_VERTEX_RATE_VERTEX };
	const rt_vertex_layout layout{ &input, 1 };
	constexpr rt_texture_range image_range{ RT_TEXTURE_ASPECT_COLOR, 0, 1, 0, 1, { 64, 16, 1 }, {} };
	constexpr usize image_byte_size = 64 * 16 * 4;
	constexpr usize pixel_offset = (8 * 64 + 32) * 4;
	constexpr std::array<float, 8> uniforms{ 128.0f / 255.0f, 64.0f / 255.0f, 1.0f, 1.0f, 0.5f, 1.0f, 0.25f, 1.0f };

	rt_program program = rtProgramCreate();
	rt_queue queue = rtQueueCreate(RT_QUEUE_GRAPHICS);
	rt_command_buffer commands = rtCommandBufferCreate();
	rt_command_buffer draws = rtCommandBufferCreate();
	rt_buffer vertex_buffer = rtBufferCreate();
	rt_buffer readback_buffer = rtBufferCreate();
	rt_texture image = rtTextureCreate();
	rt_texture_view image_view = rtTextureViewCreate();
	rt_framebuffer framebuffer = rtFramebufferCreate();

	const bool succeeded = [&] {
		if (!program || !queue || !commands || !draws || !vertex_buffer || !readback_buffer || !image || !image_view || !framebuffer) {
			return false;
		}
		rtProgramSource(program, "draw", reflected_program_data_rtslp.data, reflected_program_data_rtslp.size);
		rtProgramSetLayout(program, &layout);
		rtProgramSetRasterState(program, RT_CULL_NONE, RT_FRONT_FACE_CCW, RT_FILL_SOLID);
		rtProgramFinalize(program);
		if (!expect_success("finalizing uniform program")) {
			return false;
		}
		const rt_location uniform = rtProgramUniformLocation(program, "ui_draw");
		const rt_location opacity_location = rtProgramUniformLocation(program, "opacity");
		const rt_location padded_location = rtProgramUniformLocation(program, "ui_layout");
		const rt_location vertex_input = rtProgramInputLocation(program, attributes.data(), attributes.size());
		const rt_location output = rtProgramOutputLocation(program, nullptr);
		if (!uniform || !vertex_input || !expect_success("querying render locations")) {
			return false;
		}

		rtBufferResize(vertex_buffer, RT_DEVICE_MEMORY, sizeof(vertices));
		rtBufferResize(readback_buffer, RT_HOST_MEMORY, image_byte_size);
		rtTextureResize(image, RT_TEXTURE_2D, RT_RGBA8_UNORM, image_range.extent, 1);
		rtTextureViewSetTexture(image_view, image);
		rtFramebufferSetColorView(framebuffer, image_view, output);
		if (!expect_success("creating render resources")) {
			return false;
		}

		rtCommandBufferBegin(commands);
		rtCmdBufferData(commands, vertex_buffer, { sizeof(vertices), 0 }, reinterpret_cast<const u08*>(vertices.data()));
		rtCmdBufferBarrier(commands, vertex_buffer, { sizeof(vertices), 0 }, { RT_STAGE_TRANSFER, RT_ACCESS_WRITE }, { RT_STAGE_VERTEX, RT_ACCESS_READ });
		rtCommandBufferContinueRendering(draws);
		rtCmdUseProgram(draws, program);
		rtCmdUniformData(draws, uniform, reinterpret_cast<const u08*>(uniforms.data()), sizeof(uniforms));
		const std::array<float, 8> padded_uniform = {64.0f, 16.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f};
		rtCmdUniformData(draws, padded_location, reinterpret_cast<const u08*>(padded_uniform.data()), sizeof(padded_uniform));
		rtCmdVertexBuffer(draws, vertex_input, vertex_buffer, { sizeof(vertices), 0 });
		for (u32 index = 0; index < draw_count; ++index) {
			const float opacity = index % 2 == 0 ? 1.0f : 0.5f;
			rtCmdUniformData(draws, opacity_location, reinterpret_cast<const u08*>(&opacity), sizeof(opacity));
			rtCmdDraw(draws, vertices.size(), 0);
		}
		rtCommandBufferEnd(draws);
		rtCmdBeginRendering(commands, framebuffer);
		rtCmdClearColor(commands, output, 0.0f, 0.0f, 0.0f, 1.0f);
		rtCmdClear(commands, RT_CLEAR_COLOR);
		rtCmdSetViewport(commands, 0, 0, 64, 16, 0.0f, 1.0f);
		rtCmdSetScissor(commands, 0, 0, 64, 16);
		rtCmdExecute(commands, draws);
		rtCmdEndRendering(commands);
		rtCmdTextureBarrier(commands, image, image_range, { RT_STAGE_COLOR_ATTACHMENT, RT_ACCESS_WRITE }, { RT_STAGE_TRANSFER, RT_ACCESS_READ });
		rtCmdTextureCopyToBuffer(commands, image, image_range, readback_buffer, { image_byte_size, 0 });
		rtCommandBufferEnd(commands);
		if (!expect_success("recording uniform draw")) {
			return false;
		}
		const rt_timepoint complete = rtQueueSubmit(queue, commands);
		if (!expect_success("submitting uniform draw")) {
			return false;
		}
		rtTimepointWait(complete);
		if (!expect_success("waiting for uniform draw") || !validation_errors.empty()) {
			return false;
		}
		std::array<u08, 4> pixel{};
		rtBufferRead(readback_buffer, { pixel.size(), pixel_offset }, pixel.data(), pixel.size());
		if (!expect_success("reading conversion test pixel")) {
			return false;
		}
		if (pixel != std::array<u08, 4>{ 128, 255, 64, 255 }) {
			std::cerr << "conversion test produced unexpected pixel: " << static_cast<unsigned>(pixel[0]) << ", "
					  << static_cast<unsigned>(pixel[1]) << ", " << static_cast<unsigned>(pixel[2]) << ", "
					  << static_cast<unsigned>(pixel[3]) << "\n";
			return false;
		}
		std::array<u08, image_byte_size> direct_read{};
		rtTextureViewRead(image_view, image_range, direct_read.data(), direct_read.size());
		if (!expect_success("reading rendered texture directly")) {
			return false;
		}
		if (!std::equal(pixel.begin(), pixel.end(), direct_read.begin() + pixel_offset)) {
			std::cerr << "direct texture readback did not match buffer readback\n";
			return false;
		}
		return true;
	}();

	rtTimepointWait(rtQueueFlush(queue));
	rtCommandBufferDestroy(commands);
	rtCommandBufferDestroy(draws);
	rtFramebufferDestroy(framebuffer);
	rtTextureViewDestroy(image_view);
	rtTextureDestroy(image);
	rtBufferDestroy(readback_buffer);
	rtBufferDestroy(vertex_buffer);
	rtQueueDestroy(queue);
	rtProgramDestroy(program);
	return succeeded && expect_success("destroying render resources");
}

} // namespace

int main(int argc, char** argv) {
	if (argc < 2 || argc > 3 || (argc == 3 && std::string_view{argv[2]} != "--upload-only")) {
		std::cerr << "usage: rutile-test <backend> [--upload-only]\n";
		return 2;
	}

	const rt_error load_error = rtLoad(argv[1], nullptr, 0);
	if (load_error != RT_SUCCESS) {
		std::cerr << "rtLoad failed: error=" << load_error << "\n";
		return 1;
	}

	rtSetOutput(record_output, nullptr);
	rtInit(nullptr, 0);
	if (!expect_success("rtInit")) {
		rtUnload();
		return 1;
	}

	validation_errors.clear();
	const bool succeeded = reflected_program_data_upload() && (argc == 3 || (reflected_program_data_render(1) && reflected_program_data_render(1025)));
	rtExit();
	const bool exit_succeeded = expect_success("rtExit");
	rtUnload();
	return succeeded && exit_succeeded ? 0 : 1;
}
