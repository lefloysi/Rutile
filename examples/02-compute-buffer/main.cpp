#define RUTILE_IMPL
#include "rt_ext_compute.h"
#include "rutile.h"

#include <array>
#include <cstring>
#include <iostream>

constexpr const char* kDefaultBackendName = "rt-vk13";
constexpr const char* kLayers[] = { "RT_VALIDATION_LAYER", "RT_LOGGING_LAYER" };

constexpr const char* kComputeShader = R"(
#version 460
layout(local_size_x = 8, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0) buffer Values {
	uint values[];
};

void main() {
	uint index = gl_GlobalInvocationID.x;
	values[index] = values[index] * 2u + 1u;
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

int main(int argc, char** argv) {
	const char* backend_name = argc > 1 ? argv[1] : kDefaultBackendName;
	if (rtLoad(backend_name, kLayers, 1) != RT_SUCCESS) {
		std::cerr << "rtLoad failed: " << rtErrorMessage() << "\n";
		return 1;
	}
	if (!rtLoad_RT_EXT_COMPUTE()) {
		std::cerr << "RT_EXT_COMPUTE is not available from backend " << backend_name << "\n";
		rtUnload();
		return 1;
	}

	std::cout << "backend: " << rtGetName() << "\n";
	rtInit(nullptr, 0);
	rt_queue queue = rtQueueQuery(RT_QUEUE_COMPUTE);

	constexpr std::array<u32, 16> kInput = {
		0,
		1,
		2,
		3,
		4,
		5,
		6,
		7,
		8,
		9,
		10,
		11,
		12,
		13,
		14,
		15,
	};
	std::array<u32, kInput.size()> values = kInput;
	constexpr u64 kBufferSize = sizeof(values);

	rt_buffer storage_buffer = rtBufferCreate();
	rtBufferData(storage_buffer, RT_BUFFER_DYNAMIC, RT_BUFFER_USAGE_STORAGE, kBufferSize, values.data());

	rt_compute_program program = rtComputeProgramCreate();
	rtComputeProgramShader(program, std::strlen(kComputeShader), kComputeShader);
	rtComputeProgramLink(program);

	rt_command_buffer cmd = rtCommandBufferCreate();
	rtCmdBegin(cmd, queue);
	rtCmdUseComputeProgram(cmd, program);
	rtCmdStorageBuffer(cmd, 0, storage_buffer, 0, kBufferSize);
	rtCmdDispatch(cmd, static_cast<u32>(values.size() / 8), 1, 1);
	rtCmdEnd(cmd);

	rt_timepoint completed = rtQueueSubmit(queue, cmd);
	rtTimepointWait(completed);
	rtBufferRead(storage_buffer, 0, kBufferSize, values.data());

	bool success = true;
	for (usize i = 0; i < values.size(); i++) {
		const u32 expected = kInput[i] * 2u + 1u;
		if (values[i] != expected) {
			std::cerr << "values[" << i << "] = " << values[i] << ", expected " << expected << "\n";
			success = false;
		}
	}

	if (success) {
		std::cout << "compute result:";
		for (u32 value : values) {
			std::cout << " " << value;
		}
		std::cout << "\n";
	}

	rtCommandBufferDestroy(cmd);
	rtComputeProgramDestroy(program);
	rtBufferDestroy(storage_buffer);
	rtExit();
	rtUnload();
	return success ? 0 : 1;
}
