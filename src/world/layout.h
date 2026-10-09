#pragma once
// Layout of the open world: the snow village sits at the origin, the medieval citadel lies to the south
// (turned around so its town gate faces the village), and "the wilds" — a valley with a road and a river —
// joins the two. All three share one global coordinate system; each town keeps its own local coordinates
// (so the numbers ported from world.js / medieval.js stay unchanged).
#include <cmath>
#include <glm/glm.hpp>
#include "core/math.h"

namespace layout {

// medieval town: centre in global space and its yaw (PI = local +Z, the gate side, points at the village)
const glm::vec2 MEDIEVAL_OFFSET(0, 250);
constexpr float MEDIEVAL_ROT = PI;

// local (town) -> global, same convention as Object3D rotation.y
inline glm::vec2 toGlobal(glm::vec2 local, glm::vec2 offset, float rot) {
    float c = std::cos(rot), s = std::sin(rot);
    return {offset.x + local.x * c + local.y * s, offset.y - local.x * s + local.y * c};
}
inline glm::vec2 toLocal(glm::vec2 global, glm::vec2 offset, float rot) {
    float c = std::cos(rot), s = std::sin(rot);
    float dx = global.x - offset.x, dz = global.y - offset.y;
    return {dx * c - dz * s, dx * s + dz * c};
}
inline glm::vec2 medievalToGlobal(glm::vec2 l) { return toGlobal(l, MEDIEVAL_OFFSET, MEDIEVAL_ROT); }

// The opening in each town's rim where the valley comes in: town-local +Z, |x| below ~22 m.
// Returns 1 inside the opening, 0 outside (smooth edge).
inline float corridor(float lx, float lz) {
    if (lz <= 0) return 0;
    return 1 - smoothstep(16, 26, std::abs(lx));
}

// ---------- the wilds ----------
// the old road: wiggles gently from the village gate (0, 80) to the town gate (0, 175)
inline float roadX(float z) { return 4.0f * std::sin((z - 80) * 0.045f); }
// the river: east of the road, from the spring pond to the harbour
inline float riverX(float z) { return 17.0f + 4.5f * std::sin(z * 0.05f + 0.8f); }
constexpr float RIVER_Z0 = 88, RIVER_Z1 = 172;
// spring pond with the first jetty (village end) and the harbour (citadel end)
const glm::vec3 SPRING_POND(18, 90, 7);
const glm::vec3 HARBOUR(18, 168, 7.5f);
constexpr float WATER_Y = -0.45f;
// valley walls
constexpr float VALLEY_HALF = 34;

inline float distToRoad(float x, float z) { return std::abs(x - roadX(z)); }

// the footbridge: an arched wooden bridge across the river, by the camp (runs along x at BRIDGE_Z)
constexpr float BRIDGE_Z = 128, BRIDGE_HALF_LEN = 7.5f, BRIDGE_HALF_W = 1.2f, BRIDGE_RISE = 2.0f;
inline float bridgeX0() { return riverX(BRIDGE_Z) - BRIDGE_HALF_LEN; }
inline float bridgeX1() { return riverX(BRIDGE_Z) + BRIDGE_HALF_LEN; }
// is a point within `margin` of the bridge deck (in plan)?
inline bool nearBridge(float x, float z, float margin = 0) {
    return x > bridgeX0() - margin && x < bridgeX1() + margin && std::abs(z - BRIDGE_Z) < BRIDGE_HALF_W + margin;
}

inline float riverDepthK(float x, float z) {
    // 1 in the river channel / the two lakes, 0 on dry land
    float k = 0;
    if (z > RIVER_Z0 && z < RIVER_Z1) k = 1 - smoothstep(3.4f, 6.5f, std::abs(x - riverX(z)));
    k = std::max(k, 1 - smoothstep(SPRING_POND.z - 2, SPRING_POND.z + 1.5f, std::hypot(x - SPRING_POND.x, z - SPRING_POND.y)));
    k = std::max(k, 1 - smoothstep(HARBOUR.z - 2, HARBOUR.z + 1.5f, std::hypot(x - HARBOUR.x, z - HARBOUR.y)));
    return k;
}

// height of the wilds terrain at a global point (also used by both towns where their rim opens)
inline float wildsHeight(float x, float z) {
    // rolling meadow that never dips below the water (so no stray puddles)
    float h = 0.35f + 0.35f * (0.5f + 0.5f * std::sin(x * 0.11f + 0.4f) * std::cos(z * 0.07f)) + 0.25f * (0.5f + 0.5f * std::sin((x - z) * 0.05f));
    // a few soft hills away from the road
    h += 1.6f * std::exp(-((x + 20) * (x + 20) + (z - 120) * (z - 120)) / 90.0f);
    h += 1.2f * std::exp(-((x - 30) * (x - 30) + (z - 140) * (z - 140)) / 60.0f);
    // the valley walls
    float e = std::abs(x);
    if (e > VALLEY_HALF - 4) h += std::pow(e - (VALLEY_HALF - 4), 1.25f) * 0.32f;
    // flat road
    h = glm::mix(0.15f, h, smoothstep(1.8f, 5.0f, distToRoad(x, z)));
    // river channel
    float k = riverDepthK(x, z);
    h = glm::mix(h, -1.6f, k);
    return h;
}

}  // namespace layout
