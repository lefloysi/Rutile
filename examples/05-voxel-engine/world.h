#pragma once

#include "rutile.h"

#include <vector>

struct Vertex {
	f32 position[3];
	f32 color[3];
	f32 normal[3];
	f32 ao;
	f32 pixel_uv[2];
	f32 edge_mask;
	f32 corner_mask;
};

std::vector<Vertex> build_world_mesh();
std::vector<Vertex> build_water_mesh();
