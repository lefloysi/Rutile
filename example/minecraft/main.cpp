#define RUTILE_IMPL
#include "rutile.h"
#include "rt_ext_glfw.h"
#include "rt_ext_swapchain.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>

constexpr const char* kDefaultBackendName = "rt-dx12";
constexpr const char* kLayers[] = { "RT_VALIDATION" };
constexpr const char* kFeatures[] = { RT_FEATURE_PRESENTATION };

constexpr i32 kChunkSize = 16;
constexpr i32 kChunkCountX = 8;
constexpr i32 kChunkCountZ = 8;
constexpr i32 kWorldX = kChunkSize * kChunkCountX;
constexpr i32 kWorldY = 24;
constexpr i32 kWorldZ = kChunkSize * kChunkCountZ;
constexpr f32 kWaterLevel = 7.35f;

struct Vertex {
	f32 position[3];
	f32 color[3];
	f32 normal[3];
	f32 ao;
	f32 pixel_uv[2];
	f32 edge_mask;
	f32 corner_mask;
};

struct TransformUniform {
	f32 transform[16];
	f32 time;
	f32 padding[3];
};

typedef TransformUniform WaterUniform;

struct Camera {
	glm::vec3 position = glm::vec3(0.0f, 13.0f, 18.0f);
	f32 yaw = -glm::radians(90.0f);
	f32 pitch = -0.32f;
};

struct AppState {
	std::atomic<rt_swapchain> swapchain = RT_NULL_HANDLE;
	std::atomic<u32> framebuffer_width = 1280;
	std::atomic<u32> framebuffer_height = 720;
	std::atomic<bool> resize_pending = false;
	std::atomic<bool> resize_in_progress = false;
	f32 mouse_dx = 0.0f;
	f32 mouse_dy = 0.0f;
};

constexpr rt_vertex_attribute kVertexAttributes[] = {
	{ 0, offsetof(Vertex, position), RT_RGB32_SFLOAT },
	{ 1, offsetof(Vertex, color), RT_RGB32_SFLOAT },
	{ 2, offsetof(Vertex, normal), RT_RGB32_SFLOAT },
	{ 3, offsetof(Vertex, ao), RT_R32_SFLOAT },
	{ 4, offsetof(Vertex, pixel_uv), RT_RG32_SFLOAT },
	{ 5, offsetof(Vertex, edge_mask), RT_R32_SFLOAT },
	{ 6, offsetof(Vertex, corner_mask), RT_R32_SFLOAT },
};

constexpr rt_vertex_layout kVertexLayout = { sizeof(Vertex), kVertexAttributes, 7 };

constexpr const char* kVertexShader = R"(
#version 460
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in float in_ao;
layout(location = 4) in vec2 in_pixel_uv;
layout(location = 5) in float in_edge_mask;
layout(location = 6) in float in_corner_mask;
layout(location = 0) flat out vec3 color;
layout(location = 1) flat out vec3 normal;
layout(location = 2) out float ao;
layout(location = 3) out vec2 pixel_uv;
layout(location = 4) flat out int edge_mask;
layout(location = 5) flat out int corner_mask;

layout(set = 0, binding = 0) uniform Transform {
	mat4 transform;
} u_transform;

void main() {
	gl_Position = u_transform.transform * vec4(in_position, 1.0);
	color = in_color;
	normal = in_normal;
	ao = in_ao;
	pixel_uv = in_pixel_uv;
	edge_mask = int(in_edge_mask + 0.5);
	corner_mask = int(in_corner_mask + 0.5);
}
)";

constexpr const char* kFragmentShader = R"(
#version 460
layout(location = 0) flat in vec3 color;
layout(location = 1) flat in vec3 normal;
layout(location = 2) in float ao;
layout(location = 3) in vec2 pixel_uv;
layout(location = 4) flat in int edge_mask;
layout(location = 5) flat in int corner_mask;
layout(location = 0) out vec4 out_color;

const float BlockPixelSize = 8.0;
const float BlockEdgePixelShade = 0.82;

bool mask_bit(int mask, int bit) {
	return (mask & bit) != 0;
}

bool masked_edge_pixel(ivec2 p) {
	return
		(p.x == 0 && mask_bit(edge_mask, 1)) ||
		(p.x == 7 && mask_bit(edge_mask, 2)) ||
		(p.y == 0 && mask_bit(edge_mask, 4)) ||
		(p.y == 7 && mask_bit(edge_mask, 8));
}

bool masked_corner_pixel(ivec2 p) {
	return
		(p.x == 0 && p.y == 0 && mask_bit(corner_mask, 1)) ||
		(p.x == 7 && p.y == 0 && mask_bit(corner_mask, 2)) ||
		(p.x == 7 && p.y == 7 && mask_bit(corner_mask, 4)) ||
		(p.x == 0 && p.y == 7 && mask_bit(corner_mask, 8));
}

void main() {
	ivec2 p = clamp(ivec2(floor(pixel_uv * BlockPixelSize)), ivec2(0), ivec2(7));
	bool outline_pixel = masked_edge_pixel(p) || masked_corner_pixel(p);
	vec3 pixel_color = color * (outline_pixel ? BlockEdgePixelShade : 1.0);
	vec3 n = normalize(normal);
	vec3 light_dir = normalize(vec3(-0.35, 0.85, -0.45));
	vec3 sky_dir = vec3(0.0, 1.0, 0.0);
	float diffuse = max(dot(n, light_dir), 0.0);
	float skylight = clamp(dot(n, sky_dir) * 0.5 + 0.5, 0.0, 1.0);
	float bounce = max(dot(n, normalize(vec3(0.4, 0.35, 0.7))), 0.0) * 0.12;
	vec3 ambient = mix(vec3(0.16, 0.18, 0.22), vec3(0.30, 0.35, 0.42), skylight);
	vec3 sunlight = vec3(1.0, 0.94, 0.82) * diffuse;
	vec3 lit = pixel_color * (ambient + sunlight + bounce) * clamp(ao, 0.35, 1.0);
	out_color = vec4(lit, 1.0);
}
)";

constexpr const char* kWaterVertexShader = R"(
#version 460
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in float in_ao;
layout(location = 4) in vec2 in_pixel_uv;
layout(location = 5) in float in_edge_mask;
layout(location = 6) in float in_corner_mask;
layout(location = 0) out vec3 color;
layout(location = 1) out vec3 normal;
layout(location = 2) out vec3 world_position;

layout(set = 0, binding = 0) uniform Transform {
	mat4 transform;
	float time;
} u_transform;

void main() {
	vec3 position = in_position;
	float wave_a = sin(position.x * 1.45 + u_transform.time * 1.8);
	float wave_b = cos(position.z * 1.20 + u_transform.time * 1.35);
	position.y += (wave_a + wave_b) * 0.055;
	gl_Position = u_transform.transform * vec4(position, 1.0);
	color = in_color;
	normal = normalize(vec3(-wave_a * 0.08, 1.0, -wave_b * 0.08));
	world_position = position;
}
)";

constexpr const char* kWaterFragmentShader = R"(
#version 460
layout(location = 0) in vec3 color;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 world_position;
layout(location = 0) out vec4 out_color;

void main() {
	vec3 n = normalize(normal);
	vec3 light_dir = normalize(vec3(-0.35, 0.85, -0.45));
	float diffuse = max(dot(n, light_dir), 0.0);
	float sparkle = pow(max(dot(n, normalize(vec3(-0.2, 0.95, 0.15))), 0.0), 56.0);
	float ripple = sin(world_position.x * 4.0 + world_position.z * 3.0) * 0.5 + 0.5;
	float fresnel = pow(1.0 - clamp(dot(n, normalize(vec3(0.0, 0.72, 0.68))), 0.0, 1.0), 3.0);
	vec3 ambient = vec3(0.08, 0.16, 0.24);
	vec3 deep_water = color * vec3(0.55, 0.75, 0.95);
	vec3 bright_water = vec3(0.24, 0.55, 0.78);
	vec3 water = mix(deep_water, bright_water, ripple * 0.18 + fresnel * 0.32);
	water *= ambient + vec3(0.35, 0.55, 0.72) * diffuse;
	water += vec3(0.58, 0.78, 0.92) * sparkle;
	out_color = vec4(clamp(water, vec3(0.0), vec3(1.0)), 0.72);
}
)";

f32 hash_noise(i32 x, i32 z) {
	u32 n = (u32)x * 374761393u + (u32)z * 668265263u;
	n = (n ^ (n >> 13)) * 1274126177u;
	return (f32)(n & 0xffffu) / 65535.0f;
}

f32 value_noise(f32 x, f32 z) {
	const i32 x0 = (i32)glm::floor(x);
	const i32 z0 = (i32)glm::floor(z);
	const f32 tx = glm::smoothstep(0.0f, 1.0f, x - (f32)x0);
	const f32 tz = glm::smoothstep(0.0f, 1.0f, z - (f32)z0);
	const f32 a = glm::mix(hash_noise(x0, z0), hash_noise(x0 + 1, z0), tx);
	const f32 b = glm::mix(hash_noise(x0, z0 + 1), hash_noise(x0 + 1, z0 + 1), tx);
	return glm::mix(a, b, tz);
}

i32 terrain_height(i32 x, i32 z) {
	static i32 cached_heights[kWorldX * kWorldZ] = {};
	if (x >= 0 && x < kWorldX && z >= 0 && z < kWorldZ) {
		i32* cached = &cached_heights[z * kWorldX + x];
		if (*cached == 0) {
			*cached = 5 + (i32)(value_noise((f32)x * 0.12f, (f32)z * 0.12f) * 11.0f);
		}
		return *cached;
	}
	return 5 + (i32)(value_noise((f32)x * 0.12f, (f32)z * 0.12f) * 11.0f);
}

bool solid_block(i32 x, i32 y, i32 z) {
	if (x < 0 || x >= kWorldX || y < 0 || y >= kWorldY || z < 0 || z >= kWorldZ) {
		return false;
	}
	return y <= terrain_height(x, z);
}

glm::vec3 block_color(i32 x, i32 y, i32 z) {
	glm::vec3 base;
	const i32 height = terrain_height(x, z);
	if (y == height) {
		base = glm::vec3(0.34f, 0.70f, 0.28f);
	} else if (y > height - 3) {
		base = glm::vec3(0.45f, 0.30f, 0.18f);
	} else {
		base = glm::vec3(0.46f, 0.47f, 0.46f);
	}

	base += glm::vec3((f32)((x * 13 + z * 7 + y * 3) & 3)) * 0.012f;
	return base;
}

void push_vertex(std::vector<Vertex>* vertices, const glm::vec3& position, const glm::vec3& color, const glm::vec3& normal, f32 ao, const glm::vec2& pixel_uv, f32 edge_mask, f32 corner_mask) {
	vertices->push_back({
		{ position.x, position.y, position.z },
		{ color.r, color.g, color.b },
		{ normal.x, normal.y, normal.z },
		ao,
		{ pixel_uv.x, pixel_uv.y },
		edge_mask,
		corner_mask,
	});
}

f32 voxel_ao(i32 x, i32 y, i32 z, const glm::vec3& normal, const glm::vec3& corner) {
	const glm::ivec3 face((i32)normal.x, (i32)normal.y, (i32)normal.z);
	glm::ivec3 side_a(0);
	glm::ivec3 side_b(0);
	if (face.x != 0) {
		side_a.y = corner.y < 0.5f ? -1 : 1;
		side_b.z = corner.z < 0.5f ? -1 : 1;
	} else if (face.y != 0) {
		side_a.x = corner.x < 0.5f ? -1 : 1;
		side_b.z = corner.z < 0.5f ? -1 : 1;
	} else {
		side_a.x = corner.x < 0.5f ? -1 : 1;
		side_b.y = corner.y < 0.5f ? -1 : 1;
	}

	const glm::ivec3 outside = glm::ivec3(x, y, z) + face;
	const bool a = solid_block(outside.x + side_a.x, outside.y + side_a.y, outside.z + side_a.z);
	const bool b = solid_block(outside.x + side_b.x, outside.y + side_b.y, outside.z + side_b.z);
	const bool c = solid_block(outside.x + side_a.x + side_b.x, outside.y + side_a.y + side_b.y, outside.z + side_a.z + side_b.z);
	const i32 value = a && b ? 0 : 3 - (i32)a - (i32)b - (i32)c;
	return 0.55f + (f32)value * 0.15f;
}

void push_triangle(std::vector<Vertex>* vertices, const glm::vec3& origin, const glm::vec3& color,const glm::vec3& normal, const glm::vec3& a, f32 ao_a, const glm::vec2& uv_a, const glm::vec3& b, f32 ao_b, const glm::vec2& uv_b, const glm::vec3& c, f32 ao_c, const glm::vec2& uv_c, f32 edge_mask, f32 corner_mask) {
	push_vertex(vertices, origin + a, color, normal, ao_a, uv_a, edge_mask, corner_mask);
	push_vertex(vertices, origin + b, color, normal, ao_b, uv_b, edge_mask, corner_mask);
	push_vertex(vertices, origin + c, color, normal, ao_c, uv_c, edge_mask, corner_mask);
}

glm::ivec3 edge_direction(const glm::vec3& direction) {
	return glm::ivec3((i32)glm::round(direction.x), (i32)glm::round(direction.y), (i32)glm::round(direction.z));
}

void push_face(std::vector<Vertex>* vertices, i32 x, i32 y, i32 z, const glm::vec3& normal, const glm::vec3 corners[4]) {
	const glm::vec3 origin = glm::vec3((f32)x - kWorldX * 0.5f, (f32)y, (f32)z - kWorldZ * 0.5f);
	const glm::vec3 color = block_color(x, y, z);
	const f32 ao[] = {
		voxel_ao(x, y, z, normal, corners[0]),
		voxel_ao(x, y, z, normal, corners[1]),
		voxel_ao(x, y, z, normal, corners[2]),
		voxel_ao(x, y, z, normal, corners[3]),
	};
	const glm::ivec3 u_edge = edge_direction(corners[1] - corners[0]);
	const glm::ivec3 v_edge = edge_direction(corners[3] - corners[0]);
	const bool edge_air[] = {
		!solid_block(x - u_edge.x, y - u_edge.y, z - u_edge.z),
		!solid_block(x + u_edge.x, y + u_edge.y, z + u_edge.z),
		!solid_block(x - v_edge.x, y - v_edge.y, z - v_edge.z),
		!solid_block(x + v_edge.x, y + v_edge.y, z + v_edge.z),
	};
	const bool corner_air[] = {
		!solid_block(x - u_edge.x - v_edge.x, y - u_edge.y - v_edge.y, z - u_edge.z - v_edge.z),
		!solid_block(x + u_edge.x - v_edge.x, y + u_edge.y - v_edge.y, z + u_edge.z - v_edge.z),
		!solid_block(x + u_edge.x + v_edge.x, y + u_edge.y + v_edge.y, z + u_edge.z + v_edge.z),
		!solid_block(x - u_edge.x + v_edge.x, y - u_edge.y + v_edge.y, z - u_edge.z + v_edge.z),
	};
	const glm::vec2 uv[] = {
		glm::vec2(0.0f, 0.0f),
		glm::vec2(1.0f, 0.0f),
		glm::vec2(1.0f, 1.0f),
		glm::vec2(0.0f, 1.0f),
	};
	const f32 edge_mask = (edge_air[0] ? 1.0f : 0.0f) + (edge_air[1] ? 2.0f : 0.0f) + (edge_air[2] ? 4.0f : 0.0f) + (edge_air[3] ? 8.0f : 0.0f);
	const f32 corner_mask = (corner_air[0] ? 1.0f : 0.0f) + (corner_air[1] ? 2.0f : 0.0f) + (corner_air[2] ? 4.0f : 0.0f) + (corner_air[3] ? 8.0f : 0.0f);
	if (ao[0] + ao[2] > ao[1] + ao[3]) {
		push_triangle(vertices, origin, color, normal, corners[0], ao[0], uv[0], corners[1], ao[1], uv[1], corners[3], ao[3], uv[3], edge_mask, corner_mask);
		push_triangle(vertices, origin, color, normal, corners[1], ao[1], uv[1], corners[2], ao[2], uv[2], corners[3], ao[3], uv[3], edge_mask, corner_mask);
	} else {
		push_triangle(vertices, origin, color, normal, corners[0], ao[0], uv[0], corners[1], ao[1], uv[1], corners[2], ao[2], uv[2], edge_mask, corner_mask);
		push_triangle(vertices, origin, color, normal, corners[0], ao[0], uv[0], corners[2], ao[2], uv[2], corners[3], ao[3], uv[3], edge_mask, corner_mask);
	}
}

std::vector<Vertex> build_world_mesh() {
	std::vector<Vertex> vertices;
	vertices.reserve(kWorldX * kWorldZ * 24);

	const glm::vec3 px[] = {
		glm::vec3(1.0f, 0.0f, 1.0f), glm::vec3(1.0f, 0.0f, 0.0f),
		glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f),
	};
	const glm::vec3 nx[] = {
		glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f),
		glm::vec3(0.0f, 1.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f),
	};
	const glm::vec3 py[] = {
		glm::vec3(0.0f, 1.0f, 1.0f), glm::vec3(1.0f, 1.0f, 1.0f),
		glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
	};
	const glm::vec3 ny[] = {
		glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f),
		glm::vec3(1.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f),
	};
	const glm::vec3 pz[] = {
		glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f, 0.0f, 1.0f),
		glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(0.0f, 1.0f, 1.0f),
	};
	const glm::vec3 nz[] = {
		glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f),
		glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.0f),
	};

	for (i32 z = 0; z < kWorldZ; z++) {
		for (i32 y = 0; y < kWorldY; y++) {
			for (i32 x = 0; x < kWorldX; x++) {
				if (!solid_block(x, y, z)) { continue; }
				if (!solid_block(x + 1, y, z)) { push_face(&vertices, x, y, z, glm::vec3(1, 0, 0), px); }
				if (!solid_block(x - 1, y, z)) { push_face(&vertices, x, y, z, glm::vec3(-1, 0, 0), nx); }
				if (!solid_block(x, y + 1, z)) { push_face(&vertices, x, y, z, glm::vec3(0, 1, 0), py); }
				if (!solid_block(x, y - 1, z)) { push_face(&vertices, x, y, z, glm::vec3(0, -1, 0), ny); }
				if (!solid_block(x, y, z + 1)) { push_face(&vertices, x, y, z, glm::vec3(0, 0, 1), pz); }
				if (!solid_block(x, y, z - 1)) { push_face(&vertices, x, y, z, glm::vec3(0, 0, -1), nz); }
			}
		}
	}

	return vertices;
}

std::vector<Vertex> build_water_mesh() {
	std::vector<Vertex> vertices;
	vertices.reserve(kWorldX * kWorldZ * 6);

	const glm::vec3 color = glm::vec3(0.08f, 0.42f, 0.76f);
	const glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);
	for (i32 z = 0; z < kWorldZ; z++) {
		for (i32 x = 0; x < kWorldX; x++) {
			if ((f32)terrain_height(x, z) >= kWaterLevel - 0.4f) { continue; }
			const f32 wx = (f32)x - kWorldX * 0.5f;
			const f32 wz = (f32)z - kWorldZ * 0.5f;
			const glm::vec2 uv(0.0f);
			push_vertex(&vertices, glm::vec3(wx, kWaterLevel, wz + 1.0f), color, normal, 1.0f, uv, 0.0f, 0.0f);
			push_vertex(&vertices, glm::vec3(wx + 1.0f, kWaterLevel, wz + 1.0f), color, normal, 1.0f, uv, 0.0f, 0.0f);
			push_vertex(&vertices, glm::vec3(wx + 1.0f, kWaterLevel, wz), color, normal, 1.0f, uv, 0.0f, 0.0f);
			push_vertex(&vertices, glm::vec3(wx, kWaterLevel, wz + 1.0f), color, normal, 1.0f, uv, 0.0f, 0.0f);
			push_vertex(&vertices, glm::vec3(wx + 1.0f, kWaterLevel, wz), color, normal, 1.0f, uv, 0.0f, 0.0f);
			push_vertex(&vertices, glm::vec3(wx, kWaterLevel, wz), color, normal, 1.0f, uv, 0.0f, 0.0f);
		}
	}

	return vertices;
}

glm::vec3 camera_forward(const Camera& camera) {
	const f32 cp = glm::cos(camera.pitch);
	return glm::normalize(glm::vec3(glm::cos(camera.yaw) * cp, glm::sin(camera.pitch), glm::sin(camera.yaw) * cp));
}

glm::mat4 camera_matrix(const Camera& camera, f32 aspect) {
	const glm::vec3 forward = camera_forward(camera);
	const glm::mat4 view = glm::lookAt(camera.position, camera.position + forward, glm::vec3(0, 1, 0));
	const glm::mat4 projection = glm::perspective(glm::radians(70.0f), aspect, 0.05f, 180.0f);
	return projection * view;
}

void write_transform(TransformUniform* uniform, const glm::mat4& transform) {
	std::memcpy(uniform->transform, glm::value_ptr(transform), sizeof(uniform->transform));
	uniform->time = 0.0f;
	uniform->padding[0] = 0.0f;
	uniform->padding[1] = 0.0f;
	uniform->padding[2] = 0.0f;
}

void write_water_transform(WaterUniform* uniform, const glm::mat4& transform, f32 time) {
	std::memcpy(uniform->transform, glm::value_ptr(transform), sizeof(uniform->transform));
	uniform->time = time;
	uniform->padding[0] = 0.0f;
	uniform->padding[1] = 0.0f;
	uniform->padding[2] = 0.0f;
}

void recreate_depth_buffer(rt_queue queue, rt_texture* depth_texture, rt_texture_view* depth_view, u32 width, u32 height) {
	if (*depth_view) { rtTextureViewDestroy(*depth_view); }
	if (*depth_texture) { rtTextureDestroy(*depth_texture); }
	*depth_texture = rtTextureCreate();
	rtTimepointWait(rtTextureData(queue, *depth_texture, RT_TEXTURE_2D, 0, width, height, 1, RT_D32_SFLOAT, NULL));
	*depth_view = rtTextureViewCreate(*depth_texture);
	if (!*depth_view) {
		rtTextureDestroy(*depth_texture);
		*depth_texture = RT_NULL_HANDLE;
	}
}

void framebuffer_resized(GLFWwindow* window, int width, int height) {
	AppState* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
	if (state && state->resize_in_progress.load(std::memory_order_acquire)) {
		return;
	}
	if (state && width > 0 && height > 0) {
		state->framebuffer_width.store((u32)width, std::memory_order_release);
		state->framebuffer_height.store((u32)height, std::memory_order_release);
		state->resize_pending.store(true, std::memory_order_release);
	}
}

void cursor_moved(GLFWwindow* window, double x, double y) {
	AppState* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
	if (!state) { return; }

	static double previous_x = x;
	static double previous_y = y;
	state->mouse_dx += (f32)(x - previous_x);
	state->mouse_dy += (f32)(y - previous_y);
	previous_x = x;
	previous_y = y;
}

void update_camera(GLFWwindow* window, AppState* state, Camera* camera, f32 dt) {
	const f32 sensitivity = 0.0024f;
	camera->yaw += state->mouse_dx * sensitivity;
	camera->pitch -= state->mouse_dy * sensitivity;
	camera->pitch = glm::clamp(camera->pitch, -1.45f, 1.45f);
	state->mouse_dx = 0.0f;
	state->mouse_dy = 0.0f;

	const glm::vec3 forward = camera_forward(*camera);
	const glm::vec3 flat_forward = glm::normalize(glm::vec3(forward.x, 0.0f, forward.z));
	const glm::vec3 right = glm::normalize(glm::cross(flat_forward, glm::vec3(0, 1, 0)));
	const f32 speed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 18.0f : 8.0f;
	glm::vec3 velocity(0.0f);

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) { velocity += flat_forward; }
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) { velocity -= flat_forward; }
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { velocity += right; }
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { velocity -= right; }
	if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) { velocity.y += 1.0f; }
	if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) { velocity.y -= 1.0f; }
	if (glm::dot(velocity, velocity) > 0.0f) {
		camera->position += glm::normalize(velocity) * speed * dt;
	}
}

int main(int argc, char** argv) {
	const char* backend_name = argc > 1 ? argv[1] : kDefaultBackendName;
	rtLoad(backend_name, kLayers, 1);
	rtLoad_RT_EXT_SWAPCHAIN();
	rtLoad_RT_EXT_GLFW();

	rtInit(kFeatures, 1);

	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(1280, 720, "Rutile Minecraft", nullptr, nullptr);

	AppState state;
	glfwSetWindowUserPointer(window, &state);
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
		state.framebuffer_width.store((u32)framebuffer_width, std::memory_order_release);
		state.framebuffer_height.store((u32)framebuffer_height, std::memory_order_release);
	}

	rt_swapchain swapchain = rtSwapchainCreate();
	rtSwapchainBindWindowGLFW(swapchain, window);
	state.swapchain.store(swapchain, std::memory_order_release);
	rt_queue queue = rtQueueQuery(RT_QUEUE_GRAPHICS);

	std::vector<Vertex> vertices = build_world_mesh();
	rt_buffer vertex_buffer = rtBufferCreate();
	const u64 vertex_size = (u64)(vertices.size() * sizeof(vertices[0]));
	rtBufferData(vertex_buffer, RT_BUFFER_STATIC, RT_BUFFER_USAGE_VERTEX, vertex_size, vertices.data());

	std::vector<Vertex> water_vertices = build_water_mesh();
	rt_buffer water_vertex_buffer = rtBufferCreate();
	const u64 water_vertex_size = (u64)(water_vertices.size() * sizeof(water_vertices[0]));
	rtBufferData(water_vertex_buffer, RT_BUFFER_STATIC, RT_BUFFER_USAGE_VERTEX, water_vertex_size, water_vertices.data());

	TransformUniform transform = {};
	rt_buffer transform_buffer = rtBufferCreate();
	rtBufferData(transform_buffer, RT_BUFFER_DYNAMIC, RT_BUFFER_USAGE_UNIFORM, sizeof(transform), &transform);

	WaterUniform water_transform = {};
	rt_buffer water_transform_buffer = rtBufferCreate();
	rtBufferData(water_transform_buffer, RT_BUFFER_DYNAMIC, RT_BUFFER_USAGE_UNIFORM, sizeof(water_transform), &water_transform);

	rt_graphics_program graphics_program = rtGraphicsProgramCreate();
	rtGraphicsProgramVertexLayout(graphics_program, &kVertexLayout);
	rtGraphicsProgramVertexShader(graphics_program, std::strlen(kVertexShader), kVertexShader);
	rtGraphicsProgramFragmentShader(graphics_program, std::strlen(kFragmentShader), kFragmentShader);
	rtGraphicsProgramRasterState(graphics_program, RT_CULL_BACK, RT_FRONT_FACE_CCW, RT_FILL_SOLID);
	rtGraphicsProgramLink(graphics_program);
	rt_uniform_location transform_location = rtGraphicsProgramUniformLocation(graphics_program, "Transform");

	rt_graphics_program water_program = rtGraphicsProgramCreate();
	rtGraphicsProgramVertexLayout(water_program, &kVertexLayout);
	rtGraphicsProgramVertexShader(water_program, std::strlen(kWaterVertexShader), kWaterVertexShader);
	rtGraphicsProgramFragmentShader(water_program, std::strlen(kWaterFragmentShader), kWaterFragmentShader);
	rtGraphicsProgramRasterState(water_program, RT_CULL_BACK, RT_FRONT_FACE_CCW, RT_FILL_SOLID);
	rtGraphicsProgramBlendState(water_program, true, RT_BLEND_SRC_ALPHA, RT_BLEND_ONE_MINUS_SRC_ALPHA, RT_BLEND_OP_ADD, RT_BLEND_ONE, RT_BLEND_ONE_MINUS_SRC_ALPHA, RT_BLEND_OP_ADD);
	rtGraphicsProgramLink(water_program);
	rt_uniform_location water_transform_location = rtGraphicsProgramUniformLocation(water_program, "Transform");

	rt_command_buffer cmd = rtCmdCreate();
	rt_texture depth_texture = RT_NULL_HANDLE;
	rt_texture_view depth_view = RT_NULL_HANDLE;
	u32 depth_width = state.framebuffer_width.load(std::memory_order_acquire);
	u32 depth_height = state.framebuffer_height.load(std::memory_order_acquire);
	recreate_depth_buffer(queue, &depth_texture, &depth_view, depth_width, depth_height);

	Camera camera;
	auto start_time = std::chrono::steady_clock::now();
	auto previous_time = start_time;
	auto fps_time = start_time;
	u32 fps_frames = 0;
	rt_timepoint last_rendered = { RT_NULL_HANDLE, 0 };

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
			glfwSetWindowShouldClose(window, GLFW_TRUE);
		}

		auto now = std::chrono::steady_clock::now();
		const std::chrono::duration<f32> delta = now - previous_time;
		previous_time = now;
		update_camera(window, &state, &camera, delta.count());

		const u32 current_width = state.framebuffer_width.load(std::memory_order_acquire);
		const u32 current_height = state.framebuffer_height.load(std::memory_order_acquire);
		const bool depth_size_changed = current_width != depth_width || current_height != depth_height;
		const bool resize_pending = state.resize_pending.load(std::memory_order_acquire);
		if (current_width && current_height && (resize_pending || depth_size_changed)) {
			rtTimepointWait(last_rendered);
			depth_width = current_width;
			depth_height = current_height;
			state.resize_pending.store(false, std::memory_order_release);
			state.resize_in_progress.store(true, std::memory_order_release);
			rtSwapchainResize(swapchain, depth_width, depth_height);
			state.resize_in_progress.store(false, std::memory_order_release);
			recreate_depth_buffer(queue, &depth_texture, &depth_view, depth_width, depth_height);
			continue;
		}

		rt_swapchain_acquire_result acquired = rtSwapchainAcquire(swapchain);
		if (!acquired.framebuffer) { continue; }

		const f32 aspect = current_height ? (f32)current_width / (f32)current_height : 1.0f;
		const glm::mat4 view_projection = camera_matrix(camera, aspect);
		const std::chrono::duration<f32> elapsed = now - start_time;
		write_transform(&transform, view_projection);
		write_water_transform(&water_transform, view_projection, elapsed.count());
		rtBufferSubdata(transform_buffer, 0, sizeof(transform), &transform);
		rtBufferSubdata(water_transform_buffer, 0, sizeof(water_transform), &water_transform);

		rtFramebufferDepthView(acquired.framebuffer, depth_view);
		rtCmdBegin(cmd, queue);
		rtCmdBeginRendering(cmd, acquired.framebuffer);
		rtCmdClearColor(cmd, 0, 0.54f, 0.72f, 0.94f, 1.0f);
		rtCmdClearDepth(cmd, 1.0f);
		rtCmdUseGraphicsProgram(cmd, graphics_program);
		rtCmdBindVertexBuffer(cmd, vertex_buffer, 0);
		rtCmdUniformBuffer(cmd, transform_location, transform_buffer, 0, sizeof(transform));
		rtCmdDraw(cmd, (u32)vertices.size(), 0);
		rtCmdUseGraphicsProgram(cmd, water_program);
		rtCmdBindVertexBuffer(cmd, water_vertex_buffer, 0);
		rtCmdUniformBuffer(cmd, water_transform_location, water_transform_buffer, 0, sizeof(water_transform));
		rtCmdDraw(cmd, (u32)water_vertices.size(), 0);
		rtCmdEndRendering(cmd);
		rtCmdEnd(cmd);

		rt_timepoint rendered = rtQueueSubmit(queue, cmd);
		last_rendered = rendered;
		rtFramebufferDepthView(acquired.framebuffer, RT_NULL_HANDLE);
		rtSwapchainPresent(swapchain, rendered);

		fps_frames++;
		const auto fps_now = std::chrono::steady_clock::now();
		const std::chrono::duration<f32> fps_delta = fps_now - fps_time;
		if (fps_delta.count() >= 0.5f) {
			char title[96];
			const f32 fps = (f32)fps_frames / fps_delta.count();
			std::snprintf(title, sizeof(title), "Rutile Minecraft - %.0f FPS", fps);
			glfwSetWindowTitle(window, title);
			fps_time = fps_now;
			fps_frames = 0;
		}
	}

	rtCmdDestroy(cmd);
	rtGraphicsProgramDestroy(water_program);
	rtGraphicsProgramDestroy(graphics_program);
	rtBufferDestroy(water_transform_buffer);
	rtBufferDestroy(transform_buffer);
	rtBufferDestroy(water_vertex_buffer);
	rtBufferDestroy(vertex_buffer);
	rtTextureViewDestroy(depth_view);
	rtTextureDestroy(depth_texture);
	rtSwapchainDestroy(swapchain);
	state.swapchain.store(RT_NULL_HANDLE, std::memory_order_release);
	glfwDestroyWindow(window);
	glfwTerminate();
	rtExit();
	rtUnload();
	return 0;
}
