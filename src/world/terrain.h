#pragma once
// Block terrain (new — not in the web build): the ground of the whole open world as one grid of
// layout::BLOCK columns, drawn Cube World style. Flat runs of equal blocks are merged into big quads
// (greedy meshing) so the mesh stays small; a column edge only gets side faces where its neighbour is lower.
#include <functional>
#include <glm/glm.hpp>
#include "scene/object.h"
#include "world/world.h"

struct TerrainSource {
    // global area to cover: x0, z0, x1, z1
    glm::vec4 bounds{0};
    // does the world have ground at this column? (outside: the edge of the world, walls go down to the base)
    std::function<bool(float x, float z)> inside;
    // ground height (already in whole blocks) and top surface at a column centre, global coordinates
    std::function<float(float x, float z)> height;
    std::function<Surface(float x, float z)> surface;
};

ObjPtr buildBlockTerrain(const TerrainSource& src);
