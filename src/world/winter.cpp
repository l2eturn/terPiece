// Snow village — port of buildWorld() in patty-project/xmas-app/web/js/world.js.
// Built in the same order as the JS so the shared seeded rng (1225) produces the same layout.
#include "world/winter.h"
#include <cmath>
#include "art/glow.h"
#include "art/textures.h"
#include "gfx/shader.h"
#include "world/layout.h"

using namespace winter;

namespace {

// the frozen lake lies on the flat ground blocks (the web build sinks it into a dip)
constexpr float ICE_Y = 0.04f;
struct Hill {
    float x, z, h, s;
};
const Hill HILL{-12, 32, 4.2f, 5.5f};
const float CABINS[4][2] = {{-16, -12}, {16, -15}, {-19, 7}, {17, 12}};
const std::vector<glm::vec2> CAVE_PATH = {{0, 2}, {0, -9}, {5, -17}, {13, -20}, {20, -15}, {25, -7}, {23, 2}, {15, 7}};
const std::vector<std::vector<glm::vec2>> PATHS = {
    // continues south through the gate onto the old road to the citadel
    {{0, 82}, {0, 36}, {0, 20}, {0, 0}, {0, -28}},
    {{0, 0}, {22, -2}},
    {{0, 0}, {-14, -4}, {-30, -6}},
    {{0, 0}, {-6, 18}, {-11, 27}},
    {{0, 0}, {12, 9}},
    {{0, -18}, {16, -22}, {20, -22}},
    {{0, -28}, {-6, -40}, {CAVE_MOUTH.x - 2, CAVE_MOUTH.y + 1}},
};

float distToPathsW(float x, float z) { return distToPaths(PATHS, x, z); }

// mountain height: flat terraces 1.1 m apart (exactly one jump each)
float mountainAt(float x, float z) {
    float dx = x - MOUNTAIN.x, dz = z - MOUNTAIN.z, d = std::hypot(dx, dz);
    if (d > MOUNTAIN.r) return 0;
    // wavy outline instead of a perfect circle
    float a = std::atan2(dz, dx);
    float wob = 1 + 0.12f * std::sin(a * 3 + 1) + 0.07f * std::sin(a * 7);
    float k = std::max(0.0f, 1 - d / (MOUNTAIN.r * wob));
    float raw = MOUNTAIN.h * std::pow(k, 1.15f);
    float t = raw / MOUNTAIN.step, f = std::floor(t);
    return (f + smoothstep(0.86f, 1.0f, t - f)) * MOUNTAIN.step;
}

// W(geo, mat): mesh with world-projected UVs
ObjPtr W(const GeoPtr& g, const MatPtr& m) { return makeMesh(geo::worldUV(g), m); }

MatPtr lam(const std::string& texKey, uint32_t color = 0xffffff) { return lambert(tex::get(texKey), hexc(color)); }

}  // namespace

namespace winter {

// the smooth shape of the village (heightAt() turns it into blocks)
float rawHeight(float x, float z) {
    float h = 0.45f * std::sin(x * 0.13f + 1.3f) * std::cos(z * 0.11f) + 0.3f * std::sin((x + z) * 0.07f);
    h *= smoothstep(6, 14, std::hypot(x, z));
    h *= smoothstep(6, 11, std::hypot(x - HOME.x, z - HOME.z));
    h += HILL.h * std::exp(-((x - HILL.x) * (x - HILL.x) + (z - HILL.z) * (z - HILL.z)) / (2 * HILL.s * HILL.s));
    float dp = std::hypot(x - POND.x, z - POND.z);
    if (dp < POND.r + 3) {
        float k = smoothstep(POND.r - 0.5f, POND.r + 3, dp);
        h = h * k - 0.35f * (1 - k);
    }
    // keep the walk in front of the cave mouth flat
    h *= smoothstep(2, 6, std::hypot(x - CAVE_MOUTH.x + 2, z - CAVE_MOUTH.y - 1));
    h = std::max(h, mountainAt(x, z) * smoothstep(1.5f, 4, std::hypot(x - CAVE_MOUTH.x + 1, z - CAVE_MOUTH.y - 0.5f)));
    float r = std::hypot(x, z);
    // the rim of the bowl, except where the valley to the citadel comes in (south)
    float open = layout::corridor(x, z);
    if (r > WORLD_R - 4) h += std::pow(r - (WORLD_R - 4), 1.5f) * 0.4f * (1 - open);
    // blend into the wilds so the ground meets the valley without a step
    if (open > 0) h = glm::mix(h, layout::wildsRaw(x, z), open * smoothstep(WORLD_R - 10, WORLD_R, r));
    return h;
}

float heightAt(float x, float z) {
    // the crystal cave has its own floor
    if (isInCave(x)) return 0;
    glm::vec2 c = layout::cellCenter(x, z);
    return layout::toBlocks(rawHeight(c.x, c.y));
}

bool isIceAt(float x, float z) { return std::hypot(x - POND.x, z - POND.z) < POND.r; }

}  // namespace winter

// ---------------------------------------------------------------------------

WinterWorld::WinterWorld() {
    name = "winter";
    spawn = SPAWN;
    group->name = "winter-world";

    mSnow = lam("snow");
    mWood = lam("planks");
    mWoodDark = lam("planks", 0x8a7a72);
    mPlaster = lam("plaster");
    mRoof = lam("roof#a8423f");
    mRoofSnow = lam("roofSnow");
    mStone = lam("stone");
    mWindow = basic(glm::vec3(1), tex::get("window"));
    mBulbWarm = basic(hexc(0xffe2a8));
    mPost = lambertColor(hexc(0x2b2840));
    mCrate = lam("crate");
    mDoor = lam("door");

    buildSky();
    // the ground and the mountain terraces are blocks now (world/terrain.cpp)
    buildPond();
    buildTrees();
    buildLamps();
    for (int i = 0; i < 4; ++i)
        cabin(CABINS[i][0], CABINS[i][1], 4.2f, 3.6f, 2.4f, i % 2 == 0, i % 2 ? lam("roof#3f5a8a") : mRoof);
    buildHomeAndProps();
    buildPortal();
    crate(10.6f, -6.4f, 0);
    crate(11.8f, -7.3f, 1);
    crate(13.0f, -8.3f, 2);
    rock(20, -21, 0.6f);
    rock(21.6f, -22.9f, 1.2f);
    rock(23.3f, -24.6f, 1.8f);
    rock(25.2f, -26.4f, 2.5f, 0.95f);
    buildChristmasTree();
    buildSummit();
    buildCaveMouth();
    buildCave();

    // lantern Patty holds (only lit inside the cave)
    lantern_ = makePointLight(hexc(0xffc27a), 0, 9, 1.4f);
    addObject(lantern_);

    // spots for the companions you have to find
    petSpots["cham"] = {-34, 0, -5};
    petSpots["juice"] = {POND.x, 0, POND.z};
    petSpots["puppy"] = {-21, 0, 20.6f};
    // Ter's cat curls up beside a cabin (Ter mode)
    petSpots["slid"] = {-21, 0, 20.6f};
    for (auto& [k, v] : petSpots) v.y = groundAt(v.x, v.z, 99);
    snowmanSpot = {2.6f, heightAt(2.6f, 13), 13};
    colliders_.push_back({snowmanSpot.x, snowmanSpot.z, 0.55f});

    mapData.type = "winter";
    mapData.radius = WORLD_R;
    mapData.paths = PATHS;
    mapData.trees = &treeShapes_;
    mapData.pond = {POND.x, POND.z, POND.r};
    for (auto& c : CABINS) mapData.cabins.push_back({c[0], c[1]});
    mapData.home = {HOME.x, HOME.z};
    mapData.hill = {HILL.x, HILL.z, HILL.s};
    mapData.mountain = {MOUNTAIN.x, MOUNTAIN.z, MOUNTAIN.r, MOUNTAIN.h};
    mapData.caveMouth = CAVE_MOUTH;
    mapData.portal = {WINTER_PORTAL.x, WINTER_PORTAL.z};

    cur_ = goal_ = preset("night");
    timeOfDay = "night";
    applyTOD(1);
    updaters_.push_back([this](float, float dt, glm::vec3) { applyTOD(std::min(1.0f, dt * 2)); });
}

// ---------- sky, stars, moon, aurora, lights, fog ----------
void WinterWorld::buildSky() {
    skyGroup_ = makeGroup("winter-sky");
    addObject(skyGroup_);
    skyMat_ = std::make_shared<Material>();
    skyMat_->type = MaterialType::Custom;
    skyMat_->shader = sharedShader("sky");
    skyMat_->side = Side::Back;
    skyMat_->depthWrite = false;
    skyMat_->fog = false;
    skyMat_->uniforms = [this](const Shader& s) {
        s.set("uTop", cur_.top);
        s.set("uMid", cur_.mid);
        s.set("uHor", cur_.hor);
        s.set("uStops", glm::vec4(-0.02f, 0.18f, 0.18f, 0.75f));
    };
    sky_ = makeMesh(geo::sphere(400, 32, 16), skyMat_);
    sky_->renderOrder = -10;
    skyGroup_->add(sky_);

    // stars
    {
        const int n = 1400;
        auto g = std::make_shared<Geometry>();
        ExtraAttribute seed{"seed", 1, {}};
        for (int i = 0; i < n; ++i) {
            glm::vec3 v = randomDirection();
            v.y = std::abs(v.y) * 0.9f + 0.08f;
            v = glm::normalize(v) * 380.0f;
            g->position.push_back(v);
            seed.data.push_back(frand());
        }
        g->extra.push_back(seed);
        starsMat_ = std::make_shared<Material>();
        starsMat_->type = MaterialType::Custom;
        starsMat_->shader = sharedShader("stars");
        starsMat_->transparent = true;
        starsMat_->depthWrite = false;
        starsMat_->fog = false;
        starsMat_->uniforms = [this](const Shader& s) {
            float fade = cur_.stars * (1 - caveK_);
            s.set("uFade", fade);
        };
        auto stars = makePoints(g, starsMat_);
        stars->renderOrder = -9;
        skyGroup_->add(stars);
    }

    // moon
    {
        auto mm = basic(hexc(0xfff6dc));
        mm->fog = false;
        moon_ = makeMesh(geo::circle(14, 32), mm);
        moon_->position = {-160, 150, -260};
        skyGroup_->add(moon_);
        moon_->lookAt({0, 0, 0});
        moonHalo_ = glowSprite(hexc(0xd9c8ff), 110, 0.55f);
        moonHalo_->material()->fog = false;
        moonHalo_->position = moon_->position;
        skyGroup_->add(moonHalo_);
    }

    // aurora
    {
        auroraMat_ = std::make_shared<Material>();
        auroraMat_->type = MaterialType::Custom;
        auroraMat_->shader = sharedShader("aurora");
        auroraMat_->transparent = true;
        auroraMat_->depthWrite = false;
        auroraMat_->side = Side::Double;
        auroraMat_->blending = Blending::Additive;
        auroraMat_->fog = false;
        auroraMat_->uniforms = [this](const Shader& s) { s.set("uFade", cur_.aurora * (1 - caveK_)); };
        auto g = geo::plane(420, 70, 80, 1);
        for (int i = 0; i < 2; ++i) {
            auto a = makeMesh(g, auroraMat_);
            a->position = {40.0f - i * 60, 115.0f + i * 15, -260.0f + i * 30};
            a->rotation = {0.25f, 0.25f - i * 0.5f, 0};
            skyGroup_->add(a);
        }
    }

    // lights: hemisphere + moon (directional, casts the shadows)
    env.shadowHalf = 14;
    env.shadowNear = 1;
    env.shadowFar = 80;
    env.shadowBias = -0.0008f;
    env.shadowNormalBias = 0.03f;
    env.fogNear = 22;
    env.fogFar = 90;
}

// ---------- frozen lake + snow island in the middle (where Juice sleeps) ----------
void WinterWorld::buildPond() {
    auto im = lambert(tex::get("ice"));
    im->transparent = true;
    im->opacity = 0.95f;
    auto cg = geo::circle(POND.r + 0.3f, 48);
    cg->rotateX(-PI / 2).translate(POND.x, 0, POND.z);
    auto ig = geo::worldUV(cg, 0.5f);
    ig->translate(-POND.x, 0, -POND.z);
    auto ice = makeMesh(ig, im);
    ice->position = {POND.x, ICE_Y, POND.z};
    ice->receiveShadow = true;
    addObject(ice);

    // scratches on the ice
    auto lines = makeGroup();
    auto lm = basic(glm::vec3(1));
    lm->transparent = true;
    lm->opacity = 0.35f;
    for (int i = 0; i < 9; ++i) {
        auto l = makeMesh(geo::plane(0.05f, 1.5f + rand_.f() * 2.5f), lm);
        l->rotation = {-PI / 2, 0, rand_.f() * PI};
        float a = rand_.f() * PI * 2, d = 1.5f + rand_.f() * 4.5f;
        l->position = {POND.x + std::cos(a) * d, ICE_Y + 0.01f, POND.z + std::sin(a) * d};
        lines->add(l);
    }
    addObject(lines);

    auto isl = W(geo::sphere(1.3f, 12, 6, 0, PI * 2, 0, PI / 2), mSnow);
    isl->scale.y = 0.28f;
    isl->position = {POND.x, ICE_Y, POND.z};
    isl->receiveShadow = true;
    addObject(isl);
    platforms_.push_back({POND.x, POND.z, 1.05f, ICE_Y + 0.3f});
}

// ---------- pine forest (instanced, 420 seeded spots avoiding paths/houses/lake/mountain) ----------
void WinterWorld::buildTrees() {
    // 3 materials: 0 = bark, 1 = needles, 2 = snow
    std::vector<GeoPtr> parts;
    auto addPart = [&](GeoPtr g, float y) {
        g->translate(0, y, 0);
        parts.push_back(geo::worldUV(g, 1.4f));
    };
    addPart(geo::cylinder(0.16f, 0.22f, 0.9f, 6), 0.45f);
    const float tiers[4][3] = {{1.5f, 1.6f, 1.3f}, {1.15f, 1.4f, 2.2f}, {0.8f, 1.2f, 3.0f}, {0.45f, 0.9f, 3.65f}};
    std::vector<int> kinds = {0};
    for (auto& t : tiers) {
        addPart(geo::cone(t[0], t[1], 8), t[2]);
        kinds.push_back(1);
        addPart(geo::cone(t[0] * 0.55f, t[1] * 0.45f, 8), t[2] + t[1] * 0.34f);
        kinds.push_back(2);
    }
    auto treeGeo = geo::merge(parts, true);
    for (size_t i = 0; i < treeGeo->groups.size(); ++i) treeGeo->groups[i].materialIndex = kinds[i];
    std::vector<MatPtr> treeMat = {lam("bark"), lam("needles"), mSnow};

    std::vector<glm::vec3> avoid = {
        {0, 0, 10}, {HOME.x, HOME.z, 8}, {POND.x, POND.z, POND.r + 2.5f}, {HILL.x, HILL.z, 3.5f},
        {SPAWN.x, SPAWN.z, 5}, {-22, 18, 4}, {-34, -5, 3.5f},
        {12, -7.5f, 4}, {22.5f, -24, 5.5f},
        {MOUNTAIN.x, MOUNTAIN.z, MOUNTAIN.r * 0.55f}, {CAVE_MOUTH.x, CAVE_MOUTH.y, 6},
        // gate to the medieval realm
        {WINTER_PORTAL.x, WINTER_PORTAL.z, 7},
        // ice-tower parkour course
        {46, -14, 8},
    };
    for (auto& c : CABINS) avoid.push_back({c[0], c[1], 5.5f});
    struct Spot {
        float x, z, s, rot;
    };
    std::vector<Spot> spots;
    for (int tries = 0; spots.size() < 420 && tries++ < 12000;) {
        bool west = rand_() < 0.28;
        float a = west ? PI * (0.75f + rand_.f() * 0.5f) : rand_.f() * PI * 2;
        float d = west ? 14 + rand_.f() * 60 : 12 + std::sqrt(rand_.f()) * 76;
        float x = std::cos(a) * d, z = std::sin(a) * d;
        if (std::hypot(x, z) > 92) continue;
        if (distToPathsW(x, z) < 2.4f) continue;
        bool bad = false;
        for (auto& v : avoid)
            if (std::hypot(x - v.x, z - v.y) < v.z) {
                bad = true;
                break;
            }
        if (bad) continue;
        for (auto& s : spots)
            if (std::hypot(s.x - x, s.z - z) < (west ? 2.9f : 2.4f)) {
                bad = true;
                break;
            }
        if (bad) continue;
        float s = 0.75f + rand_.f() * 0.75f;
        float rot = rand_.f() * PI;
        spots.push_back({x, z, s, rot});
    }
    auto inst = makeInstanced(treeGeo, treeMat, spots.size());
    Object3D* instPtr = inst.get();
    for (size_t i = 0; i < spots.size(); ++i) {
        const Spot& s = spots[i];
        glm::quat q = glm::angleAxis(s.rot, glm::vec3(0, 1, 0));
        float sy = s.s * (0.9f + rand_.f() * 0.3f);
        float y = layout::footHeight(heightAt, s.x, s.z, 0.8f * s.s);
        inst->instances[i] = composeMatrix({s.x, y - 0.25f, s.z}, q, {s.s, sy, s.s});
        treeShapes_.push_back({s.x, s.z, y - 0.25f + 0.6f * s.s, 4.0f * s.s, 1.45f * s.s});
        TreeRef ref;
        ref.x = s.x;
        ref.z = s.z;
        ref.shape = (int)treeShapes_.size() - 1;
        if (std::hypot(s.x, s.z) < WORLD_R + 1) {
            colliders_.push_back({s.x, s.z, 0.45f * s.s + 0.15f, true});
            ref.collider = (int)colliders_.size() - 1;
        }
        ref.hide = [instPtr, i] {
            instPtr->instances[i] = glm::mat4(0);
            instPtr->instancesDirty = true;
        };
        trees_.push_back(ref);
    }
    inst->castShadow = inst->receiveShadow = true;
    // use the Cube World model when available
    CwTag tag;
    tag.name = "fir-tree";
    tag.h = 4.6f;
    tag.snow = true;
    inst->cw.push_back(tag);
    addObject(inst);
}

// ---------- street lamps ----------
void WinterWorld::lamp(float x, float z) {
    static GeoPtr post = geo::cylinder(0.05f, 0.07f, 2.2f, 6);
    static GeoPtr cap = geo::cone(0.22f, 0.2f, 4);
    static GeoPtr bulbG = geo::sphere(0.11f, 10, 8);
    float y = heightAt(x, z);
    auto g = makeGroup();
    auto p = makeMesh(post, mPost);
    p->position.y = 1.1f;
    p->castShadow = true;
    auto bulb = makeMesh(bulbG, mBulbWarm);
    bulb->position.y = 2.2f;
    auto c = makeMesh(cap, mPost);
    c->position.y = 2.38f;
    c->rotation.y = PI / 4;
    auto glow = glowSprite(hexc(0xffc27a), 1.8f, 0.8f);
    glow->position.y = 2.2f;
    lampGlows_.push_back(glow);
    g->add(p, bulb, c, glow);
    CwTag tag;
    tag.name = "street-light";
    tag.h = 2.55f;
    g->cw.push_back(tag);
    g->position = {x, y, z};
    addObject(g);
    colliders_.push_back({x, z, 0.18f});
}

void WinterWorld::buildLamps() {
    for (auto& p : PATHS)
        for (size_t i = 0; i + 1 < p.size(); ++i) {
            glm::vec2 a = p[i], b = p[i + 1];
            float len = glm::length(b - a);
            float nx = -(b.y - a.y) / len, nz = (b.x - a.x) / len;
            for (float d = 5; d < len - 2; d += 8) {
                float side = ((int)std::round(d / 8) % 2 ? 1.0f : -1.0f) * 1.7f;
                float x = a.x + (b.x - a.x) * d / len + nx * side, z = a.y + (b.y - a.y) * d / len + nz * side;
                if (std::hypot(x, z) < 7) continue;
                lamp(x, z);
            }
        }
}

// ---------- cabins ----------
void WinterWorld::cabin(float x, float z, float w, float d, float h, bool lights, const MatPtr& roofMat) {
    float facing = std::atan2(-x, -z);
    auto g = makeGroup();
    float y = heightAt(x, z);
    auto body = W(geo::box(w, h, d), mPlaster);
    body->position.y = h / 2;
    // stone base sunk into the ground for slopes
    auto base = W(geo::box(w + 0.2f, 0.8f, d + 0.2f), mStone);
    base->position.y = -0.2f;
    // gable roof
    float roofH = h * 0.65f;
    Shape shape;
    shape.moveTo(-w / 2 - 0.4f, 0).lineTo(0, roofH).lineTo(w / 2 + 0.4f, 0).lineTo(-w / 2 - 0.4f, 0);
    ExtrudeOptions eo;
    eo.depth = d + 0.6f;
    eo.bevelEnabled = false;
    auto roof = W(geo::extrude(shape, eo), roofMat);
    roof->position = {0, h, -d / 2 - 0.3f};
    Shape snowShape;
    snowShape.moveTo(-w / 2 - 0.5f, -0.05f)
        .lineTo(0, roofH + 0.18f)
        .lineTo(w / 2 + 0.5f, -0.05f)
        .lineTo(w / 2 + 0.2f, -0.05f)
        .lineTo(0, roofH - 0.1f)
        .lineTo(-w / 2 - 0.2f, -0.05f);
    eo.depth = d + 0.75f;
    auto snowRoof = W(geo::extrude(snowShape, eo), mRoofSnow);
    snowRoof->position = {0, h + 0.08f, -d / 2 - 0.37f};
    // chimney
    auto chim = W(geo::box(0.6f, 1.4f, 0.6f), mStone);
    chim->position = {w * 0.25f, h + roofH * 0.7f, -d * 0.15f};
    // door + windows
    auto door = makeMesh(geo::box(0.9f, 1.6f, 0.08f), mDoor);
    door->position = {0, 0.8f, d / 2 + 0.03f};
    g->add(body, base, roof, snowRoof, chim, door);
    static GeoPtr winGeo = geo::plane(0.75f, 0.75f);
    for (int sx : {-1, 1}) {
        auto win = makeMesh(winGeo, mWindow);
        win->position = {sx * w * 0.3f, h * 0.55f, d / 2 + 0.01f};
        auto frame = W(geo::box(0.9f, 0.9f, 0.05f), mWoodDark);
        frame->position = {sx * w * 0.3f, h * 0.55f, d / 2 - 0.01f};
        g->add(frame, win);
        auto sw = win->clone();
        sw->position = {sx * (w / 2 + 0.01f), h * 0.55f, 0};
        sw->rotation.y = sx * PI / 2;
        g->add(sw);
    }
    if (lights) {
        // string of coloured lights along the eaves
        const uint32_t cols[5] = {0xff4f6d, 0xffd76a, 0x5fd3ff, 0x8aff8a, 0xff8fe0};
        static GeoPtr bg = geo::sphere(0.07f, 6, 4);
        for (int i = 0; i <= 14; ++i) {
            float t = i / 14.0f;
            float bx = -w / 2 - 0.3f + t * (w + 0.6f);
            float by = h - 0.1f - std::pow(std::sin(t * PI * 7), 2.0f) * 0.18f;
            auto b = makeMesh(bg, basic(hexc(cols[i % 5])));
            b->position = {bx, by, d / 2 + 0.35f};
            g->add(b);
        }
    }
    g->traverse([](Object3D& o) {
        if (o.kind == ObjectKind::Mesh) o.castShadow = o.receiveShadow = true;
    });
    g->position = {x, y, z};
    g->rotation.y = facing;
    addObject(g);
    boxes_.push_back({x, z, w / 2 + 0.25f, d / 2 + 0.25f, facing, y + h + roofH});
}

// ---------- home anchor, kennel, stump ----------
void WinterWorld::buildHomeAndProps() {
    // the real home is the old Christmas room (added by home.cpp) — keep players out of its base
    colliders_.push_back({HOME.x, HOME.z, 4.3f});
    homeAnchorPos = {HOME.x, heightAt(HOME.x, HOME.z), HOME.z};
    doorGlow_ = glowSprite(hexc(0xffd2a0), 2.6f, 0);
    doorGlow_->position = {HOME_DOOR.x, heightAt(HOME_DOOR.x, HOME_DOOR.z) + 1.2f, HOME_DOOR.z - 1};
    addObject(doorGlow_);
    homeLight_ = makePointLight(hexc(0xffb070), 0, 12, 2);
    homeLight_->position = {HOME.x, 3, HOME.z + 3};
    addObject(homeLight_);

    // dog house
    {
        float x = -22, z = 19;
        auto g = makeGroup();
        auto b = W(geo::box(1.2f, 0.9f, 1.3f), lam("planks", 0xffa0a0));
        b->position.y = 0.45f;
        auto r1 = makeMesh(geo::box(0.9f, 0.08f, 1.5f), mSnow);
        r1->position = {-0.32f, 1.12f, 0};
        r1->rotation.z = 0.7f;
        auto r2 = r1->clone();
        r2->position.x = 0.32f;
        r2->rotation.z = -0.7f;
        auto hole = makeMesh(geo::circle(0.28f, 16), basic(hexc(0x1a1020)));
        hole->position = {0, 0.35f, 0.66f};
        g->add(b, r1, r2, hole);
        g->traverse([](Object3D& o) {
            if (o.kind == ObjectKind::Mesh) o.castShadow = true;
        });
        g->position = {x, heightAt(x, z), z};
        g->rotation.y = std::atan2(-x, -z);
        addObject(g);
        colliders_.push_back({x, z, 0.85f});
    }

    // tree stump in the woods (Cham perches here)
    {
        float x = -34, z = -5;
        auto stump = W(geo::cylinder(0.45f, 0.55f, 0.6f, 10), lam("bark"));
        auto top = W(geo::cylinder(0.46f, 0.46f, 0.06f, 10), mSnow);
        top->position.y = 0.32f;
        stump->add(top);
        stump->position = {x, heightAt(x, z) + 0.25f, z};
        stump->castShadow = true;
        addObject(stump);
        platforms_.push_back({x, z, 0.5f, heightAt(x, z) + 0.58f});
    }
}

// ---------- Ancient Time Portal to the medieval realm ----------
void WinterWorld::buildPortal() {
    float x = WINTER_PORTAL.x, z = WINTER_PORTAL.z, y = heightAt(x, z);
    auto portal = makeGroup();
    // two stone pillars
    auto pillarGeo = geo::worldUV(geo::box(1.2f, 5.2f, 1.2f));
    auto p1 = makeMesh(pillarGeo, mStone);
    p1->position = {-2.4f, 2.6f, 0};
    auto p2 = makeMesh(pillarGeo, mStone);
    p2->position = {2.4f, 2.6f, 0};
    // lintel
    auto lintel = W(geo::box(6.4f, 1.2f, 1.6f), mStone);
    lintel->position = {0, 5.0f, 0};
    // snow on top
    auto snowCap = W(geo::box(6.6f, 0.25f, 1.8f), mSnow);
    snowCap->position = {0, 5.7f, 0};
    // magic vortex inside the gate
    auto vm = basic(hexc(0x7fe8ff), tex::get("ice"));
    vm->transparent = true;
    vm->opacity = 0.75f;
    vm->side = Side::Double;
    auto vortex = makeMesh(geo::plane(3.8f, 4.4f), vm);
    vortex->position = {0, 2.4f, 0};
    auto glow = glowSprite(hexc(0x7fe8ff), 6.5f, 0.7f);
    glow->position = {0, 2.4f, 0};
    portal->add(p1, p2, lintel, snowCap, vortex, glow);
    portal->position = {x, y, z};
    addObject(portal);
    colliders_.push_back({x - 2.4f, z, 0.8f});
    colliders_.push_back({x + 2.4f, z, 0.8f});
    Object3D* g = glow.get();
    Object3D* v = vortex.get();
    updaters_.push_back([g, v](float t, float, glm::vec3) {
        g->material()->opacity = 0.6f + std::sin(t * 3) * 0.25f;
        v->rotation.z = std::sin(t * 1.2f) * 0.05f;
    });
}

// ---------- crates / rocks to jump on ----------
float WinterWorld::crate(float x, float z, int level) {
    const float s = 1.1f;
    float top = heightAt(x, z) - 0.05f + s * (level + 1);
    static GeoPtr box = geo::box(s, s, s);
    for (int i = 0; i <= level; ++i) {
        auto c = makeMesh(box, mCrate);
        c->position = {x, top - s / 2 - i * s, z};
        c->rotation.y = 0.3f * i;
        c->castShadow = c->receiveShadow = true;
        CwTag tag;
        tag.name = "crate";
        tag.hasFit = true;
        tag.fit = {s, s, s};
        tag.center = true;
        c->cw.push_back(tag);
        addObject(c);
    }
    auto cap = W(geo::box(s * 0.9f, 0.08f, s * 0.9f), mSnow);
    cap->position = {x, top + 0.02f, z};
    addObject(cap);
    platforms_.push_back({x, z, 0.66f, top});
    return top;
}

float WinterWorld::rock(float x, float z, float h, float r) {
    float base = heightAt(x, z) - 0.3f, top = base + h + 0.3f;
    auto m = W(geo::cylinder(r * 0.85f, r, h + 0.3f, 7), mStone);
    m->position = {x, base + (h + 0.3f) / 2, z};
    m->rotation.y = x;
    m->castShadow = m->receiveShadow = true;
    auto cap = W(geo::cylinder(r * 0.8f, r * 0.85f, 0.08f, 7), mSnow);
    cap->position.y = (h + 0.3f) / 2 + 0.03f;
    m->add(cap);
    addObject(m);
    platforms_.push_back({x, z, r * 0.85f, top});
    return top;
}

// ---------- Christmas tree in the square ----------
void WinterWorld::buildChristmasTree() {
    auto bigTree = makeGroup();
    auto green = lam("needles");
    auto trunk = W(geo::cylinder(0.35f, 0.5f, 1.4f, 8), lam("bark"));
    trunk->position.y = 0.7f;
    bigTree->add(trunk);
    const float tiers[5][3] = {{3.2f, 3.2f, 2.4f}, {2.6f, 2.8f, 4.1f}, {2.0f, 2.4f, 5.6f}, {1.35f, 2.0f, 7.0f}, {0.75f, 1.5f, 8.2f}};
    for (auto& t : tiers) {
        auto cg = geo::cone(t[0], t[1], 10);
        cg->translate(0, t[2], 0);
        auto c = W(cg, green);
        c->castShadow = true;
        auto sg = geo::cone(t[0] * 0.5f, t[1] * 0.4f, 10);
        sg->translate(0, t[2] + t[1] * 0.33f, 0);
        auto s = W(sg, mSnow);
        bigTree->add(c, s);
    }
    // star on top
    Shape star;
    for (int i = 0; i < 10; ++i) {
        float a = i / 10.0f * PI * 2 + PI / 2, rr = i % 2 ? 0.28f : 0.65f;
        if (i) star.lineTo(std::cos(a) * rr, std::sin(a) * rr);
        else star.moveTo(std::cos(a) * rr, std::sin(a) * rr);
    }
    ExtrudeOptions eo;
    eo.depth = 0.18f;
    eo.bevelEnabled = true;
    eo.bevelSize = 0.05f;
    eo.bevelThickness = 0.05f;
    eo.bevelSegments = 1;
    auto sgeo = geo::extrude(star, eo);
    sgeo->center();
    star_ = makeMesh(sgeo, basic(hexc(0x8a7a50)));
    star_->position.y = 9.35f;
    bigTree->add(star_);
    starGlow_ = glowSprite(hexc(0xffe08a), 4, 0);
    starGlow_->position.y = 9.35f;
    bigTree->add(starGlow_);

    // baubles
    const uint32_t ballCols[5] = {0xd7263d, 0xf4d35e, 0x3a86ff, 0xffffff, 0xb565f5};
    auto ballGeo = geo::sphere(0.17f, 12, 8);
    for (int i = 0; i < 26; ++i) {
        float t = rand_.f();
        float y = 1.6f + t * 6.2f;
        float r = 3.0f * (1 - t * 0.82f) * 0.9f;
        float a = rand_.f() * PI * 2;
        auto b = makeMesh(ballGeo, standard(hexc(ballCols[i % 5]), 0.2f, 0.5f));
        b->position = {std::cos(a) * r, y, std::sin(a) * r};
        bigTree->add(b);
    }
    // spiral of bulbs (lit progressively)
    const uint32_t cols[6] = {0xff4f6d, 0xffd76a, 0x5fd3ff, 0x9dff8a, 0xff8fe0, 0xffffff};
    auto bulbGeo = geo::sphere(0.09f, 8, 6);
    const int N = 90;
    for (int i = 0; i < N; ++i) {
        float t = (float)i / N;
        float y = 1.4f + t * 7.0f;
        float r = 3.15f * (1 - t * 0.85f) * 0.92f + 0.08f;
        float a = t * PI * 2 * 6;
        glm::vec3 color = hexc(cols[i % 6]);
        auto mat = basic(color * 0.18f);
        auto b = makeMesh(bulbGeo, mat);
        b->position = {std::cos(a) * r, y, std::sin(a) * r};
        auto glow = glowSprite(color, 0.9f, 0);
        glow->position = b->position;
        bigTree->add(b, glow);
        bulbs_.push_back({mat, glow, color, rand_.f() * 6});
    }
    addObject(bigTree);
    colliders_.push_back({0, 0, 2.9f});
    treeLight_ = makePointLight(hexc(0xffc48a), 0.3f, 16, 2);
    treeLight_->position = {0, 3, 0};
    addObject(treeLight_);
    updaters_.push_back([this](float t, float dt, glm::vec3) {
        treeShown_ += (treeProgress_ - treeShown_) * std::min(1.0f, dt * 1.5f);
        float lit = treeShown_ * bulbs_.size();
        for (size_t i = 0; i < bulbs_.size(); ++i) {
            auto& b = bulbs_[i];
            float on = std::clamp(lit - (float)i, 0.0f, 1.0f);
            float tw = 0.75f + 0.25f * std::sin(t * 3 + b.phase);
            b.mat->color = b.color * (0.18f + 0.82f * on * tw);
            b.glow->material()->opacity = on * tw * 0.9f;
        }
        treeLight_->intensity = 0.3f + treeShown_ * 1.7f;
        float full = smoothstep(0.98f, 1, treeShown_);
        star_->material()->color = {0.54f + 0.46f * full, 0.48f + 0.4f * full, 0.31f + 0.1f * full};
        starGlow_->material()->opacity = full * (0.8f + 0.2f * std::sin(t * 2));
        star_->rotation.y = t * 0.6f;
    });
}

// ---------- summit: flag, bench, lamps ----------
void WinterWorld::buildSummit() {
    float top = heightAt(MOUNTAIN.x, MOUNTAIN.z);
    auto g = makeGroup();
    auto pole = makeMesh(geo::cylinder(0.05f, 0.06f, 3.2f, 6), mPost);
    pole->position = {0, 1.6f, 0};
    auto fm = lambertColor(hexc(0xff4f8b));
    fm->side = Side::Double;
    auto flagGeo = geo::plane(1.2f, 0.7f, 6, 1);
    // waves every frame
    flagGeo->dynamic = true;
    auto flag = makeMesh(flagGeo, fm);
    flag->position = {0.62f, 2.8f, 0};
    auto bench = W(geo::box(1.6f, 0.12f, 0.5f), mWood);
    bench->position = {-1.4f, 0.45f, 1.2f};
    auto legs = W(geo::box(1.4f, 0.4f, 0.4f), mWoodDark);
    legs->position = {-1.4f, 0.2f, 1.2f};
    g->add(pole, flag, bench, legs);
    g->traverse([](Object3D& o) {
        if (o.kind == ObjectKind::Mesh) o.castShadow = true;
    });
    g->position = {MOUNTAIN.x, top, MOUNTAIN.z};
    addObject(g);
    std::vector<glm::vec3> base = flagGeo->position;
    updaters_.push_back([flagGeo, base](float t, float, glm::vec3) {
        for (size_t i = 0; i < flagGeo->position.size(); ++i) {
            float x = base[i].x + 0.6f;
            flagGeo->position[i].z = std::sin(t * 5 + x * 4) * 0.12f * x;
        }
        flagGeo->markDirty();
    });
    lamp(MOUNTAIN.x + 2, MOUNTAIN.z + 2);
    lamp(MOUNTAIN.x - 2.5f, MOUNTAIN.z - 1.5f);
    colliders_.push_back({MOUNTAIN.x, MOUNTAIN.z, 0.15f});
}

// ---------- cave mouth (stone arch) ----------
void WinterWorld::buildCaveMouth() {
    auto g = makeGroup();
    float y = heightAt(CAVE_MOUTH.x, CAVE_MOUTH.y);
    auto rockBox = [&](float w, float h, float d, float x, float yy, float z) {
        auto m = W(geo::box(w, h, d), mStone);
        m->position = {x, yy, z};
        m->castShadow = m->receiveShadow = true;
        g->add(m);
    };
    rockBox(1.4f, 3.6f, 2.2f, 0, 1.8f, -1.9f);
    rockBox(1.4f, 3.6f, 2.2f, 0, 1.8f, 1.9f);
    rockBox(1.6f, 1.2f, 5.6f, 0, 3.9f, 0);
    auto snowCap = W(geo::box(1.8f, 0.25f, 5.8f), mSnow);
    snowCap->position = {0, 4.6f, 0};
    g->add(snowCap);
    auto dark = makeMesh(geo::plane(2.6f, 3.4f), basic(hexc(0x07060d)));
    dark->position = {0.3f, 1.7f, 0};
    dark->rotation.y = PI / 2;
    g->add(dark);
    auto glow = glowSprite(hexc(0x7fe8ff), 3, 0.5f);
    glow->position = {0.6f, 1.5f, 0};
    g->add(glow);
    for (auto [ix, iz] : {std::pair{-0.8f, -1.2f}, std::pair{-0.8f, 1.3f}}) {
        auto ic = makeMesh(geo::cone(0.12f, 0.6f, 5), basic(hexc(0xbff4ff)));
        ic->position = {ix, 3.1f, iz};
        ic->rotation.x = PI;
        g->add(ic);
    }
    g->position = {CAVE_MOUTH.x, y, CAVE_MOUTH.y};
    // the mouth faces west
    g->rotation.y = PI;
    addObject(g);
    colliders_.push_back({CAVE_MOUTH.x, CAVE_MOUTH.y - 1.9f, 1.2f});
    colliders_.push_back({CAVE_MOUTH.x, CAVE_MOUTH.y + 1.9f, 1.2f});
}

// ---------- crystal cave interior (warp in/out) ----------
void WinterWorld::buildCave() {
    {
        auto pg = geo::plane(80, 80);
        pg->rotateX(-PI / 2).translate(CAVE.x, 0, CAVE.y);
        auto floor = makeMesh(geo::worldUV(pg, 0.8f), lam("stone", 0x6a6478));
        floor->receiveShadow = true;
        addObject(floor);
    }
    std::vector<glm::vec2> path;
    for (auto& p : CAVE_PATH) path.push_back(CAVE + p);
    auto distToCave = [&](float x, float z) {
        float d = INFINITY;
        for (size_t i = 0; i + 1 < path.size(); ++i) d = std::min(d, distToSegment({x, z}, path[i], path[i + 1]));
        return d;
    };
    // rock walls: boulders around the path (uneven width)
    auto rockGeo = geo::worldUV(geo::dodecahedron(1));
    auto rockMat = lam("stone", 0x9a92ae);
    std::vector<glm::vec4> rocks;
    for (float x = -6; x <= 32; x += 1.3f)
        for (float z = -26; z <= 12; z += 1.3f) {
            float wx = CAVE.x + x + (rand_.f() - 0.5f) * 0.8f;
            float wz = CAVE.y + z + (rand_.f() - 0.5f) * 0.8f;
            float d = distToCave(wx, wz);
            float half = 2.4f + std::sin(wx * 0.4f) * 0.8f + (wz < CAVE.y - 14 ? 1.6f : 0);
            if (d < half || d > half + 2.6f) continue;
            float r = 0.9f + rand_.f() * 0.7f;
            float h = 1.4f + rand_.f() * 1.6f;
            rocks.push_back({wx, wz, r, h});
        }
    auto inst = makeInstanced(rockGeo, {rockMat}, rocks.size());
    for (size_t i = 0; i < rocks.size(); ++i) {
        float x = rocks[i].x, z = rocks[i].y, r = rocks[i].z, h = rocks[i].w;
        float ex = rand_.f(), ey = rand_.f() * 6, ez = rand_.f();
        inst->instances[i] = composeMatrix({x, h * 0.4f, z}, eulerToQuat({ex, ey, ez}), {r, h, r});
        colliders_.push_back({x, z, r * 0.85f});
    }
    inst->castShadow = inst->receiveShadow = true;
    addObject(inst);

    // glowing crystals
    const uint32_t crystalCols[3] = {0x7fe8ff, 0xb58bff, 0x9dffd8};
    auto crystalGeo = geo::octahedron(0.35f);
    crystalGeo->scale(0.6f, 1.8f, 0.6f);
    std::vector<ObjPtr> glows;
    auto placeCrystal = [&](float x, float z, uint32_t col, float size = 1) {
        auto grp = makeGroup();
        for (int i = 0; i < 3; ++i) {
            auto c = makeMesh(crystalGeo, basic(hexc(col)));
            float px = (rand_.f() - 0.5f) * 0.5f;
            float pz = (rand_.f() - 0.5f) * 0.5f;
            c->position = {px, 0.4f * size, pz};
            float rx = (rand_.f() - 0.5f) * 0.7f;
            float ry = rand_.f() * 3;
            float rz = (rand_.f() - 0.5f) * 0.7f;
            c->rotation = {rx, ry, rz};
            c->scale = glm::vec3(size * (0.6f + rand_.f() * 0.6f));
            grp->add(c);
        }
        auto gl = glowSprite(hexc(col), 2.4f * size, 0.7f);
        gl->position.y = 0.6f * size;
        grp->add(gl);
        grp->position = {x, 0, z};
        addObject(grp);
        colliders_.push_back({x, z, 0.35f * size});
        return gl;
    };
    for (size_t i = 1; i < path.size(); ++i) {
        glm::vec2 a = path[i - 1], b = path[i];
        float nx = -(b.y - a.y), nz = b.x - a.x, l = std::hypot(nx, nz);
        float side = i % 2 ? 1.0f : -1.0f;
        glows.push_back(placeCrystal((a.x + b.x) / 2 + nx / l * 1.9f * side, (a.y + b.y) / 2 + nz / l * 1.9f * side, crystalCols[i % 3]));
    }
    // frozen underground lake + giant crystals
    glm::vec2 lake(CAVE.x + 19, CAVE.y - 16);
    {
        auto lg = geo::circle(3.4f, 24);
        lg->rotateX(-PI / 2).translate(lake.x, 0.02f, lake.y);
        auto lm = lambert(tex::get("ice"));
        lm->emissive = hexc(0x1c4a66);
        lm->emissiveMap = tex::get("ice");
        addObject(makeMesh(geo::worldUV(lg, 0.5f), lm));
        platforms_.push_back({lake.x, lake.y, 3.3f, 0.03f});
    }
    glows.push_back(placeCrystal(lake.x, lake.y - 4.6f, 0x7fe8ff, 2.2f));
    glows.push_back(placeCrystal(lake.x + 4.2f, lake.y + 1, 0xb58bff, 1.6f));
    // light shaft from a hole in the ceiling
    {
        auto sm = basic(hexc(0xbfe8ff));
        sm->transparent = true;
        sm->opacity = 0.12f;
        sm->depthWrite = false;
        sm->blending = Blending::Additive;
        sm->side = Side::Double;
        auto shaft = makeMesh(geo::cylinder(1.2f, 2.2f, 12, 16, 1, true), sm);
        shaft->position = {lake.x, 6, lake.y};
        addObject(shaft);
    }
    // stalagmites
    for (int i = 0; i < 18; ++i) {
        float k = (i + 0.5f) / 18 * (path.size() - 1);
        glm::vec2 a = path[(size_t)std::floor(k)], b = path[std::min(path.size() - 1, (size_t)std::floor(k) + 1)];
        float f = k - std::floor(k);
        float x = a.x + (b.x - a.x) * f + (rand_.f() - 0.5f) * 3.5f;
        float z = a.y + (b.y - a.y) * f + (rand_.f() - 0.5f) * 3.5f;
        if (distToCave(x, z) < 1.2f) continue;
        float r = 0.25f + rand_.f() * 0.2f;
        float h = 0.8f + rand_.f() * 1.2f;
        auto st = W(geo::cone(r, h, 6), rockMat);
        st->position = {x, 0.5f, z};
        st->castShadow = true;
        addObject(st);
    }
    // three crystal lights
    const std::tuple<float, float, uint32_t> lights[3] = {
        {CAVE.x + 2, CAVE.y - 10, 0x7fe8ff}, {CAVE.x + 16, CAVE.y - 18, 0xb58bff}, {CAVE.x + 22, CAVE.y + 1, 0x9dffd8}};
    for (auto [x, z, col] : lights) {
        auto l = makePointLight(hexc(col), 0, 12, 1.6f);
        l->position = {x, 2.2f, z};
        addObject(l);
        caveLights_.push_back(l);
    }
    updaters_.push_back([glows](float t, float, glm::vec3) {
        for (size_t i = 0; i < glows.size(); ++i) glows[i]->material()->opacity = 0.5f + std::sin(t * 1.7f + i) * 0.2f;
    });
}

// ---------------------------------------------------------------------------
// physics helpers
// ---------------------------------------------------------------------------

float WinterWorld::groundAt(float x, float z, float y) const {
    float g = heightAt(x, z);
    if (isIceAt(x, z)) g = std::max(g, ICE_Y);
    for (auto& p : platforms_)
        if (y >= p.top - 0.45f && std::hypot(x - p.x, z - p.z) < p.r + 0.15f) g = std::max(g, p.top);
    return extraGround(x, z, y, g);
}

void WinterWorld::resolveCollisions(glm::vec3& pos, float radius) const {
    for (auto& c : colliders_) {
        if (c.removed || (skipTrees_ && c.tree)) continue;
        float dx = pos.x - c.x, dz = pos.z - c.z, d = std::hypot(dx, dz), m = c.r + radius;
        if (d < m && d > 1e-4f) {
            pos.x = c.x + dx / d * m;
            pos.z = c.z + dz / d * m;
        }
    }
    for (auto& p : platforms_) {
        if (pos.y >= p.top - 0.45f) continue;
        float dx = pos.x - p.x, dz = pos.z - p.z, d = std::hypot(dx, dz), m = p.r + radius;
        if (d < m && d > 1e-4f) {
            pos.x = p.x + dx / d * m;
            pos.z = p.z + dz / d * m;
        }
    }
    for (auto& b : boxes_) {
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
    if (isInCave(pos.x)) return;
    float r = std::hypot(pos.x, pos.z);
    // the south gate opens onto the valley
    if (r > WORLD_R && layout::corridor(pos.x, pos.z) < 0.5f) {
        pos.x *= WORLD_R / r;
        pos.z *= WORLD_R / r;
    }
}

bool WinterWorld::isFree(float x, float z, float r, bool ignoreTrees) const {
    if (isInCave(x) || isIceAt(x, z) || distToPathsW(x, z) < r + 1.2f) return false;
    glm::vec3 p(x, heightAt(x, z), z);
    skipTrees_ = ignoreTrees;
    resolveCollisions(p, r);
    skipTrees_ = false;
    return p.x == x && p.z == z;
}

Surface WinterWorld::surfaceAt(float x, float z) const {
    // trodden paths: snow tinted brown
    if (distToPathsW(x, z) < 1.1f) return {Ground::Snow, glm::mix(glm::vec3(1), hexc(0xa8968c), 0.8f)};
    return {Ground::Snow};
}

std::string WinterWorld::zoneAt(float x, float z) const {
    if (isInCave(x)) return "cave";
    if (std::hypot(x - WINTER_PORTAL.x, z - WINTER_PORTAL.z) < 6) return "portal";
    float md = std::hypot(x - MOUNTAIN.x, z - MOUNTAIN.z);
    if (md < 5) return "summit";
    if (md < MOUNTAIN.r) return "mountain";
    if (std::hypot(x - HOME.x, z - HOME.z) < 9) return "home";
    if (std::hypot(x - POND.x, z - POND.z) < POND.r + 3) return "lake";
    if (std::hypot(x - HILL.x, z - HILL.z) < HILL.s * 1.5f) return "hill";
    if (std::hypot(x, z) < 11) return "square";
    if (std::hypot(x + 22, z - 19) < 6) return "kennel";
    if (x > 17 && z < -17) return "steps";
    if (x < -20) return "woods";
    return "meadow";
}

void WinterWorld::setHomeReady(bool on) {
    doorGlow_->material()->opacity = on ? 0.6f : 0;
    homeLight_->intensity = on ? 2.2f : 0;
}

// ---------------------------------------------------------------------------
// time of day
// ---------------------------------------------------------------------------

WinterWorld::Tod WinterWorld::preset(const std::string& n) {
    struct P {
        uint32_t top, mid, hor, fog, hemiS, hemiG;
        float hemiI;
        uint32_t sun;
        float sunI;
        glm::vec3 dir;
        float stars, aurora, moon, lamps;
        uint32_t win;
    };
    P p;
    if (n == "dusk") p = {0x23205a, 0x8a4f8f, 0xffa66b, 0x7a5585, 0xc9c0ff, 0x4a3a6a, 0.5f, 0xffc6a0, 0.8f, {0.9f, 0.35f, -0.3f}, 0.35f, 0, 0.4f, 0.65f, 0xffc98a};
    else if (n == "day") p = {0x3f7fd6, 0x86b6ee, 0xd8e6fb, 0xb4c6e6, 0xdfe8ff, 0x6f7fb0, 0.55f, 0xfff1dc, 0.75f, {0.4f, 1, 0.5f}, 0, 0, 0, 0.15f, 0xb0a690};
    else p = {0x08061f, 0x291a5c, 0x7f54ad, 0x2c2156, 0x7d8cf0, 0x2e2258, 0.32f, 0xaab6ff, 0.42f, {-0.45f, 1, 0.35f}, 1, 1, 1, 0.8f, 0xffd9a0};
    Tod t;
    t.top = hexc(p.top);
    t.mid = hexc(p.mid);
    t.hor = hexc(p.hor);
    t.fog = hexc(p.fog);
    t.hemiS = hexc(p.hemiS);
    t.hemiG = hexc(p.hemiG);
    t.sun = hexc(p.sun);
    t.win = hexc(p.win);
    t.dir = glm::normalize(p.dir);
    t.hemiI = p.hemiI;
    t.sunI = p.sunI;
    t.stars = p.stars;
    t.aurora = p.aurora;
    t.moon = p.moon;
    t.lamps = p.lamps;
    return t;
}

void WinterWorld::setTimeOfDay(const std::string& n) {
    if (n != "day" && n != "dusk" && n != "night") return;
    timeOfDay = n;
    goal_ = preset(n);
}

void WinterWorld::applyTOD(float k) {
    auto L = [k](auto& a, const auto& b) { a += (b - a) * k; };
    L(cur_.top, goal_.top);
    L(cur_.mid, goal_.mid);
    L(cur_.hor, goal_.hor);
    L(cur_.fog, goal_.fog);
    L(cur_.hemiS, goal_.hemiS);
    L(cur_.hemiG, goal_.hemiG);
    L(cur_.sun, goal_.sun);
    L(cur_.win, goal_.win);
    L(cur_.dir, goal_.dir);
    L(cur_.hemiI, goal_.hemiI);
    L(cur_.sunI, goal_.sunI);
    L(cur_.stars, goal_.stars);
    L(cur_.aurora, goal_.aurora);
    L(cur_.moon, goal_.moon);
    L(cur_.lamps, goal_.lamps);

    env.fogColor = cur_.fog;
    env.hemiSky = cur_.hemiS;
    env.hemiGround = cur_.hemiG;
    env.hemiIntensity = cur_.hemiI;
    env.sunColor = cur_.sun;
    env.sunIntensity = cur_.sunI;
    moon_->visible = cur_.moon > 0.02f;
    moon_->material()->opacity = cur_.moon;
    moon_->material()->transparent = true;
    moonHalo_->material()->opacity = 0.55f * cur_.moon;
    for (auto& g : lampGlows_) g->material()->opacity = cur_.lamps;
    mWindow->color = cur_.win;
    // inside the cave: darker, black fog, only crystals + lantern
    if (caveK_ > 0) {
        env.hemiIntensity *= 1 - caveK_ * 0.75f;
        env.sunIntensity *= 1 - caveK_ * 0.85f;
        env.fogColor = glm::mix(env.fogColor, hexc(0x0c0a1c), caveK_);
    }
    env.fogNear = 22 - caveK_ * 12;
    env.fogFar = 90 - caveK_ * 55;
    // no sky inside the cave
    bool outside = caveK_ < 0.5f;
    sky_->visible = outside;
    moon_->visible = moon_->visible && outside;
    moonHalo_->visible = outside;
    for (auto& l : caveLights_) l->intensity = caveK_ * 2.2f;
    lantern_->intensity = caveK_ * 1.6f;
}

void WinterWorld::update(float t, float dt, glm::vec3 focus) {
    caveK_ += ((isInCave(focus.x) ? 1.0f : 0.0f) - caveK_) * std::min(1.0f, dt * 3);
    lantern_->position = {focus.x, focus.y + 1.6f, focus.z};
    for (auto& u : updaters_) u(t, dt, focus);
    glm::vec3 dir = glm::normalize(cur_.dir);
    env.sunPosition = focus + dir * 30.0f;
    env.sunTarget = focus;
}
