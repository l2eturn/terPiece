// Port of patty-project/xmas-app/web/js/cubeworld.js
#include "world/cubeworld.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <nlohmann/json.hpp>
#include "core/math.h"
#include "scene/gltf.h"
#include "world/world.h"

using nlohmann::json;

CubeWorld& cubeWorld() {
    static CubeWorld cw;
    return cw;
}

namespace {

// ---------- decorations: each group scatters items around `at` inside the ring r = [inner, outer] ----------
// h = height in metres, n = count, solid = collide + stand on, face = turn towards `at`,
// perch = sit on top of an already placed item (by model name), water = float on the surface, fire = flames + light
struct Item {
    std::string name;
    float h;
    int n = 1;
    bool solid = false, face = false, water = false, fire = false;
    std::vector<std::string> perch;
};
struct Group {
    glm::vec2 at, r;
    std::vector<Item> items;
};

Item I(std::string name, float h, int n = 1) { return Item{std::move(name), h, n}; }
Item solid(Item i) {
    i.solid = true;
    return i;
}
Item face(Item i) {
    i.face = true;
    return i;
}

std::vector<Group> layout(const std::string& world) {
    std::vector<Group> L;
    if (world == "winter") {
        // campfire + benches beside the square (placed before the market so they keep their spot)
        Item fire = I("campfire", 0.75f);
        fire.fire = true;
        L.push_back({{7.5f, 12.5f}, {0, 0.5f}, {fire}});
        L.push_back({{7.5f, 12.5f}, {2.6f, 3.2f}, {solid(face(I("bench", 0.5f, 3)))}});
        // Christmas market around the tree
        L.push_back({{0, 0}, {8.5f, 13}, {solid(face(I("market-stand1", 2.7f))), solid(face(I("market-stand2", 2.7f))),
                                         solid(face(I("market-stand3", 2.7f))), solid(I("barrel", 0.95f, 3)), solid(I("crate", 0.8f, 2)),
                                         I("sack", 0.7f, 2)}});
        // a small Christmas tree beside every cabin
        for (glm::vec2 at : {glm::vec2(-16, -12), glm::vec2(16, -15), glm::vec2(-19, 7), glm::vec2(17, 12)})
            L.push_back({at, {4.5f, 7}, {solid(I("christmas-tree", 2.6f))}});
        L.push_back({{0, 36}, {3.5f, 5}, {solid(face(I("sign", 1.3f)))}});
        // snowy woods
        L.push_back({{0, 0}, {12, 75}, {I("snow-bush", 0.8f, 34), I("heartflower-frozen", 0.45f, 22), I("snow-berry", 0.35f, 14), I("stone", 0.18f, 12)}});
        Item b1 = I("tiny-bird1", 0.38f, 2), b2 = I("tiny-bird2", 0.38f, 2);
        b1.perch = {"barrel", "crate"};
        b2.perch = {"christmas-tree", "barrel"};
        L.push_back({{0, 0}, {8, 20}, {b1, b2}});
    } else if (world == "medieval") {
        // market square around the fountain (the four stalls in medieval.cpp are swapped through tags)
        L.push_back({{0, 8}, {7, 15}, {I("fruit-basket", 0.55f, 3), solid(I("barrel", 0.95f, 3)), solid(I("open-crate", 0.45f, 2)), I("flower-vase1", 0.6f, 2)}});
        // in front of the tavern
        L.push_back({{-24, 6}, {4.5f, 9}, {solid(I("barrel", 0.95f, 4)), solid(I("crate", 0.8f, 3)), solid(I("wagon2", 2.0f)), solid(I("horse-trough1", 0.6f)), I("flower-box01", 0.5f, 2)}});
        // training grounds: soldier tents
        L.push_back({{23, 5}, {8, 14}, {solid(face(I("soldier-tent", 2.6f))), solid(face(I("tent", 1.9f))), solid(I("sawbench", 0.9f))}});
        // flower garden
        L.push_back({{20, 27}, {5, 12}, {I("sunflower", 1.4f, 6), I("tulips-colorful", 0.7f, 5), I("echinacea", 0.9f, 4), I("lily", 0.7f, 3)}});
        // swan lake: floating lilies + reeds
        Item w1 = I("water-lily01", 0.25f, 4), w2 = I("water-lily02", 0.15f, 6);
        w1.water = w2.water = true;
        L.push_back({{-40, -26}, {0, 10}, {w1, w2}});
        L.push_back({{-40, -26}, {10, 15}, {I("reed", 1.1f, 10)}});
        // pumpkin patch + scarecrow
        L.push_back({{36, 34}, {0, 7}, {solid(I("scarecrow", 2.0f)), solid(I("big-pumpkin", 0.9f, 2)), solid(I("pumpkin", 0.6f, 6)), I("wheat", 0.9f, 8)}});
        // meadows
        L.push_back({{0, 8}, {16, 62}, {I("bush", 0.9f, 16), I("flowers", 0.3f, 14), I("flowers2", 0.3f, 14), I("flowers3", 0.3f, 14), I("mushroom", 0.35f, 8), I("stone2", 0.2f, 8)}});
        Item b1 = I("tiny-bird1", 0.38f, 2), b2 = I("tiny-bird2", 0.38f, 2);
        b1.perch = {"barrel", "open-crate"};
        b2.perch = {"barrel", "crate"};
        L.push_back({{0, 8}, {5, 20}, {b1, b2}});
    } else if (world == "wilds") {
        // the valley between the two towns (new): snowy bushes up north, flowers down south
        L.push_back({{0, 100}, {4, 22}, {I("snow-bush", 0.8f, 14), I("heartflower-frozen", 0.45f, 10), I("snow-berry", 0.35f, 8), I("stone", 0.18f, 8)}});
        L.push_back({{0, 155}, {4, 24}, {I("bush", 0.9f, 10), I("flowers", 0.3f, 12), I("flowers2", 0.3f, 12), I("flowers3", 0.3f, 10),
                                         I("mushroom", 0.35f, 6), I("stone2", 0.2f, 6)}});
        // rest stop by the campfire
        L.push_back({{-9, 128}, {3.4f, 5.5f}, {solid(I("barrel", 0.95f, 2)), solid(I("crate", 0.8f, 2)), I("sack", 0.7f, 2)}});
        // reeds along the river and at the two lakes
        L.push_back({{18, 90}, {7.5f, 10}, {I("reed", 1.1f, 8)}});
        L.push_back({{18, 168}, {8, 11}, {I("reed", 1.1f, 8)}});
    }
    return L;
}

// ---------- animals (head / body / feet parts) ----------
// u = size of one voxel in metres; models face -Z
struct RideSpec {
    float walk, run, jump;
};
struct AnimalSpec {
    float u;
    std::string body, head, foot, hand, tail;
    bool biped = false;
    float speed;
    bool rideable = false;
    RideSpec ride{};
};
const std::map<std::string, AnimalSpec>& animals() {
    static const std::map<std::string, AnimalSpec> A = {
        {"penguin", {0.04f, "penguin-body", "penguin-head", "penguin-foot", "penguin-hand", "", true, 0.5f}},
        {"sheep", {0.05f, "sheep-body", "sheep-head", "sheep-foot", "", "", false, 0.6f}},
        {"pig", {0.045f, "pig-body", "pig-head", "pig-foot", "", "", false, 0.6f}},
        {"chicken", {0.04f, "chicken-body", "chicken-head", "chicken-foot", "chicken-hand", "", true, 0.8f}},
        {"cat", {0.035f, "cat-body", "cat-head", "cat-foot", "cat-hand", "cat-tail", false, 0.9f}},
        // rideable (walk / run speed and jump while ridden)
        {"pony", {0.055f, "pony-body", "pony-head", "pony-foot", "", "pony-tail", false, 0.7f, true, {5.5f, 10, 8}}},
        {"alpaca", {0.06f, "alpaca-body", "alpaca-head", "alpaca-foot", "alpaca-hand", "", false, 0.6f, true, {5, 8.5f, 8.5f}}},
        // monsters of the wilds (game/combat.cpp)
        {"wolf", {0.05f, "wolf-body", "wolf-head", "wolf-foot", "wolf-hand", "wolf-tail", false, 1.0f}},
        {"gnoll", {0.05f, "gnoll-body", "gnoll-head", "gnoll-foot", "gnoll-hand", "", true, 0.8f}},
        {"polar-gnoll", {0.05f, "polar-gnoll-body", "polar-gnoll-head", "polar-gnoll-foot", "polar-gnoll-hand", "", true, 0.8f}},
        {"skeleton", {0.05f, "skeleton-body", "skeleton-head", "skeleton-foot", "skeleton-hand", "", true, 0.8f}},
        {"bat", {0.04f, "bat-body", "bat-head", "bat-foot", "bat-hand", "", true, 1.0f}},
        {"ogre", {0.09f, "ogre-body", "ogre-head", "ogre-foot", "ogre-hand", "", true, 0.6f}},
    };
    return A;
}
struct Herd {
    std::string kind;
    int n;
    glm::vec2 at;
    float r;
    bool ice = false;
};
std::vector<Herd> herds(const std::string& world) {
    if (world == "winter") return {{"penguin", 4, {30, -2}, 11, true}, {"alpaca", 2, {-6, 16}, 6}};
    if (world == "medieval")
        return {{"pony", 3, {-28, 16}, 6}, {"sheep", 4, {40, 24}, 9}, {"pig", 2, {34, 36}, 6}, {"chicken", 4, {-24, 13}, 5}, {"cat", 1, {0, 8}, 9}};
    // ponies waiting at both ends of the old road (ride them from town to town) + sheep in the meadow
    if (world == "wilds") return {{"pony", 2, {-8, 92}, 4}, {"pony", 2, {-8, 166}, 4}, {"sheep", 3, {-20, 145}, 7}, {"alpaca", 1, {-14, 104}, 5}};
    return {};
}

// ---------- buildings (walk inside / climb stairs / stand on roofs from per-column voxel data) ----------
// 1 voxel = 0.45 m: Cube World doors are 3 voxels tall (1.35 m), stairs 0.45 m (stepable, <= 0.5)
constexpr float VS = 0.45f;
// home = furnished house where you can sleep / cook
struct BuildingSpec {
    std::string name;
    glm::vec2 at;
    float r;
    bool snow = false, home = false;
};
std::vector<BuildingSpec> buildings(const std::string& world) {
    if (world == "winter")
        return {{"frame-house01", {-26, 30}, 26, true, true}, {"building-watchtower", {40, 28}, 26, true}, {"building-mage", {-46, -30}, 26, true}};
    if (world == "medieval")
        return {{"frame-house01", {-30, 40}, 20, false, true}, {"building-barn", {46, 38}, 14},     {"building-stilt-hut1", {-54, -12}, 14},
                {"building-witch-house", {-48, 22}, 18},     {"building-smithy", {38, -2}, 14}, {"building-alchemy", {-34, -48}, 18}};
    // a fisherman's hut on stilts by the harbour
    if (world == "wilds") return {{"building-stilt-hut1", {-20, 160}, 12}};
    return {};
}

// furniture of frame-house01 (house-local metres; floor = voxel 9; door on +Z at x ~ 3.6)
// rot = facing (models face -Z: 0 = towards the back wall, PI = out of the front door, PI/2 = towards -X)
constexpr int HOME_FLOOR = 9;
struct Furniture {
    std::string name;
    float h = 0, w = 0;
    glm::vec2 at;
    float rot = 0, solid = 0, y = 0;
    std::string act;
    bool table = false, fire = false;
};
const std::vector<Furniture>& homeLayout() {
    static const std::vector<Furniture> L = {
        {"bed", 0.8f, 0, {-4.6f, -3.0f}, 0, 0.9f, 0, "bed"},
        {"bedtable", 0.7f, 0, {-3.2f, -4.0f}, 0, 0.35f},
        {"candle01", 0.45f, 0, {-3.2f, -4.0f}, 0, 0, 0.7f},
        {"desktop", 0.85f, 0, {-0.8f, 0.6f}, 0, 0.9f, 0, "", true},
        {"stool", 0.5f, 0, {-1.5f, 1.6f}, 0, 0.25f},
        {"stool", 0.5f, 0, {-0.1f, 1.6f}, 0, 0.25f},
        {"carpet1", 0, 2.8f, {-0.8f, 1.4f}},
        {"cauldron", 0, 1.5f, {4.7f, -3.1f}, 0, 0.7f, 0, "cook", false, true},
        {"shelf1", 2.0f, 0, {6.0f, 0.4f}, PI / 2, 0.5f},
        {"cupboard", 2.0f, 0, {1.2f, -4.2f}, PI, 0.6f},
        {"painting02", 0, 1.6f, {-1.4f, -4.55f}, PI, 0, 1.5f},
        {"flower-vase2", 0.4f, 0, {-0.3f, 0.6f}, 0, 0, 0.85f},
        {"fruit-basket", 0.35f, 0, {-1.4f, 0.5f}, 0, 0, 0.85f},
        {"carpet2", 0, 2.0f, {-4.6f, -1.0f}},
        {"barrel", 0.9f, 0, {3.1f, -4.0f}, 0, 0.4f},
        {"sack", 0.7f, 0, {5.9f, -1.6f}, 0, 0.35f},
        {"bench", 0.5f, 0, {-5.9f, 1.2f}, PI / 2, 0.5f},
        {"flower-vase1", 0.7f, 0, {-5.9f, 3.9f}},
        {"flower-vase3", 0.7f, 0, {5.9f, 3.9f}},
        {"candle02", 0.5f, 0, {5.9f, -0.6f}},
    };
    return L;
}
// dishes the pot can make (Cube World food models)
const std::vector<std::string> DISHES = {"strawberry-cake", "pumpkin-muffin", "pancakes", "soup-ginger", "hot-chocolate",
                                         "pizza", "croissant", "candied-apple", "cake2", "bread"};

double nowSeconds() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

std::string tagKey(const std::string& name, const CwTag& o) {
    char buf[256];
    std::snprintf(buf, sizeof buf, "%s|%g|%g|%g|%d|%g,%g,%g|%d|%g|%g,%g,%g|%d", name.c_str(), o.h, o.w, o.d, o.hasFit, o.fit.x, o.fit.y, o.fit.z,
                  o.center, o.rotY, o.offset.x, o.offset.y, o.offset.z, o.snow);
    return buf;
}

}  // namespace

// placed decoration / building (prevents overlaps; perching birds pick hosts from here)
struct Placed {
    std::string name;
    float x, z, fr, top;
    bool taken = false;
};

struct CubeWorld::Table {
    ObjPtr room;
    glm::vec3 at;
    ObjPtr dish;
};

// per-column voxel collision of a building
struct VoxelCollider {
    int W = 0, D = 0;
    std::vector<std::vector<int>> cols;
    float x0, y0, z0, rot, cs, sn, R2;
    const std::vector<int>* runsAt(float x, float z) const {
        float dx = x - x0, dz = z - z0;
        if (dx * dx + dz * dz > R2) return nullptr;
        float lx = dx * cs - dz * sn, lz = dx * sn + dz * cs;
        int ix = (int)std::floor(lx / VS + W / 2.0f), iz = (int)std::floor(lz / VS + D / 2.0f);
        if (ix < 0 || iz < 0 || ix >= W || iz >= D) return nullptr;
        const auto& r = cols[(size_t)iz * W + ix];
        return r.empty() ? nullptr : &r;
    }
    // highest floor reachable from height y (walls are too tall to step -> you bump into them)
    float ground(float x, float z, float y) const {
        const auto* runs = runsAt(x, z);
        float g = -INFINITY;
        if (!runs) return g;
        for (size_t i = 0; i + 1 < runs->size(); i += 2) {
            // gaps lower than a door (< 3 voxels), e.g. windows, count as solid
            int bottom = (*runs)[i], top = (*runs)[i + 1];
            while (i + 2 < runs->size() && (*runs)[i + 2] - top < 3) {
                top = (*runs)[i + 3];
                i += 2;
            }
            if (y0 + bottom * VS <= y + 0.5f) g = std::max(g, y0 + top * VS);
        }
        return g;
    }
    // a roof or ceiling above the head
    bool roofAbove(float x, float z, float y) const {
        const auto* runs = runsAt(x, z);
        if (runs)
            for (size_t i = 0; i + 1 < runs->size(); i += 2)
                if (y0 + (*runs)[i] * VS > y + 1.2f) return true;
        return false;
    }
};

struct CubeWorld::Building {
    ObjPtr mesh;
    std::shared_ptr<VoxelCollider> vc;
};

struct CubeWorld::Animal {
    AnimalModel m;
    Herd herd;
    const AnimalSpec* spec;
    float heading, wait, phase, vy = 0;
    bool hasTarget = false, rider = false;
    glm::vec2 target{0};
};

struct CubeWorld::Herds : CubeWorld::Ride {
    World* world;
    std::shared_ptr<Rng> rand;
    std::vector<std::unique_ptr<Animal>> list;
    std::vector<glm::vec3> blocked;
    Animal* mounted = nullptr;
    float mountSpeed = 0;

    bool okSpot(const Animal& a, float x, float z) const { return okSpotFor(a.herd, x, z); }
    bool okSpotFor(const Herd& h, float x, float z) const {
        for (auto& b : blocked)
            if (std::hypot(b.x - x, b.y - z) < b.z) return false;
        if (world->isWater(x, z)) return false;
        if (!h.ice && world->isIce(x, z)) return false;
        if (std::abs(world->groundAt(x, z, 99) - world->groundAt(x, z, -9)) >= 0.05f) return false;
        return (h.ice && world->isIce(x, z)) || world->isFree(x, z, 0.3f);
    }
    // one step along the heading (faces -Z); false when blocked (collision / too high / water)
    bool stepForward(Animal& a, float dist, float maxStep) {
        glm::vec3& p = a.m.g->position;
        glm::vec3 tmp(p.x - std::sin(a.heading) * dist, p.y, p.z - std::cos(a.heading) * dist);
        world->resolveCollisions(tmp, a.rider ? 0.45f : 0.25f);
        float g = world->groundAt(tmp.x, tmp.z, p.y);
        if (g > p.y + maxStep || world->isWater(tmp.x, tmp.z)) return false;
        float moved = std::hypot(tmp.x - p.x, tmp.z - p.z);
        p.x = tmp.x;
        p.z = tmp.z;
        return moved > dist * 0.3f;
    }
    void animate(Animal& a, float t, float dt, float speed) {
        a.m.g->rotation.y = a.heading;
        a.phase += dt * (speed > 0.05f ? 6 + speed * 2.2f : 0);
        float sw = speed > 0.05f ? std::sin(a.phase) : 0;
        float k = std::min(1.0f, 0.4f + speed * 0.12f);
        a.m.rig->position.y = std::abs(sw) * 0.05f * k;
        for (size_t i = 0; i < a.m.legs.size(); ++i) a.m.legs[i]->position.y = std::max(0.0f, ((i == 0 || i == 3) ? sw : -sw) * 0.08f * k);
        a.m.head->rotation.x = speed > 0.05f ? sw * 0.04f : std::sin(t * 0.7f + a.phase) * 0.12f;
    }
    void update(float t, float dt) {
        for (auto& ap : list) {
            Animal& a = *ap;
            if (a.rider) continue;
            glm::vec3& p = a.m.g->position;
            if (!a.hasTarget) {
                a.wait -= dt;
                if (a.wait < 0) {
                    float ang = (float)(*rand)() * PI * 2, d = (float)(*rand)() * a.herd.r;
                    float x = a.herd.at.x + std::cos(ang) * d, z = a.herd.at.y + std::sin(ang) * d;
                    if (okSpot(a, x, z)) {
                        a.target = {x, z};
                        a.hasTarget = true;
                    } else {
                        a.wait = 0.5f;
                    }
                }
            }
            float speed = 0;
            if (a.hasTarget) {
                float dx = a.target.x - p.x, dz = a.target.y - p.z, dist = std::hypot(dx, dz);
                if (dist < 0.2f) {
                    a.hasTarget = false;
                    a.wait = 2 + (float)(*rand)() * 5;
                } else {
                    float dh = wrapAngle(std::atan2(-dx, -dz) - a.heading);
                    a.heading += dh * std::min(1.0f, dt * 4);
                    if (std::abs(dh) < 0.6f) {
                        // stuck on something: pick another target
                        if (!stepForward(a, std::min(dist, a.spec->speed * dt), 0.3f)) {
                            a.hasTarget = false;
                            a.wait = 1;
                        }
                        speed = a.spec->speed;
                    }
                }
            }
            // fall off ledges / walk down slopes
            float g = world->groundAt(p.x, p.z, p.y);
            if (p.y > g + 0.05f) {
                a.vy -= 24 * dt;
                p.y = std::max(g, p.y + a.vy * dt);
            } else {
                p.y = g;
                a.vy = 0;
            }
            animate(a, t, dt, speed);
        }
    }

    // ---------- Ride ----------
    bool active() const override { return mounted != nullptr; }
    std::string kind() const override { return mounted ? mounted->herd.kind : ""; }
    glm::vec4 pose() const override {
        if (!mounted) return glm::vec4(0);
        glm::vec3 p = mounted->m.g->position;
        return {p.x, p.y, p.z, mounted->heading};
    }
    void* nearest(glm::vec3 pos, float r) override {
        Animal* best = nullptr;
        float bd = r;
        for (auto& a : list) {
            if (!a->spec->rideable) continue;
            glm::vec3 p = a->m.g->position;
            float d = std::hypot(p.x - pos.x, p.z - pos.z);
            if (d < bd && std::abs(p.y - pos.y) < 1.5f) {
                best = a.get();
                bd = d;
            }
        }
        return best;
    }
    void mount(void* animal) override {
        auto* a = (Animal*)animal;
        mounted = a;
        a->rider = true;
        a->hasTarget = false;
        a->vy = 0;
        mountSpeed = 0;
    }
    glm::vec3 dismount() override {
        Animal* a = mounted;
        mounted = nullptr;
        a->rider = false;
        a->wait = 4;
        glm::vec3 p = a->m.g->position;
        float sx = std::cos(a->heading), sz = -std::sin(a->heading);
        for (float side : {1.0f, -1.0f}) {
            float x = p.x + sx * side * 1.2f, z = p.z + sz * side * 1.2f;
            if (std::abs(world->groundAt(x, z, p.y) - p.y) < 0.6f && !world->isWater(x, z)) return {x, p.y, z};
        }
        return p;
    }
    void jump() override {
        if (mounted && mounted->vy == 0) mounted->vy = mounted->spec->ride.jump;
    }
    float update(float t, float dt, glm::vec2 move, float camYaw, bool sprint, glm::vec3& seat) override {
        Animal& a = *mounted;
        glm::vec3& p = a.m.g->position;
        const RideSpec& R = a.spec->ride;
        float mag = std::min(1.0f, std::hypot(move.x, move.y));
        if (mag > 0.1f) {
            float s = std::sin(camYaw), c = std::cos(camYaw);
            float dx = move.x * c + move.y * s, dz = -move.x * s + move.y * c;
            float dh = wrapAngle(std::atan2(-dx, -dz) - a.heading);
            a.heading += dh * std::min(1.0f, dt * 7);
        }
        mountSpeed += ((sprint ? R.run : R.walk) * mag - mountSpeed) * std::min(1.0f, dt * 5);
        if (mountSpeed > 0.05f && !stepForward(a, mountSpeed * dt, 0.6f)) mountSpeed *= 0.3f;
        // jump / fall
        float g = world->groundAt(p.x, p.z, p.y);
        if (a.vy != 0 || p.y > g + 0.05f) {
            a.vy -= 24 * dt;
            p.y += a.vy * dt;
            if (p.y <= g) {
                p.y = g;
                a.vy = 0;
            }
        } else {
            p.y = g;
        }
        animate(a, t, dt, a.vy != 0 ? 0 : mountSpeed);
        // seat: on the back, a little towards the tail (+Z of the animal), sunk so the legs disappear
        float back = a.m.bodyD * 0.12f;
        seat = {p.x + std::sin(a.heading) * back, p.y + a.m.rig->position.y + a.m.seatY - 0.32f, p.z + std::cos(a.heading) * back};
        // the player faces +Z
        return a.heading + PI;
    }
};

struct CubeWorld::PerWorld {
    std::shared_ptr<Rng> rand;
    std::vector<Building> buildings;
    std::vector<std::unique_ptr<Act>> acts;
    std::vector<std::unique_ptr<Table>> tables;
    std::unique_ptr<Herds> herds;
};

// ---------------------------------------------------------------------------

bool CubeWorld::init() {
    if (ok_) return true;
    const char* env = std::getenv("CW_ASSETS");
    dir_ = env && *env ? env : std::string(WEB_DIR) + "/assets/cubeworld";
    std::ifstream f(dir_ + "/manifest.json");
    if (!f) {
        std::printf("Cube World assets not found at %s (set CW_ASSETS) — using the built-in models\n", dir_.c_str());
        return false;
    }
    json j = json::parse(f, nullptr, false);
    if (!j.is_object()) return false;
    for (auto& [name, v] : j.items()) {
        Info info;
        auto s = v["size"];
        info.size = {s[0].get<float>(), s[1].get<float>(), s[2].get<float>()};
        info.cols = v.value("cols", false);
        manifest_[name] = info;
    }
    mat_ = lambert(nullptr);
    mat_->vertexColors = true;
    ok_ = true;
    std::printf("Cube World: %zu models in %s\n", manifest_.size(), dir_.c_str());
    return true;
}

bool CubeWorld::has(const std::string& name) {
    if (!ok_ || !manifest_.count(name)) return false;
    auto it = base_.find(name);
    if (it != base_.end()) return it->second != nullptr;
    GeoPtr g;
    if (auto sc = loadGltf(dir_ + "/" + name + ".glb")) {
        sc->root->traverse([&](Object3D& o) {
            if (o.kind == ObjectKind::Mesh && o.geometry) g = o.geometry;
        });
    }
    base_[name] = g;
    return g != nullptr;
}

glm::vec3 CubeWorld::size(const std::string& name) const {
    auto it = manifest_.find(name);
    return it == manifest_.end() ? glm::vec3(1) : it->second.size;
}

GeoPtr CubeWorld::geometry(const std::string& name, const CwTag& opt) {
    if (!has(name)) return nullptr;
    std::string key = tagKey(name, opt);
    auto it = cache_.find(key);
    if (it != cache_.end()) return it->second;
    glm::vec3 S = manifest_[name].size;
    float sx, sy, sz;
    if (opt.hasFit) {
        sx = opt.fit.x / S.x;
        sy = opt.fit.y / S.y;
        sz = opt.fit.z / S.z;
    } else {
        sx = sy = sz = opt.h ? opt.h / S.y : opt.w ? opt.w / S.x : opt.d ? opt.d / S.z : 1.0f / 16;
    }
    auto g = base_[name]->clone();
    g->scale(sx, sy, sz);
    if (opt.center) g->translate(0, -S.y * sy / 2, 0);
    if (opt.rotY) g->rotateY(opt.rotY);
    if (opt.offset != glm::vec3(0)) g->translate(opt.offset.x, opt.offset.y, opt.offset.z);
    if (opt.snow) {
        for (size_t i = 0; i < g->color.size(); ++i) {
            if (g->normal[i].y < 0.5f) continue;
            glm::vec3& c = g->color[i];
            c = {c.x * 0.2f + 0.72f, c.y * 0.2f + 0.76f, c.z * 0.2f + 0.85f};
        }
    }
    g->computeBounds();
    cache_[key] = g;
    return g;
}

ObjPtr CubeWorld::mesh(const std::string& name, const CwTag& opt) {
    auto g = geometry(name, opt);
    if (!g) return nullptr;
    auto m = makeMesh(g, mat_);
    m->castShadow = m->receiveShadow = true;
    return m;
}

// Mesh / InstancedMesh: swap geometry + material in place (position, hiding and animation keep working).
// Group: hide its meshes (except tagged ones or those in cwKeep) and add the model(s).
void CubeWorld::applyTags(Object3D& root) {
    if (!ok_) return;
    std::vector<Object3D*> tagged;
    root.traverse([&](Object3D& o) {
        if (!o.cw.empty()) tagged.push_back(&o);
    });
    for (Object3D* o : tagged) {
        bool all = true;
        for (auto& p : o->cw)
            if (!geometry(p.name, p)) all = false;
        if (!all) continue;
        if (o->kind == ObjectKind::Mesh) {
            o->geometry = geometry(o->cw[0].name, o->cw[0]);
            o->materials = {mat_};
            o->multiMaterial = false;
            continue;
        }
        auto& keep = o->cwKeep;
        o->traverse([&](Object3D& c) {
            if (c.kind != ObjectKind::Mesh || &c == o) return;
            for (Object3D* p = &c; p && p != o; p = p->parent)
                if (!p->cw.empty() || std::find(keep.begin(), keep.end(), p) != keep.end()) return;
            c.visible = false;
        });
        for (auto& p : o->cw) o->add(mesh(p.name, p));
    }
}

ObjPtr CubeWorld::createFire(glm::vec3 pos) {
    auto g = makeGroup("fire");
    g->position = pos;
    const uint32_t cols[3] = {0xffd25a, 0xff8a2a, 0xff5a1f};
    std::vector<Object3D*> cubes;
    for (int i = 0; i < 3; ++i) {
        float s = 0.34f - i * 0.07f;
        auto m = basic(hexc(cols[i]));
        m->transparent = true;
        m->opacity = 0.9f;
        auto c = makeMesh(geo::box(s, s, s), m);
        g->add(c);
        cubes.push_back(c.get());
    }
    auto light = makePointLight(hexc(0xff9a4a), 1.4f, 9, 2);
    light->position.y = 0.6f;
    g->add(light);
    Object3D* l = light.get();
    cubes[0]->onBeforeRender = [cubes, l](Object3D&) {
        float t = (float)std::fmod(nowSeconds(), 10000.0);
        for (int i = 0; i < 3; ++i) {
            float k = std::fmod(t * 1.6f + i / 3.0f, 1.0f);
            cubes[i]->position = {std::sin(t * 3 + i * 2) * 0.12f, k * 0.9f, std::cos(t * 2.4f + i) * 0.12f};
            cubes[i]->scale = glm::vec3(1 - k * 0.8f);
            cubes[i]->rotation = {t + i, t * 1.3f, 0};
        }
        l->intensity = 1.2f + std::sin(t * 13) * 0.15f + std::sin(t * 7.3f) * 0.15f;
    };
    return g;
}

CubeWorld::AnimalModel CubeWorld::buildAnimal(const std::string& kind) {
    const AnimalSpec& spec = animals().at(kind);
    float u = spec.u;
    auto sizeOf = [&](const std::string& n) { return manifest_[n].size * u; };
    auto part = [&](const std::string& n) {
        CwTag t;
        t.name = n;
        t.h = manifest_[n].size.y * u;
        return mesh(n, t);
    };
    AnimalModel a;
    a.g = makeGroup();
    // bobbing while walking
    a.rig = makeGroup();
    a.g->add(a.rig);
    glm::vec3 bs = sizeOf(spec.body);
    float fh = sizeOf(spec.foot).y;
    auto legAt = [&](const std::string& n, float x, float z) {
        auto m = part(n);
        m->position = {x, 0, z};
        a.g->add(m);
        a.legs.push_back(m);
    };
    float bodyY;
    if (spec.biped) {
        bodyY = fh * 0.55f;
        legAt(spec.foot, -bs.x * 0.22f, 0);
        legAt(spec.foot, bs.x * 0.22f, 0);
    } else {
        bodyY = fh * 0.75f;
        const std::string& front = spec.hand.empty() ? spec.foot : spec.hand;
        legAt(front, -bs.x * 0.3f, -bs.z * 0.3f);
        legAt(front, bs.x * 0.3f, -bs.z * 0.3f);
        legAt(spec.foot, -bs.x * 0.3f, bs.z * 0.3f);
        legAt(spec.foot, bs.x * 0.3f, bs.z * 0.3f);
    }
    auto body = part(spec.body);
    body->position.y = bodyY;
    a.head = part(spec.head);
    glm::vec3 hs = sizeOf(spec.head);
    if (spec.biped) a.head->position = {0, bodyY + bs.y * 0.75f, -bs.z * 0.05f};
    else a.head->position = {0, bodyY + bs.y * 0.35f, -bs.z / 2 - hs.z * 0.3f};
    a.rig->add(body, a.head);
    if (!spec.hand.empty() && spec.biped) {
        float hw2 = sizeOf(spec.hand).x;
        for (float sx : {-1.0f, 1.0f}) {
            auto w = part(spec.hand);
            w->position = {sx * (bs.x / 2 + hw2 / 2), bodyY + bs.y * 0.25f, 0};
            a.rig->add(w);
        }
    }
    if (!spec.tail.empty()) {
        auto t = part(spec.tail);
        t->position = {0, bodyY + bs.y * 0.5f, bs.z / 2};
        t->rotation.x = 0.5f;
        a.rig->add(t);
    }
    a.height = bodyY + bs.y + hs.y;
    a.seatY = bodyY + bs.y * 0.8f;
    a.bodyD = bs.z;
    return a;
}

ObjPtr CubeWorld::animalModel(const std::string& kind) {
    if (!hasAnimal(kind)) return nullptr;
    return buildAnimal(kind).g;
}

bool CubeWorld::hasAnimal(const std::string& kind) {
    auto it = animals().find(kind);
    if (!ok_ || it == animals().end()) return false;
    const AnimalSpec& a = it->second;
    for (auto* n : {&a.body, &a.head, &a.foot, &a.hand, &a.tail})
        if (!n->empty() && !has(*n)) return false;
    return true;
}

void CubeWorld::createHerds(World& world, PerWorld& pw, const std::vector<glm::vec3>& blocked) {
    pw.herds = std::make_unique<Herds>();
    Herds& H = *pw.herds;
    H.world = &world;
    H.rand = pw.rand;
    H.blocked = blocked;
    Rng& rand = *pw.rand;
    for (const Herd& herd : herds(world.name)) {
        const AnimalSpec& spec = animals().at(herd.kind);
        bool ok = true;
        for (auto* n : {&spec.body, &spec.head, &spec.foot, &spec.hand, &spec.tail})
            if (!n->empty() && !has(*n)) ok = false;
        if (!ok) continue;
        for (int i = 0; i < herd.n; ++i) {
            float x, z;
            int t = 0;
            do {
                float a = rand.f() * PI * 2, d = rand.f() * herd.r;
                x = herd.at.x + std::cos(a) * d;
                z = herd.at.y + std::sin(a) * d;
            } while (!H.okSpotFor(herd, x, z) && ++t < 60);
            if (t >= 60) continue;
            auto an = std::make_unique<Animal>();
            an->m = buildAnimal(herd.kind);
            an->m.g->position = {x, world.groundAt(x, z, -9), z};
            an->m.g->name = "cw:" + herd.kind;
            world.addObject(an->m.g);
            an->herd = herd;
            an->spec = &spec;
            an->heading = rand.f() * 6.28f;
            an->wait = rand.f() * 3;
            an->phase = rand.f() * 10;
            H.list.push_back(std::move(an));
        }
    }
}

void CubeWorld::furnish(World& world, PerWorld& pw, glm::vec3 floor, float rot) {
    auto room = makeGroup("room");
    room->position = floor;
    room->rotation.y = rot;
    world.addObject(room);
    room->updateWorldFromParents();
    Table* table = nullptr;
    std::vector<Act*> mine;
    for (const Furniture& f : homeLayout()) {
        CwTag t;
        t.name = f.name;
        if (f.w) t.w = f.w;
        else t.h = f.h;
        auto m = mesh(f.name, t);
        if (!m) continue;
        m->name = "cw:" + f.name;
        m->position = {f.at.x, f.y ? f.y : 0.01f, f.at.y};
        m->rotation.y = f.rot;
        room->add(m);
        glm::vec3 wp = glm::vec3(room->matrixWorld * glm::vec4(f.at.x, 0, f.at.y, 1));
        if (f.solid) world.addPlatform({wp.x, wp.z, f.solid, floor.y + f.y + (f.h ? f.h : 0.1f)});
        if (f.fire) {
            auto fire = createFire({0, 0, 0});
            fire->position = {f.at.x, 0.25f, f.at.y};
            room->add(fire);
        }
        if (f.table) {
            auto tb = std::make_unique<Table>();
            tb->room = room;
            tb->at = {f.at.x, f.h + 0.01f, f.at.y};
            table = tb.get();
            pw.tables.push_back(std::move(tb));
        }
        if (!f.act.empty()) {
            auto a = std::make_unique<Act>();
            a->kind = f.act;
            a->pos = {wp.x, floor.y, wp.z};
            mine.push_back(a.get());
            pw.acts.push_back(std::move(a));
        }
    }
    // the table is resolved after the whole room is built (act.table() in the JS)
    for (Act* a : mine) a->table = table;
}

void CubeWorld::placeBuildings(World& world, PerWorld& pw, std::vector<Placed>& placed, const std::vector<glm::vec3>& avoid) {
    for (const BuildingSpec& b : buildings(world.name)) {
        auto mi = manifest_.find(b.name);
        if (mi == manifest_.end() || !mi->second.cols) continue;
        std::ifstream f(dir_ + "/" + b.name + ".cols.json");
        json j = f ? json::parse(f, nullptr, false) : json();
        if (!j.is_object()) continue;
        CwTag tag;
        tag.name = b.name;
        tag.h = mi->second.size.y * VS;
        tag.snow = b.snow;
        auto geo = geometry(b.name, tag);
        if (!geo) continue;
        auto vc = std::make_shared<VoxelCollider>();
        vc->W = j["w"];
        vc->D = j["d"];
        for (auto& c : j["cols"]) {
            std::vector<int> runs;
            if (c.is_array())
                for (auto& v : c) runs.push_back(v.get<int>());
            vc->cols.push_back(std::move(runs));
        }
        float hw = vc->W * VS / 2, hd = vc->D * VS / 2, R = std::hypot(hw, hd);
        // Cube World buildings bury their base: sink until the lowest edge step (usually stairs) is 1 voxel tall
        int edgeTop = INT32_MAX;
        for (int iz = 0; iz < vc->D; ++iz)
            for (int ix = 0; ix < vc->W; ++ix) {
                if (ix && iz && ix < vc->W - 1 && iz < vc->D - 1) continue;
                const auto& runs = vc->cols[(size_t)iz * vc->W + ix];
                if (!runs.empty() && runs[0] == 0) edgeTop = std::min(edgeTop, runs[1]);
            }
        float sink = edgeTop != INT32_MAX ? std::max(0, edgeTop - 1) * VS : 0;
        // search outwards from `at`: the whole footprint must be free (trees allowed), off important spots, not too steep
        std::vector<glm::vec2> cands;
        for (float d = 0; d <= b.r; d += 2) {
            int n = std::max(1, (int)std::lround(d * 1.2f));
            for (int a = 0; a < n; ++a)
                cands.push_back({b.at.x + std::cos((float)a / n * PI * 2 + d) * d, b.at.y + std::sin((float)a / n * PI * 2 + d) * d});
        }
        for (glm::vec2 c : cands) {
            float x = c.x, z = c.y;
            bool bad = false;
            for (auto& p : placed)
                if (std::hypot(p.x - x, p.z - z) < p.fr + R) bad = true;
            for (auto& a : avoid)
                if (std::hypot(a.x - x, a.y - z) < a.z + R) bad = true;
            if (bad) continue;
            // door (+Z of the house) faces the middle of the map
            float rot = std::round(std::atan2(-x, -z) / (PI / 2)) * PI / 2;
            float cs = std::cos(rot), sn = std::sin(rot);
            bool ok = true;
            float lo = INFINITY, hi = -INFINITY;
            for (float u = -hw - 0.5f; u <= hw + 0.51f && ok; u += 1.5f)
                for (float v = -hd - 0.5f; v <= hd + 0.51f && ok; v += 1.5f) {
                    float px = x + u * cs + v * sn, pz = z - u * sn + v * cs;
                    if (!world.isFree(px, pz, 0.3f, true) || world.isWater(px, pz)) ok = false;
                    float g = world.groundAt(px, pz, -9);
                    lo = std::min(lo, g);
                    hi = std::max(hi, g);
                }
            if (!ok || hi - lo > 1.6f) continue;
            world.clearTrees(x, z, R + 1);
            float y = lo - 0.1f - sink;
            // own material: each house is cut open separately when you walk inside
            auto m = makeMesh(geo, mat_->clone());
            m->name = "cw:" + b.name;
            m->position = {x, y, z};
            m->rotation.y = rot;
            m->castShadow = m->receiveShadow = true;
            world.addObject(m);
            vc->x0 = x;
            vc->y0 = y;
            vc->z0 = z;
            vc->rot = rot;
            vc->cs = cs;
            vc->sn = sn;
            vc->R2 = (vc->W * vc->W + vc->D * vc->D) * VS * VS / 4;
            world.addGround([vc](float px, float pz, float py) { return vc->ground(px, pz, py); });
            placed.push_back({b.name, x, z, R, y});
            pw.buildings.push_back({m, vc});
            if (b.home) furnish(world, pw, {x, y + HOME_FLOOR * VS, z}, rot);
            break;
        }
    }
}

void CubeWorld::placeProps(World& world, PerWorld& pw, float waterY, std::vector<Placed>& placed) {
    Rng& rand = *pw.rand;
    for (const Group& group : layout(world.name)) {
        float cx = group.at.x, cz = group.at.y, r0 = group.r.x, r1 = group.r.y;
        for (const Item& item : group.items) {
            CwTag t;
            t.name = item.name;
            t.h = item.h;
            auto geo = geometry(item.name, t);
            if (!geo) continue;
            glm::vec3 S = manifest_[item.name].size;
            float s = item.h / S.y;
            float fr = std::max(S.x, S.z) * s / 2;
            for (int k = 0; k < item.n; ++k) {
                float x = 0, z = 0, y = 0;
                if (!item.perch.empty()) {
                    std::vector<Placed*> host;
                    for (auto& p : placed)
                        if (std::find(item.perch.begin(), item.perch.end(), p.name) != item.perch.end() && !p.taken && std::hypot(p.x - cx, p.z - cz) < r1 + 2)
                            host.push_back(&p);
                    if (host.empty()) break;
                    Placed* p = host[(size_t)(rand() * host.size())];
                    p->taken = true;
                    x = p->x;
                    z = p->z;
                    y = p->top;
                } else {
                    bool ok = false;
                    for (int tr = 0; tr < 80 && !ok; ++tr) {
                        float a = rand.f() * PI * 2, dist = std::sqrt(r0 * r0 + rand.f() * (r1 * r1 - r0 * r0));
                        x = cx + std::cos(a) * dist;
                        z = cz + std::sin(a) * dist;
                        bool bad = false;
                        for (auto& p : placed)
                            if (std::hypot(p.x - x, p.z - z) < p.fr + fr + 0.3f) bad = true;
                        if (bad) continue;
                        if (item.water) {
                            ok = world.isWater(x, z) && world.isWater(x + fr, z) && world.isWater(x - fr, z) && world.isWater(x, z + fr) &&
                                 world.isWater(x, z - fr);
                            y = waterY - 0.02f;
                            continue;
                        }
                        if (!world.isFree(x, z, std::max(0.3f, fr * 0.8f))) continue;
                        // ground must be flat enough (never straddle a mountain terrace)
                        float hs[5] = {world.groundAt(x, z, -9), world.groundAt(x + fr, z, -9), world.groundAt(x - fr, z, -9),
                                       world.groundAt(x, z + fr, -9), world.groundAt(x, z - fr, -9)};
                        float mx = *std::max_element(hs, hs + 5), mn = *std::min_element(hs, hs + 5);
                        if (mx - mn > 0.3f) continue;
                        y = mn - 0.03f;
                        ok = true;
                    }
                    if (!ok) continue;
                }
                auto m = makeMesh(geo, mat_);
                m->name = "cw:" + item.name;
                m->position = {x, y, z};
                // Cube World models face -Z (checked against the in-game market stalls)
                m->rotation.y = item.face ? std::atan2(cx - x, cz - z) + PI : rand.f() * PI * 2;
                m->castShadow = item.h > 0.4f;
                m->receiveShadow = true;
                world.addObject(m);
                placed.push_back({item.name, x, z, fr, y + item.h});
                if (item.fire) world.addObject(createFire({x, y + item.h * 0.55f, z}));
                if (item.solid) world.addPlatform({x, z, std::min(S.x, S.z) * s / 2 * 0.9f, y + item.h});
            }
        }
    }
}

void CubeWorld::populate(World& world, float waterY, const std::vector<glm::vec3>& avoid) {
    if (!ok_) return;
    auto pw = std::make_unique<PerWorld>();
    pw->rand = std::make_shared<Rng>(world.name == "winter" ? 1213 : world.name == "medieval" ? 2512 : 7171);
    std::vector<Placed> placed;
    placeBuildings(world, *pw, placed, avoid);
    placeProps(world, *pw, waterY, placed);
    std::vector<glm::vec3> blocked;
    for (auto& p : placed)
        for (auto& b : buildings(world.name))
            if (b.name == p.name) blocked.push_back({p.x, p.z, p.fr + 0.5f});
    createHerds(world, *pw, blocked);
    per_[world.name] = std::move(pw);
}

void CubeWorld::update(float t, float dt, World& world, glm::vec3 pos) {
    auto it = per_.find(world.name);
    if (it == per_.end()) return;
    PerWorld& pw = *it->second;
    pw.herds->update(t, dt);
    // under a roof: cut the building above head height so the room is visible
    for (auto& b : pw.buildings) {
        Material& m = *b.mesh->material();
        if (b.vc->roofAbove(pos.x, pos.z, pos.y)) {
            m.clipping = true;
            m.clipPlane = {0, -1, 0, pos.y + 2.0f};
        } else {
            m.clipping = false;
        }
    }
}

CubeWorld::Ride* CubeWorld::ride(World& world) {
    auto it = per_.find(world.name);
    return it == per_.end() ? nullptr : it->second->herds.get();
}

const CubeWorld::Act* CubeWorld::nearestAct(World& world, glm::vec3 pos, float r) {
    auto it = per_.find(world.name);
    if (it == per_.end()) return nullptr;
    const Act* best = nullptr;
    float bd = r;
    for (auto& a : it->second->acts) {
        float d = std::hypot(a->pos.x - pos.x, a->pos.z - pos.z);
        if (d < bd && std::abs(a->pos.y - pos.y) < 1.2f) {
            best = a.get();
            bd = d;
        }
    }
    return best;
}

std::string CubeWorld::cook(const Act& act) {
    Table* tbl = act.table;
    if (!tbl) return "";
    const std::string& name = DISHES[(size_t)(frand() * DISHES.size()) % DISHES.size()];
    CwTag t;
    t.name = name;
    t.w = 0.42f;
    auto dish = mesh(name, t);
    if (!dish) return "";
    dish->name = "cw:dish:" + name;
    if (tbl->dish) tbl->room->remove(tbl->dish.get());
    dish->position = tbl->at;
    tbl->room->add(dish);
    tbl->dish = dish;
    return name;
}
