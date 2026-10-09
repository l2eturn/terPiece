// The open world and "the wilds" valley (new — not in the web build).
#include "world/overworld.h"
#include <cmath>
#include "art/glow.h"
#include "art/textures.h"
#include "world/cubeworld.h"
#include "world/layout.h"

using namespace layout;

namespace {

constexpr float WINTER_R = winter::WORLD_R;
constexpr float MEDIEVAL_R = medieval::WORLD_R;
// the wilds ground grid shares the snow village's grid (200 m / 240) so the seam has no cracks
constexpr float GRID = 200.0f / 240;
constexpr float X0 = -55, X1 = 55, Z0 = 60, Z1 = 192;

ObjPtr W(const GeoPtr& g, const MatPtr& m) { return makeMesh(geo::worldUV(g), m); }
MatPtr lam(const std::string& t, uint32_t color = 0xffffff) { return lambert(tex::get(t), hexc(color)); }

// does the snow village's ground mesh cover this point? (same test the village uses when it crops)
bool winterCovers(float x, float z) {
    if (std::abs(x) > 100 || std::abs(z) > 100) return false;
    return !(corridor(x, z) > 0.5f && std::hypot(x, z) > WINTER_R + 2);
}
// does the citadel's ground mesh cover this point, with some margin to spare
bool medievalCoversDeep(float x, float z) {
    for (float ox : {-1.2f, 1.2f})
        for (float oz : {-1.2f, 1.2f}) {
            glm::vec2 l = toLocal({x + ox, z + oz}, MEDIEVAL_OFFSET, MEDIEVAL_ROT);
            if (std::abs(l.x) > 90 || std::abs(l.y) > 90) return false;
            if (corridor(l.x, l.y) > 0.5f && std::hypot(l.x, l.y) > MEDIEVAL_R + 2) return false;
        }
    return true;
}

}  // namespace

Overworld::Overworld(WinterWorld& winter, MedievalWorld& medieval) : winter_(winter), medieval_(medieval) {
    name = "wilds";
    spawn = winter::SPAWN;
    group->name = "overworld";
    // the citadel stands south of the village, its gate facing north
    medieval_.group->position = {MEDIEVAL_OFFSET.x, 0, MEDIEVAL_OFFSET.y};
    medieval_.group->rotation.y = MEDIEVAL_ROT;
    group->add(winter_.group);
    group->add(medieval_.group);
    medieval_.setVisible(true);
    winter_.setVisible(true);

    buildTerrain();
    buildRoad();
    buildRiver();
    buildForest();
    buildCamp();
    buildBridge();
    placeBerries();

    wildsMap.type = "wilds";
    wildsMap.radius = 72;
    wildsMap.center = {0, 128};
    wildsMap.trees = &treeShapes_;
    std::vector<glm::vec2> road, river;
    for (float z = 76; z <= 180; z += 4) road.push_back({roadX(z), z});
    for (float z = RIVER_Z0; z <= RIVER_Z1; z += 4) river.push_back({riverX(z), z});
    // the footbridge shows as a short path across the river
    wildsMap.paths = {road, {{bridgeX0(), BRIDGE_Z}, {bridgeX1(), BRIDGE_Z}}};
    wildsMap.river = river;
    wildsMap.pond = SPRING_POND;
    wildsMap.lake = HARBOUR;
    env = winter_.env;
}

// ---------------------------------------------------------------------------
// areas
// ---------------------------------------------------------------------------

std::string Overworld::areaAt(float x, float z) const {
    if (winter::isInCave(x) && std::abs(z) < 60) return "winter";
    if (std::hypot(x, z) < WINTER_R) return "winter";
    glm::vec2 l = layout::toLocal({x, z}, MEDIEVAL_OFFSET, MEDIEVAL_ROT);
    if (std::hypot(l.x, l.y) < MEDIEVAL_R) return "medieval";
    return "wilds";
}

Overworld::Area Overworld::area(const std::string& n) const {
    if (n == "medieval") return {&medieval_, MEDIEVAL_OFFSET, MEDIEVAL_ROT};
    if (n == "winter") return {&winter_, {0, 0}, 0};
    return {const_cast<Overworld*>(this), {0, 0}, 0};
}

glm::vec3 Overworld::toLocal(const std::string& a, glm::vec3 p) const {
    Area ar = area(a);
    glm::vec2 l = layout::toLocal({p.x, p.z}, ar.offset, ar.rot);
    return {l.x, p.y, l.y};
}

glm::vec3 Overworld::toGlobal(const std::string& a, glm::vec3 p) const {
    Area ar = area(a);
    glm::vec2 g = layout::toGlobal({p.x, p.z}, ar.offset, ar.rot);
    return {g.x, p.y, g.y};
}

float Overworld::yawToLocal(const std::string& a, float yaw) const { return yaw - area(a).rot; }
float Overworld::yawToGlobal(const std::string& a, float yaw) const { return yaw + area(a).rot; }

float Overworld::southness(float z) const { return smoothstep(95, 165, z); }

// ---------------------------------------------------------------------------
// physics dispatch
// ---------------------------------------------------------------------------

float Overworld::groundAt(float x, float z, float y) const {
    std::string a = areaAt(x, z);
    if (a == "winter") return winter_.groundAt(x, z, y);
    if (a == "medieval") {
        glm::vec3 l = toLocal(a, {x, 0, z});
        return medieval_.groundAt(l.x, l.z, y);
    }
    return wildsGround(x, z, y);
}

float Overworld::wildsGround(float x, float z, float y) const {
    // the river is swimmable like the moat (you float, you do not walk on the bottom)
    float g = std::max(wildsHeight(x, z), WATER_Y - 0.55f);
    for (auto& p : platforms_)
        if (y >= p.top - 0.5f && std::hypot(x - p.x, z - p.z) < p.r + 0.2f) g = std::max(g, p.top);
    return extraGround(x, z, y, g);
}

void Overworld::wildsCollide(glm::vec3& pos, float radius) const {
    for (auto& c : colliders_) {
        if (c.removed || (skipTrees_ && c.tree)) continue;
        float dx = pos.x - c.x, dz = pos.z - c.z, d = std::hypot(dx, dz), m = c.r + radius;
        if (d < m && d > 1e-4f) {
            pos.x = c.x + dx / d * m;
            pos.z = c.z + dz / d * m;
        }
    }
    for (auto& p : platforms_) {
        if (p.thin || pos.y >= p.top - 0.45f) continue;
        float dx = pos.x - p.x, dz = pos.z - p.z, d = std::hypot(dx, dz), m = p.r + radius;
        if (d < m && d > 1e-4f) {
            pos.x = p.x + dx / d * m;
            pos.z = p.z + dz / d * m;
        }
    }
    for (auto& b : boxes_) {
        if (pos.y >= b.top - 0.45f) continue;
        float c = std::cos(b.rot), s = std::sin(b.rot);
        float dx = pos.x - b.x, dz = pos.z - b.z;
        float lx = dx * c - dz * s, lz = dx * s + dz * c;
        float hw = b.hw + radius, hd = b.hd + radius;
        if (std::abs(lx) < hw && std::abs(lz) < hd) {
            float nx = lx, nz = lz;
            if (hw - std::abs(lx) < hd - std::abs(lz)) nx = (lx > 0 ? 1 : lx < 0 ? -1 : 0) * hw;
            else nz = (lz > 0 ? 1 : lz < 0 ? -1 : 0) * hd;
            pos.x = b.x + nx * c + nz * s;
            pos.z = b.z - nx * s + nz * c;
        }
    }
    // the valley walls
    pos.x = std::clamp(pos.x, -VALLEY_HALF - 10, VALLEY_HALF + 10);
}

void Overworld::resolveCollisions(glm::vec3& pos, float radius) const {
    std::string a = areaAt(pos.x, pos.z);
    if (a == "winter") {
        winter_.resolveCollisions(pos, radius);
        // lamps / trees of the valley right by the seam
        if (std::hypot(pos.x, pos.z) > WINTER_R - 4 && !winter::isInCave(pos.x)) wildsCollide(pos, radius);
    } else if (a == "medieval") {
        glm::vec3 l = toLocal(a, pos);
        medieval_.resolveCollisions(l, radius);
        glm::vec3 g = toGlobal(a, l);
        pos.x = g.x;
        pos.z = g.z;
        if (std::hypot(l.x, l.z) > MEDIEVAL_R - 4) wildsCollide(pos, radius);
    } else {
        wildsCollide(pos, radius);
    }
}

bool Overworld::isIce(float x, float z) const { return areaAt(x, z) == "winter" && winter_.isIce(x, z); }

bool Overworld::isWater(float x, float z) const {
    std::string a = areaAt(x, z);
    if (a == "winter") return false;
    if (a == "medieval") {
        glm::vec3 l = toLocal(a, {x, 0, z});
        return medieval_.isWater(l.x, l.z);
    }
    return wildsHeight(x, z) < WATER_Y - 0.15f;
}

// the Silver River flows south (+Z): a gentle current plus a pull back towards the middle of the channel
bool Overworld::waterFlow(float x, float z, glm::vec2& flow, float& heading) const {
    if (areaAt(x, z) != "wilds" || z < RIVER_Z0 + 2 || z > RIVER_Z1 - 2) return false;
    float off = x - riverX(z);
    if (std::abs(off) > 6.5f) return false;
    // slope of the river line dx/dz
    float slope = 4.5f * 0.05f * std::cos(z * 0.05f + 0.8f);
    glm::vec2 dir = glm::normalize(glm::vec2(slope, 1));
    flow = dir * 0.7f + glm::vec2(-off * 0.25f, 0);
    heading = std::atan2(dir.x, dir.y);
    return true;
}

bool Overworld::isFree(float x, float z, float r, bool ignoreTrees) const {
    if (areaAt(x, z) != "wilds") return false;
    if (distToRoad(x, z) < r + 2.5f || riverDepthK(x, z) > 0 || std::abs(x) > VALLEY_HALF - 2) return false;
    if (nearBridge(x, z, r + 2.5f)) return false;
    for (auto s : berrySpots)
        if (std::hypot(x - s.x, z - s.y) < r + 1.5f) return false;
    if (std::abs(x - riverX(z)) < r + 6 && z > RIVER_Z0 - 8 && z < RIVER_Z1 + 8) return false;
    glm::vec3 p(x, wildsHeight(x, z), z);
    skipTrees_ = ignoreTrees;
    wildsCollide(p, r);
    skipTrees_ = false;
    return p.x == x && p.z == z;
}

std::string Overworld::zoneAt(float x, float z) const {
    std::string a = areaAt(x, z);
    if (a == "winter") return winter_.zoneAt(x, z);
    if (a == "medieval") {
        glm::vec3 l = toLocal(a, {x, 0, z});
        return medieval_.zoneAt(l.x, l.z);
    }
    if (std::hypot(x - SPRING_POND.x, z - SPRING_POND.y) < SPRING_POND.z + 4) return "spring";
    if (std::hypot(x - HARBOUR.x, z - HARBOUR.y) < HARBOUR.z + 4) return "harbour";
    if (std::hypot(x + 9, z - 128) < 9) return "camp";
    if (nearBridge(x, z, 1.5f)) return "bridge";
    if (std::abs(x - riverX(z)) < 8) return "riverside";
    if (distToRoad(x, z) < 6) return "oldroad";
    return z < 128 ? "pinewood" : "oakwood";
}

void Overworld::setTimeOfDay(const std::string& n) {
    timeOfDay = n;
    winter_.setTimeOfDay(n);
    medieval_.setTimeOfDay(n);
}

void Overworld::update(float t, float dt, glm::vec3 focus) {
    std::string a = areaAt(focus.x, focus.z);
    winter_.update(t, dt, focus);
    medieval_.update(t, dt, toLocal("medieval", focus));
    for (auto& u : updaters_) u(t, dt, focus);
    // weather / light: the village's in the village, the citadel's in the citadel, blended along the valley
    float k = a == "winter" ? 0.0f : a == "medieval" ? 1.0f : southness(focus.z);
    const Environment &A = winter_.env, &B = medieval_.env;
    env.hemiSky = glm::mix(A.hemiSky, B.hemiSky, k);
    env.hemiGround = glm::mix(A.hemiGround, B.hemiGround, k);
    env.hemiIntensity = glm::mix(A.hemiIntensity, B.hemiIntensity, k);
    env.sunColor = glm::mix(A.sunColor, B.sunColor, k);
    env.sunIntensity = glm::mix(A.sunIntensity, B.sunIntensity, k);
    env.fogColor = glm::mix(A.fogColor, B.fogColor, k);
    env.fogNear = glm::mix(A.fogNear, B.fogNear, k);
    env.fogFar = glm::mix(A.fogFar, B.fogFar, k);
    env.shadowHalf = glm::mix(A.shadowHalf, B.shadowHalf, k);
    env.shadowFar = glm::mix(A.shadowFar, B.shadowFar, k);
    glm::vec3 dA = glm::normalize(A.sunPosition - A.sunTarget), dB = glm::normalize(B.sunPosition - B.sunTarget);
    glm::vec3 dir = glm::normalize(glm::mix(dA, dB, k));
    env.sunPosition = focus + dir * 32.0f;
    env.sunTarget = focus;
    winter_.setSkyVisible(k < 0.5f);
    medieval_.setSkyVisible(k >= 0.5f);
    // road lamps follow the evening like the village lamps
    float lamps = timeOfDay == "day" ? 0.15f : timeOfDay == "dusk" ? 0.65f : 0.8f;
    for (auto& g : lampGlows_) {
        float& o = g->material()->opacity;
        o += (lamps - o) * std::min(1.0f, dt * 2);
    }
}

// ---------------------------------------------------------------------------
// building the wilds
// ---------------------------------------------------------------------------

void Overworld::buildTerrain() {
    auto g = std::make_shared<Geometry>();
    int nx = (int)std::round((X1 - X0) / GRID), nz = (int)std::round((Z1 - Z0) / GRID);
    // align the grid with the village's (-100 + k * GRID)
    float gx0 = -100 + std::round((X0 + 100) / GRID) * GRID, gz0 = -100 + std::round((Z0 + 100) / GRID) * GRID;
    const glm::vec3 snowPath = hexc(0xa8968c), grassPath = hexc(0xb7b2c2), bank = hexc(0xd8b88a), rockC = hexc(0x8d8794);
    ExtraAttribute blend{"blend", 1, {}};
    auto height = [&](float x, float z) {
        std::string a = areaAt(x, z);
        if (a == "winter") return winter::heightAt(x, z);
        if (a == "medieval") {
            glm::vec2 l = layout::toLocal({x, z}, MEDIEVAL_OFFSET, MEDIEVAL_ROT);
            return medieval::heightAt(l.x, l.y);
        }
        return wildsHeight(x, z);
    };
    for (int j = 0; j <= nz; ++j)
        for (int i = 0; i <= nx; ++i) {
            float x = gx0 + i * GRID, z = gz0 + j * GRID;
            g->position.push_back({x, height(x, z), z});
            g->uv.push_back({x, z});
            float s = southness(z);
            blend.data.push_back(s);
            // road tint (snowy path -> cobbles), sandy river banks
            float road = 1 - smoothstep(0.6f, 2.2f, distToRoad(x, z));
            glm::vec3 c = glm::mix(glm::vec3(1), glm::mix(snowPath, grassPath, s), road * glm::mix(0.8f, 0.9f, s));
            float rk = riverDepthK(x, z);
            float wet = smoothstep(0.0f, 0.3f, rk) * (1 - smoothstep(0.6f, 1.0f, rk));
            c = glm::mix(c, bank, wet * 0.8f * s + wet * 0.3f * (1 - s));
            float n = std::sin(x * 1.9f) * std::cos(z * 1.7f) * 0.025f;
            g->color.push_back(c + glm::vec3(n));
        }
    g->extra.push_back(blend);
    // same triangle pattern as PlaneGeometry: (a, b, d), (b, c, d)
    auto idx = [&](int i, int j) { return (uint32_t)(j * (nx + 1) + i); };
    for (int j = 0; j < nz; ++j)
        for (int i = 0; i < nx; ++i) {
            uint32_t a = idx(i, j), b = idx(i, j + 1), c = idx(i + 1, j + 1), d = idx(i + 1, j);
            for (auto tri : {std::array<uint32_t, 3>{a, b, d}, std::array<uint32_t, 3>{b, c, d}}) {
                glm::vec3 m = (g->position[tri[0]] + g->position[tri[1]] + g->position[tri[2]]) / 3.0f;
                if (winterCovers(m.x, m.z) || medievalCoversDeep(m.x, m.z)) continue;
                g->index.insert(g->index.end(), tri.begin(), tri.end());
            }
        }
    g->computeVertexNormals();
    // steep valley walls -> rock colour
    for (size_t i = 0; i < g->position.size(); ++i) {
        float k = 1 - smoothstep(0.55f, 0.8f, g->normal[i].y);
        if (k > 0) g->color[i] = glm::mix(g->color[i], rockC, k);
    }
    auto m = lambert(tex::get("snow"));
    m->map2 = tex::get("grass");
    m->vertexColors = true;
    // sits behind the towns' own ground where both overlap at the citadel seam
    m->polygonOffset = true;
    m->polygonOffsetFactor = 1;
    m->polygonOffsetUnits = 2;
    auto ground = makeMesh(g, m);
    ground->receiveShadow = true;
    addObject(ground);
}

void Overworld::buildRoad() {
    auto post = geo::cylinder(0.05f, 0.07f, 2.2f, 6);
    auto cap = geo::cone(0.22f, 0.2f, 4);
    auto bulbG = geo::sphere(0.11f, 10, 8);
    auto postMat = lambertColor(hexc(0x2b2840));
    auto bulbMat = basic(hexc(0xffe2a8));
    int k = 0;
    for (float z = 88; z <= 172; z += 14, ++k) {
        float side = k % 2 ? 2.2f : -2.2f;
        float x = roadX(z) + side;
        auto g = makeGroup("road-lamp");
        auto p = makeMesh(post, postMat);
        p->position.y = 1.1f;
        p->castShadow = true;
        auto b = makeMesh(bulbG, bulbMat);
        b->position.y = 2.2f;
        auto c = makeMesh(cap, postMat);
        c->position.y = 2.38f;
        c->rotation.y = PI / 4;
        auto glow = glowSprite(hexc(0xffc27a), 1.8f, 0.65f);
        glow->position.y = 2.2f;
        lampGlows_.push_back(glow);
        g->add(p, b, c, glow);
        CwTag tag;
        tag.name = "street-light";
        tag.h = 2.55f;
        g->cw.push_back(tag);
        g->position = {x, wildsHeight(x, z), z};
        addObject(g);
        colliders_.push_back({x, z, 0.18f});
    }
    // signposts at both ends
    auto wood = lam("planks", 0x9a7058);
    auto sign = [&](float x, float z, float yaw) {
        auto g = makeGroup("signpost");
        auto pole = W(geo::box(0.14f, 1.6f, 0.14f), wood);
        pole->position.y = 0.8f;
        auto board = W(geo::box(1.2f, 0.32f, 0.08f), wood);
        board->position = {0.35f, 1.35f, 0};
        auto board2 = W(geo::box(1.0f, 0.28f, 0.08f), wood);
        board2->position = {-0.3f, 1.0f, 0};
        board2->rotation.y = 0.5f;
        g->add(pole, board, board2);
        g->traverse([](Object3D& o) {
            if (o.kind == ObjectKind::Mesh) o.castShadow = true;
        });
        CwTag tag;
        tag.name = "sign";
        tag.h = 1.3f;
        g->cw.push_back(tag);
        g->position = {x, wildsHeight(x, z), z};
        g->rotation.y = yaw;
        addObject(g);
        colliders_.push_back({x, z, 0.3f});
    };
    sign(roadX(84) - 3.2f, 84, 0);
    sign(roadX(172) + 3.2f, 172, PI);
}

void Overworld::buildRiver() {
    // one water sheet under the whole river area — it only shows where the ground dips into the channel
    auto wm = lambert(tex::get("water"));
    wm->transparent = true;
    wm->opacity = 0.88f;
    auto pg = geo::plane(30, RIVER_Z1 - RIVER_Z0 + 24);
    pg->rotateX(-PI / 2).translate(18, WATER_Y, (RIVER_Z0 + RIVER_Z1) / 2);
    addObject(makeMesh(geo::worldUV(pg, 0.5f), wm));

    // jetties: from the road side out into the spring pond and the harbour
    auto deckMat = lam("planks", 0x6e5240);
    auto jetty = [&](glm::vec3 lake, glm::vec3& dock) {
        float x0 = lake.x - lake.z - 1.5f, x1 = lake.x - 1.2f, z = lake.y;
        float len = x1 - x0;
        auto deck = W(geo::box(len, 0.18f, 1.8f), deckMat);
        deck->position = {(x0 + x1) / 2, 0.22f, z};
        deck->castShadow = deck->receiveShadow = true;
        addObject(deck);
        for (float px = x0 + 0.3f; px <= x1; px += 1.6f)
            for (float pz : {-0.8f, 0.8f}) {
                auto p = W(geo::cylinder(0.1f, 0.1f, 1.9f, 6), deckMat);
                p->position = {px, -0.5f, z + pz};
                addObject(p);
            }
        for (float px = x0 + 0.4f; px <= x1; px += 0.9f) platforms_.push_back({px, z, 0.85f, 0.31f, true});
        dock = {x1, 0.31f, z};
        // a torch at the end of the jetty
        auto glow = glowSprite(hexc(0xff9f3b), 1.8f, 0.85f);
        glow->position = {x1 - 0.4f, 1.85f, z + 1.0f};
        addObject(glow);
        lampGlows_.push_back(glow);
        auto light = makePointLight(hexc(0xffa244), 0.75f, 8, 2);
        light->position = {x1 - 0.4f, 1.9f, z + 1.0f};
        addObject(light);
    };
    jetty(SPRING_POND, springDock);
    jetty(HARBOUR, harbourDock);
    // the boat waits at the village end, pointing downstream
    springBoat = {springDock.x + 1.3f, springDock.z};
    springBoatHeading = PI / 2;
}

void Overworld::buildForest() {
    // pines near the snow, oaks towards the citadel
    std::vector<GeoPtr> parts;
    std::vector<int> kinds = {0};
    auto addPart = [&](GeoPtr g, float y) {
        g->translate(0, y, 0);
        parts.push_back(geo::worldUV(g, 1.4f));
    };
    addPart(geo::cylinder(0.16f, 0.22f, 0.9f, 6), 0.45f);
    const float tiers[4][3] = {{1.5f, 1.6f, 1.3f}, {1.15f, 1.4f, 2.2f}, {0.8f, 1.2f, 3.0f}, {0.45f, 0.9f, 3.65f}};
    for (auto& t : tiers) {
        addPart(geo::cone(t[0], t[1], 8), t[2]);
        kinds.push_back(1);
        addPart(geo::cone(t[0] * 0.55f, t[1] * 0.45f, 8), t[2] + t[1] * 0.34f);
        kinds.push_back(2);
    }
    auto pineGeo = geo::merge(parts, true);
    for (size_t i = 0; i < pineGeo->groups.size(); ++i) pineGeo->groups[i].materialIndex = kinds[i];
    std::vector<MatPtr> pineMat = {lam("bark"), lam("needles"), lam("snow")};

    struct Spot {
        float x, z, s, rot;
        bool pine;
    };
    std::vector<Spot> spots;
    for (int tries = 0; spots.size() < 150 && tries < 6000; ++tries) {
        float x = (rand_.f() * 2 - 1) * (VALLEY_HALF + 6), z = 78 + rand_.f() * 100;
        if (areaAt(x, z) != "wilds") continue;
        if (distToRoad(x, z) < 4 || riverDepthK(x, z) > 0 || std::abs(x - riverX(z)) < 7.5f) continue;
        if (std::hypot(x - SPRING_POND.x, z - SPRING_POND.y) < SPRING_POND.z + 4) continue;
        if (std::hypot(x - HARBOUR.x, z - HARBOUR.y) < HARBOUR.z + 4) continue;
        // the camp
        if (std::hypot(x + 9, z - 128) < 7) continue;
        bool bad = false;
        for (auto& s : spots)
            if (std::hypot(s.x - x, s.z - z) < 3.2f) {
                bad = true;
                break;
            }
        if (bad) continue;
        // pines give way to oaks between z 120 and 140
        bool pine = z < 120 + rand_.f() * 20;
        spots.push_back({x, z, 0.8f + rand_.f() * 0.6f, rand_.f() * PI, pine});
    }
    int nPine = 0;
    for (auto& s : spots) nPine += s.pine;
    auto pines = makeInstanced(pineGeo, pineMat, nPine);
    Object3D* pinePtr = pines.get();
    auto crownMat = lam("needles", 0x3f7a42);
    auto trunkGeo = geo::worldUV(geo::cylinder(0.35f, 0.5f, 2.2f, 8), 1.2f);
    trunkGeo->translate(0, 1.1f, 0);
    auto trunkMat = lam("bark");
    int pi = 0;
    for (auto& s : spots) {
        float y = wildsHeight(s.x, s.z);
        TreeRef ref;
        ref.x = s.x;
        ref.z = s.z;
        if (s.pine) {
            int i = pi++;
            pines->instances[i] = composeMatrix({s.x, y - 0.25f, s.z}, glm::angleAxis(s.rot, glm::vec3(0, 1, 0)), glm::vec3(s.s));
            treeShapes_.push_back({s.x, s.z, y - 0.25f + 0.6f * s.s, 4.0f * s.s, 1.45f * s.s});
            colliders_.push_back({s.x, s.z, 0.45f * s.s + 0.15f, true});
            ref.hide = [pinePtr, i] {
                pinePtr->instances[i] = glm::mat4(0);
                pinePtr->instancesDirty = true;
            };
        } else {
            auto tree = makeGroup("oak");
            float sc = s.s;
            auto trunk = makeMesh(trunkGeo, trunkMat);
            auto c1 = W(geo::dodecahedron(1.9f * sc, 1), crownMat);
            c1->position.y = 2.4f * sc;
            auto c2 = W(geo::dodecahedron(1.5f * sc, 1), crownMat);
            c2->position = {0.4f * sc, 3.4f * sc, 0.3f * sc};
            auto c3 = W(geo::dodecahedron(1.2f * sc, 1), crownMat);
            c3->position = {-0.3f * sc, 4.0f * sc, -0.2f * sc};
            tree->add(trunk, c1, c2, c3);
            tree->traverse([](Object3D& o) {
                if (o.kind == ObjectKind::Mesh) o.castShadow = o.receiveShadow = true;
            });
            auto tag = [](const char* n, float h, float wd, glm::vec3 off) {
                CwTag t;
                t.name = n;
                t.h = h;
                t.w = wd;
                t.offset = off;
                return t;
            };
            tree->cw = {tag("trunk", 3.0f, 0, {0, 0, 0}), tag("tree-leaves", 0, 3.6f, {0, 2.6f, 0}), tag("leaves", 0, 2.6f, {0.9f, 3.4f, 0.5f}),
                        tag("tree-leaves", 0, 2.2f, {-0.7f, 4.0f, -0.4f})};
            tree->rotation.y = s.rot;
            tree->position = {s.x, y, s.z};
            tree->scale = glm::vec3(sc);
            addObject(tree);
            treeShapes_.push_back({s.x, s.z, y, 4.5f * sc, 1.8f * sc});
            colliders_.push_back({s.x, s.z, 0.5f * sc, true});
            Object3D* tp = tree.get();
            ref.hide = [tp] { tp->visible = false; };
        }
        ref.collider = (int)colliders_.size() - 1;
        ref.shape = (int)treeShapes_.size() - 1;
        trees_.push_back(ref);
    }
    pines->castShadow = pines->receiveShadow = true;
    // Cube World pines, snow-dusted
    CwTag tag;
    tag.name = "fir-tree";
    tag.h = 4.6f;
    tag.snow = true;
    pines->cw.push_back(tag);
    addObject(pines);
}

// a campfire half-way with log benches
void Overworld::buildCamp() {
    float cx = -9, cz = 128, cy = wildsHeight(cx, cz);
    addObject(CubeWorld::createFire({cx, cy + 0.3f, cz}));
    // ring of stones
    auto stoneMat = lam("stone");
    for (int i = 0; i < 8; ++i) {
        float a = i / 8.0f * PI * 2;
        auto st = W(geo::dodecahedron(0.18f), stoneMat);
        st->position = {cx + std::cos(a) * 0.55f, cy + 0.08f, cz + std::sin(a) * 0.55f};
        addObject(st);
    }
    colliders_.push_back({cx, cz, 0.6f});
    // log benches facing the fire
    auto logMat = lam("bark");
    for (int i = 0; i < 3; ++i) {
        float a = i / 3.0f * PI * 2 + 0.4f;
        float bx = cx + std::cos(a) * 2.4f, bz = cz + std::sin(a) * 2.4f;
        auto lg = geo::cylinder(0.22f, 0.22f, 1.6f, 8);
        lg->rotateZ(PI / 2);
        auto log = W(lg, logMat);
        log->position = {bx, wildsHeight(bx, bz) + 0.22f, bz};
        log->rotation.y = -a + PI / 2;
        log->castShadow = log->receiveShadow = true;
        CwTag tag;
        tag.name = "bench";
        tag.h = 0.5f;
        tag.rotY = PI / 2;
        log->cw.push_back(tag);
        addObject(log);
        platforms_.push_back({bx, bz, 0.45f, wildsHeight(bx, bz) + 0.44f});
    }
}

// ---------------------------------------------------------------------------
// the footbridge across the river
// ---------------------------------------------------------------------------

float Overworld::bridgeDeck(float x) const {
    float x0 = bridgeX0(), x1 = bridgeX1();
    float t = std::clamp((x - x0) / (x1 - x0), 0.0f, 1.0f);
    float base = glm::mix(wildsHeight(x0, BRIDGE_Z), wildsHeight(x1, BRIDGE_Z), t);
    return base + BRIDGE_RISE * std::sin(PI * t) + 0.12f;
}

void Overworld::buildBridge() {
    const float x0 = bridgeX0(), x1 = bridgeX1(), z = BRIDGE_Z;
    const int N = 18;
    const float seg = (x1 - x0) / N;
    auto deckMat = lam("planks", 0x8a6448);
    auto railMat = lam("planks", 0x6e5240);
    auto bridge = makeGroup("bridge");
    // the deck: short sloped plank sections following the arch
    for (int i = 0; i < N; ++i) {
        float xa = x0 + i * seg, xb = xa + seg;
        float ya = bridgeDeck(xa), yb = bridgeDeck(xb);
        float len = std::hypot(xb - xa, yb - ya);
        auto d = W(geo::box(len + 0.04f, 0.16f, BRIDGE_HALF_W * 2), deckMat);
        d->position = {(xa + xb) / 2, (ya + yb) / 2 - 0.08f, z};
        d->rotation.z = std::atan2(yb - ya, xb - xa);
        bridge->add(d);
    }
    // posts every few sections and a hand rail on both sides
    for (float side : {-1.0f, 1.0f}) {
        float rz = z + side * (BRIDGE_HALF_W - 0.08f);
        for (int i = 0; i <= N; i += 3) {
            float px = x0 + i * seg, py = bridgeDeck(px);
            auto post = W(geo::box(0.12f, 0.95f, 0.12f), railMat);
            post->position = {px, py + 0.4f, rz};
            bridge->add(post);
            if (i + 3 > N) break;
            float qx = px + 3 * seg, qy = bridgeDeck(qx);
            float len = std::hypot(qx - px, qy - py);
            auto rail = W(geo::box(len, 0.08f, 0.1f), railMat);
            rail->position = {(px + qx) / 2, (py + qy) / 2 + 0.85f, rz};
            rail->rotation.z = std::atan2(qy - py, qx - px);
            bridge->add(rail);
        }
    }
    // piers standing in the river under the arch
    for (float k : {-0.45f, 0.45f}) {
        float px = riverX(z) + k * BRIDGE_HALF_LEN, top = bridgeDeck(px) - 0.16f;
        for (float side : {-1.0f, 1.0f}) {
            float h = top + 1.8f;
            auto pier = W(geo::cylinder(0.14f, 0.16f, h, 6), railMat);
            pier->position = {px, top - h / 2, z + side * (BRIDGE_HALF_W - 0.25f)};
            bridge->add(pier);
        }
    }
    bridge->traverse([](Object3D& o) {
        if (o.kind == ObjectKind::Mesh) o.castShadow = o.receiveShadow = true;
    });
    addObject(bridge);
    // walkable from either bank; swimmers and boats pass underneath (only counts when you are near the deck)
    addGround([this](float x, float z, float y) {
        if (!nearBridge(x, z)) return -INFINITY;
        float d = bridgeDeck(x);
        return y >= d - 0.6f ? d : -INFINITY;
    });
    // no trees on the bridge heads
    clearTrees(x0, z, 3);
    clearTrees(x1, z, 3);
}

// ---------------------------------------------------------------------------
// berry bushes in the wilds — half of them across the river (cross the bridge to reach them)
// ---------------------------------------------------------------------------

void Overworld::placeBerries() {
    Rng rand(4242);
    auto ok = [&](float x, float z) {
        if (areaAt(x, z) != "wilds" || std::abs(x) > VALLEY_HALF - 3) return false;
        if (distToRoad(x, z) < 3.5f || riverDepthK(x, z) > 0 || std::abs(x - riverX(z)) < 6) return false;
        if (std::hypot(x - SPRING_POND.x, z - SPRING_POND.y) < SPRING_POND.z + 4) return false;
        if (std::hypot(x - HARBOUR.x, z - HARBOUR.y) < HARBOUR.z + 4) return false;
        if (std::hypot(x + 9, z - 128) < 5 || nearBridge(x, z, 2.5f)) return false;
        for (auto& c : colliders_)
            if (!c.removed && std::hypot(x - c.x, z - c.z) < c.r + 1.2f) return false;
        for (auto s : berrySpots)
            if (std::hypot(x - s.x, z - s.y) < 4) return false;
        return true;
    };
    // across the river: between the river and the east valley wall
    for (int tries = 0, n = 0; n < 8 && tries < 4000; ++tries) {
        float z = 96 + rand.f() * 72, x = riverX(z) + 6 + rand.f() * (VALLEY_HALF - 3 - riverX(z) - 6);
        if (!ok(x, z)) continue;
        berrySpots.push_back({x, z});
        ++n;
    }
    // the west side of the valley, along the road and in the woods
    for (int tries = 0, n = 0; n < 8 && tries < 4000; ++tries) {
        float z = 84 + rand.f() * 90, x = -(VALLEY_HALF - 3) + rand.f() * (VALLEY_HALF - 3 + roadX(z) + 8);
        if (!ok(x, z)) continue;
        berrySpots.push_back({x, z});
        ++n;
    }
}
