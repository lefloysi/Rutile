#ifndef RUTILE_BACKEND_TOOLS_SHADER_TRANSLATION_HPP
#define RUTILE_BACKEND_TOOLS_SHADER_TRANSLATION_HPP

#include <rutile.h>

#include <string>
#include <vector>

namespace rutile::backend_tools {

enum class ShaderStage {
	Vertex,
	Fragment,
};

enum class ShaderResourceKind {
	UniformBuffer,
	StorageBuffer,
	Texture,
};

struct ShaderResource {
	std::string name;
	ShaderResourceKind kind = ShaderResourceKind::UniformBuffer;
	u32 binding = 0;
};

struct StageTranslation {
	std::vector<u32> spirv;
	std::string hlsl;
	std::vector<ShaderResource> resources;
};

struct GraphicsTranslation {
	StageTranslation vertex;
	StageTranslation fragment;
};

void initialize_shader_tools();
void shutdown_shader_tools();

GraphicsTranslation translate_graphics_glsl_to_hlsl(
	const rt_vertex_layout* vertex_layout,
	u64 vertex_size,
	const void* vertex_source,
	u64 fragment_size,
	const void* fragment_source);

} // namespace rutile::backend_tools

#endif

