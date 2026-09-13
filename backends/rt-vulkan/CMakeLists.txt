cmake_minimum_required(VERSION 3.28)
project(rt-vulkan LANGUAGES C CXX)

find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(SPIRV-Tools CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(volk CONFIG REQUIRED)

add_library(rt-vulkan SHARED)

target_sources(rt-vulkan PRIVATE
    "src/config.h"
    "src/context.c"
    "src/context.h"
    "src/core.c"
    "src/core.h"
    "src/error.c"
    "src/error.h"
    "src/glfw.c"
    "src/glfw.h"
    "src/resource/buffer.c"
    "src/resource/buffer.h"
    "src/resource/command_buffer.c"
    "src/resource/command_buffer.h"
    "src/resource/framebuffer.c"
    "src/resource/framebuffer.h"
    "src/resource/program.c"
    "src/resource/program.h"
    "src/resource/queue.c"
    "src/resource/queue.h"
    "src/resource/resource.c"
    "src/resource/resource.h"
    "src/resource/sampler.c"
    "src/resource/sampler.h"
    "src/resource/swapchain.c"
    "src/resource/swapchain.h"
    "src/resource/texture.c"
    "src/resource/texture.h"
    "src/rutile.h"
	"src/system/sync.h"
	"src/system/atomic.h"
	"src/system/atomic.cpp"
	"src/transpiler/transpiler.cpp"
	"src/transpiler/transpiler.h"
    "src/vma.cpp"
)

if(WIN32)
    target_sources(rt-vulkan PRIVATE
		"src/system/windows/sync.c"
    )
else()
    target_sources(rt-vulkan PRIVATE
		"src/system/posix/sync.c"
    )
endif()

target_compile_features(rt-vulkan PRIVATE c_std_11 cxx_std_23)
target_compile_definitions(rt-vulkan PRIVATE
    VK_NO_PROTOTYPES
    VMA_STATIC_VULKAN_FUNCTIONS=0
    VMA_DYNAMIC_VULKAN_FUNCTIONS=1
    "$<$<CONFIG:Debug>:RTVK_ENABLE_VULKAN_VALIDATION>"
)
target_include_directories(rt-vulkan PRIVATE "src" "src/system")
target_link_libraries(rt-vulkan PRIVATE
	RTSL::rtsl
    SPIRV-Tools-static
    volk::volk
    GPUOpen::VulkanMemoryAllocator
    $<$<TARGET_EXISTS:Vulkan::Headers>:Vulkan::Headers>
    ${CMAKE_DL_LIBS}
    Threads::Threads
)

if(MSVC)
	target_compile_options(rt-vulkan PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/EHsc>)
endif()

set_target_properties(rt-vulkan PROPERTIES
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
