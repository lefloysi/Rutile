#include "world.h"

#include <glm/glm.hpp>

constexpr i32 kChunkSize = 16;
constexpr i32 kChunkCountX = 8;
constexpr i32 kChunkCountZ = 8;
constexpr i32 kWorldX = kChunkSize * kChunkCountX;
constexpr i32 kWorldY = 24;
constexpr i32 kWorldZ = kChunkSize * kChunkCountZ;
constexpr f32 kWaterLevel = 7.35f;

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

void push_triangle(std::vector<Vertex>* vertices, const glm::vec3& origin, const glm::vec3& color, const glm::vec3& normal, const glm::vec3& a, f32 ao_a, const glm::vec2& uv_a, const glm::vec3& b, f32 ao_b, const glm::vec2& uv_b, const glm::vec3& c, f32 ao_c, const glm::vec2& uv_c, f32 edge_mask, f32 corner_mask) {
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

	const glm::vec3 px[] = { glm::vec3(1, 0, 1), glm::vec3(1, 0, 0), glm::vec3(1, 1, 0), glm::vec3(1, 1, 1) };
	const glm::vec3 nx[] = { glm::vec3(0, 0, 0), glm::vec3(0, 0, 1), glm::vec3(0, 1, 1), glm::vec3(0, 1, 0) };
	const glm::vec3 py[] = { glm::vec3(0, 1, 1), glm::vec3(1, 1, 1), glm::vec3(1, 1, 0), glm::vec3(0, 1, 0) };
	const glm::vec3 ny[] = { glm::vec3(0, 0, 0), glm::vec3(1, 0, 0), glm::vec3(1, 0, 1), glm::vec3(0, 0, 1) };
	const glm::vec3 pz[] = { glm::vec3(0, 0, 1), glm::vec3(1, 0, 1), glm::vec3(1, 1, 1), glm::vec3(0, 1, 1) };
	const glm::vec3 nz[] = { glm::vec3(1, 0, 0), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0), glm::vec3(1, 1, 0) };

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
