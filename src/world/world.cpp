#include "world/world.h"
#include <cmath>

float distToSegment(glm::vec2 p, glm::vec2 a, glm::vec2 b) {
    glm::vec2 d = b - a;
    float t = std::clamp(glm::dot(p - a, d) / glm::dot(d, d), 0.0f, 1.0f);
    return glm::length(p - (a + t * d));
}

float distToPaths(const std::vector<std::vector<glm::vec2>>& paths, float x, float z) {
    float best = INFINITY;
    for (auto& p : paths)
        for (size_t i = 0; i + 1 < p.size(); ++i) best = std::min(best, distToSegment({x, z}, p[i], p[i + 1]));
    return best;
}

void World::clearTrees(float x, float z, float r) {
    for (auto& t : trees_) {
        if (t.gone || std::hypot(t.x - x, t.z - z) > r) continue;
        t.gone = true;
        if (t.hide) t.hide();
        if (t.collider >= 0) colliders_[t.collider].removed = true;
        if (t.shape >= 0) treeShapes_[t.shape].removed = true;
    }
}

float World::extraGround(float x, float z, float y, float g) const {
    for (auto& f : groundFns_) g = std::max(g, f(x, z, y));
    return g;
}

bool World::blocksView(float px, float py, float pz) const {
    for (auto& t : treeShapes_) {
        if (t.removed) continue;
        float dx = px - t.x, dz = pz - t.z;
        if (dx * dx + dz * dz > t.r * t.r) continue;
        float k = (py - t.base) / t.h;
        if (k < 0 || k > 1) continue;
        if (std::hypot(dx, dz) < t.r * (1 - k * 0.7f) + 0.25f) return true;
    }
    for (auto& b : boxes_) {
        if (py > b.top) continue;
        float c = std::cos(b.rot), s = std::sin(b.rot);
        float dx = px - b.x, dz = pz - b.z;
        if (std::abs(dx * c - dz * s) < b.hw && std::abs(dx * s + dz * c) < b.hd) return true;
    }
    return false;
}

float World::cameraClearance(glm::vec3 from, glm::vec3 to) const {
    const int steps = 14;
    for (int i = 1; i <= steps; ++i) {
        glm::vec3 p = glm::mix(from, to, (float)i / steps);
        if (blocksView(p.x, p.y, p.z)) return std::max(0.12f, (float)(i - 1) / steps);
    }
    return 1;
}
