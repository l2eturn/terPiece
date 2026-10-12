// Medieval citadel — port of buildMedievalWorld() in patty-project/xmas-app/web/js/medieval.js.
// Built in the same order as the JS so the seeded rng (420) gives the same clouds / roses / oak trees.
#include "world/medieval.h"
#include <cmath>
#include "art/glow.h"
#include "art/textures.h"
#include "gfx/shader.h"
#include "world/layout.h"

using namespace medieval;

namespace {

struct Moat {
    float inner, outer, depth;
};
// distances from the castle wall (m)
const Moat MOAT{1.2f, 7.2f, 1.3f};
const glm::vec3 FOUNTAIN(0, 8, 3.5f);
const Rect4 TAVERN{-24, 6, 7.8f, 6.2f, 5.6f};
const glm::vec3 ARCHERY(23, 5, 9);
const Rect4 GARDEN{20, 27, 14, 12, 0};
struct Bell {
    float x, z, s, h;
};
const Bell BELL_TOWER{-19, 27, 3.8f, 18};

const std::vector<std::vector<glm::vec2>> PATHS = {
    // main road from the town gate to the fountain and the drawbridge
    {{0, 78}, {0, 54}, {0, 24}, {0, -11}},
    // to the tavern
    {{0, 8}, {-22, 6}},
    // to the training grounds
    {{0, 8}, {21, 5}},
    // to the rose garden
    {{0, 20}, {18, 26}},
    // to the bell tower
    {{0, 20}, {-17, 26}},
    // west courtyard walk
    {{-12, -22}, {-12, -8}, {-22, -6}},
    // east courtyard walk
    {{12, -22}, {12, -8}, {22, -6}},
};

float distToPathsM(float x, float z) { return distToPaths(PATHS, x, z); }

MatPtr lam(const std::string& t, uint32_t color = 0xffffff) { return lambert(tex::get(t), hexc(color)); }
ObjPtr W(const GeoPtr& g, const MatPtr& m) { return makeMesh(geo::worldUV(g), m); }

}  // namespace

namespace medieval {

// the smooth shape of the town in its own coordinates (heightAt() turns it into blocks)
float rawHeight(float x, float z) {
    // moat: a ring of water around the castle walls (distance to the castle rectangle)
    float qx = std::abs(x - CASTLE.x) - CASTLE.w / 2, qz = std::abs(z - CASTLE.z) - CASTLE.d / 2;
    // raised castle base
    if (qx < 0 && qz < 0) return 0.4f;
    float out = std::hypot(std::max(qx, 0.0f), std::max(qz, 0.0f));
    if (out > MOAT.inner - 0.6f && out < MOAT.outer + 1) {
        // the drawbridge spans the moat in front of the gate
        if (std::abs(x - DRAWBRIDGE.x) < DRAWBRIDGE.w / 2 + 0.3f && z > CASTLE.z) return 0.05f;
        float k = smoothstep(MOAT.inner - 0.6f, MOAT.inner + 0.4f, out) * (1 - smoothstep(MOAT.outer, MOAT.outer + 1, out));
        return -MOAT.depth * k;
    }
    // rolling hills outside the town
    float h = 0.25f * std::sin(x * 0.08f + 0.5f) * std::cos(z * 0.07f) + 0.15f * std::sin((x + z) * 0.05f);
    // flat market square and streets
    h *= smoothstep(16, 38, std::hypot(x, z - 8));
    // swan lake (wavy shore)
    {
        float lx = x - LAKE.x, lz = z - LAKE.z, a = std::atan2(lz, lx);
        float dw = std::hypot(lx, lz) / (LAKE.r * (1 + 0.12f * std::sin(a * 3 + 0.7f) + 0.06f * std::sin(a * 5)));
        if (dw < 1.3f) {
            float k = 1 - smoothstep(0.82f, 1.12f, dw);
            h = h * (1 - k) - 1.4f * k;
        }
    }
    float r = std::hypot(x, z);
    // the rim, except where the valley from the snow village comes in (local +Z, the gate side)
    float open = layout::corridor(x, z);
    if (r > WORLD_R - 5) h += std::pow(r - (WORLD_R - 5), 1.4f) * 0.35f * (1 - open);
    if (open > 0) {
        glm::vec2 g = layout::medievalToGlobal({x, z});
        h = glm::mix(h, layout::wildsRaw(g.x, g.y), open * smoothstep(WORLD_R - 10, WORLD_R, r));
    }
    return h;
}

float heightAt(float x, float z) {
    // the block grid is global: snap there, then sample the town's shape at that column's centre
    glm::vec2 g = layout::medievalToGlobal({x, z});
    glm::vec2 l = layout::medievalToLocal(layout::cellCenter(g.x, g.y));
    return layout::toBlocks(rawHeight(l.x, l.y));
}

}  // namespace medieval

// all the building code lives here so the materials can be shared like M in the JS
struct MedievalWorld::Impl {
    MedievalWorld& w;
    MatPtr grass, cobble, dirt, stone, stoneSmooth, wood, woodDark, plaster, slate, roofRed, water, iron, crate, barrel, gold, torchFire, post,
        carpet, door, window;
    explicit Impl(MedievalWorld& world) : w(world) {
        grass = lam("grass");
        cobble = lam("cobble");
        dirt = lam("dirt");
        stone = lam("castleStone");
        stoneSmooth = lam("stone");
        wood = lam("planks");
        woodDark = lam("planks", 0x6e5240);
        plaster = lam("plaster");
        slate = lam("roofSlate");
        roofRed = lam("roof#9e3430");
        water = lambert(tex::get("water"));
        water->transparent = true;
        water->opacity = 0.88f;
        iron = lambert(tex::get("ironGrate"));
        iron->transparent = true;
        crate = lam("crate");
        barrel = lam("barrel");
        gold = standard(hexc(0xffd76a), 0.3f, 0.8f);
        torchFire = basic(hexc(0xffaa33));
        post = lambertColor(hexc(0x2e241c));
        carpet = lam("carpet");
        door = lam("door");
        window = basic(glm::vec3(1), tex::get("window"));
        w.mWindow_ = window;
    }
    void add(const ObjPtr& o) { w.addObject(o); }
    Rng& rand() { return w.rand_; }

    void sky();
    void moatAndBridge();
    void torch(float x, float y, float z, float facing = 0);
    void castle();
    void lake();
    void fountain();
    ObjPtr stall(float x, float z, float angle, uint32_t c1, uint32_t c2, const std::string& cw);
    void market();
    void tavern();
    void archery();
    void garden();
    void bellTower();
    void portal();
    void trees();
    void outerWalls();
};

void MedievalWorld::Impl::sky() {
    w.skyGroup_ = makeGroup("medieval-sky");
    add(w.skyGroup_);
    w.skyMat_ = std::make_shared<Material>();
    auto& m = w.skyMat_;
    m->type = MaterialType::Custom;
    m->shader = sharedShader("sky");
    m->side = Side::Back;
    m->depthWrite = false;
    m->fog = false;
    // capture the world, not this short-lived builder
    MedievalWorld* mw = &w;
    m->uniforms = [mw](const Shader& s) {
        s.set("uTop", mw->cur_.top);
        s.set("uMid", mw->cur_.mid);
        s.set("uHor", mw->cur_.hor);
        s.set("uStops", glm::vec4(-0.02f, 0.22f, 0.22f, 0.8f));
    };
    auto skyMesh = makeMesh(geo::sphere(400, 32, 16), m);
    skyMesh->renderOrder = -10;
    w.skyGroup_->add(skyMesh);

    // sun / moon
    auto sm = basic(hexc(0xfffae6));
    sm->fog = false;
    w.sunMesh_ = makeMesh(geo::circle(16, 32), sm);
    w.sunMesh_->position = {-140, 160, -260};
    w.skyGroup_->add(w.sunMesh_);
    w.sunMesh_->lookAt({0, 0, 0});
    w.sunHalo_ = glowSprite(hexc(0xffe9ad), 130, 0.6f);
    w.sunHalo_->material()->fog = false;
    w.sunHalo_->position = w.sunMesh_->position;
    w.skyGroup_->add(w.sunHalo_);

    // slow clouds
    auto cloudMat = basic(glm::vec3(1));
    cloudMat->transparent = true;
    cloudMat->opacity = 0.28f;
    cloudMat->fog = false;
    auto clouds = makeGroup("clouds");
    for (int i = 0; i < 9; ++i) {
        float cw = 60 + rand().f() * 40;
        float ch = 24 + rand().f() * 16;
        auto c = makeMesh(geo::plane(cw, ch), cloudMat);
        float px = (rand().f() - 0.5f) * 450;
        float py = 110 + rand().f() * 40;
        float pz = -180 - rand().f() * 160;
        c->position = {px, py, pz};
        c->rotation.x = 0.25f;
        clouds->add(c);
    }
    w.skyGroup_->add(clouds);
    Object3D* cl = clouds.get();
    w.updaters_.push_back([cl](float, float dt, glm::vec3) {
        for (size_t i = 0; i < cl->children.size(); ++i) {
            auto& c = cl->children[i];
            c->position.x += dt * (1.2f + i * 0.2f);
            if (c->position.x > 240) c->position.x = -240;
        }
    });

    // lights: shadow camera of the directional light
    w.env.shadowHalf = 16;
    w.env.shadowNear = 1;
    w.env.shadowFar = 90;
    w.env.shadowBias = -0.0008f;
    w.env.shadowNormalBias = 0.03f;
    w.env.fogNear = 22;
    w.env.fogFar = 90;
}

void MedievalWorld::Impl::moatAndBridge() {
    // one water sheet below ground level -> only visible where the ground dips into the moat
    {
        float span = 2 * (MOAT.outer + 1.5f);
        auto pg = geo::plane(CASTLE.w + span, CASTLE.d + span);
        pg->rotateX(-PI / 2).translate(CASTLE.x, -0.45f, CASTLE.z);
        add(makeMesh(geo::worldUV(pg, 0.5f), water));
    }
    // drawbridge
    auto g = makeGroup("drawbridge");
    float bw = DRAWBRIDGE.w, bd = DRAWBRIDGE.d, top = DRAWBRIDGE.top;
    // stone pier under the bridge
    auto stoneBase = W(geo::box(bw + 0.8f, 1.4f, bd + 0.4f), stoneSmooth);
    stoneBase->position = {0, -0.5f, 0};
    // wooden deck
    auto deck = W(geo::box(bw, 0.35f, bd), wood);
    deck->position = {0, top - 0.17f, 0};
    // wooden railings on both sides
    for (float sx : {-bw / 2 + 0.15f, bw / 2 - 0.15f}) {
        auto rail = W(geo::box(0.2f, 0.85f, bd), woodDark);
        rail->position = {sx, top + 0.35f, 0};
        g->add(rail);
        for (int i = -4; i <= 4; i += 2) {
            auto p = W(geo::box(0.25f, 0.9f, 0.25f), woodDark);
            p->position = {sx, top + 0.35f, (float)i};
            g->add(p);
        }
    }
    // iron chains
    auto chainMat = basic(hexc(0x3a3c48));
    for (float sx : {-bw / 2 + 0.4f, bw / 2 - 0.4f}) {
        auto chain = makeMesh(geo::cylinder(0.04f, 0.04f, 5.2f, 5), chainMat);
        chain->position = {sx, top + 1.8f, -bd * 0.3f};
        chain->rotation.x = 0.55f;
        g->add(chain);
    }
    g->add(stoneBase, deck);
    g->position = {DRAWBRIDGE.x, 0, DRAWBRIDGE.z};
    add(g);
    w.platforms_.push_back({DRAWBRIDGE.x, DRAWBRIDGE.z, bw / 2, top});
}

// ---------- torches with a warm glow ----------
void MedievalWorld::Impl::torch(float x, float y, float z, float facing) {
    static GeoPtr bracketG = geo::cylinder(0.04f, 0.04f, 0.5f, 5);
    static GeoPtr headG = geo::cylinder(0.08f, 0.05f, 0.25f, 6);
    static GeoPtr flameG = geo::cone(0.09f, 0.22f, 6);
    auto g = makeGroup("torch");
    // iron bracket
    auto bracket = makeMesh(bracketG, post);
    bracket->position.y = 0.2f;
    bracket->rotation.z = PI / 6;
    auto head = makeMesh(headG, post);
    head->position = {0.12f, 0.38f, 0};
    auto flame = makeMesh(flameG, torchFire);
    flame->position = {0.12f, 0.55f, 0};
    auto glow = glowSprite(hexc(0xff9f3b), 1.8f, 0.85f);
    glow->position = {0.12f, 0.55f, 0};
    w.torchGlows_.push_back(glow);
    g->add(bracket, head, flame, glow);
    g->position = {x, y, z};
    g->rotation.y = facing;
    add(g);
    auto light = makePointLight(hexc(0xffa244), 0.75f, 8, 2);
    light->position = {x, y + 0.6f, z};
    add(light);
    float phase = rand().f() * 10;
    Object3D* l = light.get();
    w.updaters_.push_back([l, phase](float t, float, glm::vec3) {
        float flicker = 0.85f + 0.15f * std::sin(t * 8 + phase) + 0.08f * std::sin(t * 19 + phase * 2);
        l->intensity = 0.75f * flicker;
    });
    w.torchLights_.push_back(light);
}

// ---------- the Royal Citadel — hollow walls you can walk into, throne hall, stairs up to the ramparts ----------
void MedievalWorld::Impl::castle() {
    float x = CASTLE.x, z = CASTLE.z, cw = CASTLE.w, d = CASTLE.d, h = CASTLE.h;
    float yBase = heightAt(x, z);
    auto cg = makeGroup("castle");
    // wall thickness
    const float T = 1.2f;
    const float gateW = 5.2f, gateH = 6.2f;
    // separate materials for the front (made see-through from inside)
    auto frontMat = stone->clone();
    auto frontSlate = slate->clone();
    float rampartY = yBase + h;
    std::vector<Object3D*> castleFront;

    // walls: [cx, cz, length x, length z, is front]
    float sideLen = d - T, frontSeg = (cw - gateW) / 2;
    struct Wall {
        float cx, cz, lx, lz;
        bool front;
    };
    const Wall walls[5] = {
        {0, -d / 2 + T / 2, cw, T, false},
        {-cw / 2 + T / 2, 0, T, sideLen, false},
        {cw / 2 - T / 2, 0, T, sideLen, false},
        {-(gateW / 2 + frontSeg / 2), d / 2 - T / 2, frontSeg, T, true},
        {(gateW / 2 + frontSeg / 2), d / 2 - T / 2, frontSeg, T, true},
    };
    auto mg = geo::worldUV(geo::box(0.7f, 0.9f, 0.7f));
    for (const Wall& wl : walls) {
        auto m = W(geo::box(wl.lx, h, wl.lz), wl.front ? frontMat : stone);
        m->position = {wl.cx, h / 2, wl.cz};
        m->castShadow = m->receiveShadow = true;
        cg->add(m);
        if (wl.front) castleFront.push_back(m.get());
        w.boxes_.push_back({x + wl.cx, z + wl.cz, wl.lx / 2, wl.lz / 2, 0, rampartY});
        // rampart walkway: small circles along the wall
        float len = std::max(wl.lx, wl.lz);
        bool along = wl.lx > wl.lz;
        for (float k = -len / 2 + 0.6f; k <= len / 2 - 0.6f; k += 1.0f)
            w.platforms_.push_back({x + wl.cx + (along ? k : 0), z + wl.cz + (along ? 0 : k), 0.75f, rampartY});
        // merlons on the outside
        for (float k = -len / 2 + 0.5f; k <= len / 2 - 0.4f; k += 1.6f) {
            auto mm = makeMesh(mg, wl.front ? frontMat : stone);
            float sgx = wl.cx > 0 ? 1.0f : wl.cx < 0 ? -1.0f : 0.0f, sgz = wl.cz > 0 ? 1.0f : wl.cz < 0 ? -1.0f : 0.0f;
            float outX = along ? 0 : sgx * (T / 2 - 0.25f);
            float outZ = along ? sgz * (T / 2 - 0.25f) : 0;
            mm->position = {wl.cx + (along ? k : 0) + outX, h + 0.45f, wl.cz + (along ? 0 : k) + outZ};
            cg->add(mm);
            if (wl.front) castleFront.push_back(mm.get());
        }
    }
    // lintel over the gate + the raised portcullis
    auto lintel = W(geo::box(gateW, h - gateH, T), frontMat);
    lintel->position = {0, gateH + (h - gateH) / 2, d / 2 - T / 2};
    cg->add(lintel);
    castleFront.push_back(lintel.get());
    // walkway over the gate (you can walk underneath)
    w.platforms_.push_back({x, z + d / 2 - T / 2, 0.75f, rampartY, true});
    auto portcullis = makeMesh(geo::plane(gateW - 0.3f, 1.4f), iron);
    portcullis->position = {0, gateH - 0.6f, d / 2 + 0.02f};
    cg->add(portcullis);
    castleFront.push_back(portcullis.get());

    // four corner towers
    const float towerR = 2.6f, towerH = h + 4.5f, towerRoofH = 4.2f;
    const float corners[4][2] = {{-cw / 2, -d / 2}, {cw / 2, -d / 2}, {-cw / 2, d / 2}, {cw / 2, d / 2}};
    for (auto& cr : corners) {
        float tx = cr[0], tz = cr[1];
        bool front = tz > 0;
        auto tm = front ? frontMat : stone;
        auto tow = W(geo::cylinder(towerR, towerR * 1.05f, towerH, 12), tm);
        tow->position = {tx, towerH / 2, tz};
        auto crown = W(geo::cylinder(towerR * 1.15f, towerR, 1.2f, 12), tm);
        crown->position = {tx, towerH + 0.5f, tz};
        auto roofCone = W(geo::cone(towerR * 1.2f, towerRoofH, 12), front ? frontSlate : slate);
        roofCone->position = {tx, towerH + 1.1f + towerRoofH / 2, tz};
        auto pole = makeMesh(geo::cylinder(0.04f, 0.05f, 2.5f, 6), gold);
        pole->position = {tx, towerH + towerRoofH + 1.8f, tz};
        auto flag = makeMesh(geo::plane(1.4f, 0.8f), lam("banner#a82c35#ffd76a"));
        flag->position = {tx + 0.72f, towerH + towerRoofH + 2.2f, tz};
        tow->castShadow = crown->castShadow = roofCone->castShadow = true;
        cg->add(tow, crown, roofCone, pole, flag);
        if (front) castleFront.insert(castleFront.end(), {tow.get(), crown.get(), roofCone.get()});
        w.colliders_.push_back({x + tx, z + tz, towerR});
        w.platforms_.push_back({x + tx, z + tz, towerR, yBase + towerH});
        torch(x + tx + (tx > 0 ? 2.6f : -2.6f), yBase + 4, z + tz, tx > 0 ? -PI / 2 : PI / 2);
    }

    // two gate towers
    for (float sx : {-gateW / 2 - 1.3f, gateW / 2 + 1.3f}) {
        auto gt = W(geo::cylinder(1.5f, 1.7f, gateH + 2.6f, 10), frontMat);
        gt->position = {sx, (gateH + 2.6f) / 2, d / 2 + 0.3f};
        auto gRoof = W(geo::cone(1.8f, 3, 10), frontSlate);
        gRoof->position = {sx, gateH + 2.6f + 1.5f, d / 2 + 0.3f};
        cg->add(gt, gRoof);
        castleFront.insert(castleFront.end(), {gt.get(), gRoof.get()});
        w.colliders_.push_back({x + sx, z + d / 2 + 0.3f, 1.5f});
        torch(x + sx, yBase + 3.2f, z + d / 2 + 2.2f, 0);
    }
    for (float sx : {-3.9f, 3.9f}) {
        auto banner = makeMesh(geo::plane(1.4f, 3.6f), lam("banner#a82c35#ffd76a"));
        banner->position = {sx * 1.55f, gateH * 0.75f, d / 2 + 0.02f};
        cg->add(banner);
        castleFront.push_back(banner.get());
    }

    // ---------- inside the castle ----------
    // polished stone floor
    auto floor = W(geo::box(cw - T * 2, 0.08f, d - T * 2), stoneSmooth);
    floor->position = {0, 0.04f, 0};
    floor->receiveShadow = true;
    cg->add(floor);
    // red carpet from the gate to the throne
    auto carpetM = W(geo::box(2.2f, 0.06f, d - T * 2 - 3), carpet);
    carpetM->position = {0, 0.1f, 0.8f};
    cg->add(carpetM);
    // throne dais (two steps)
    float daisZ = -d / 2 + T + 2.2f;
    auto dais1 = W(geo::box(6, 0.4f, 3.6f), stoneSmooth);
    dais1->position = {0, 0.2f, daisZ};
    auto dais2 = W(geo::box(3.6f, 0.4f, 2.4f), stoneSmooth);
    dais2->position = {0, 0.6f, daisZ - 0.4f};
    auto throneSeat = W(geo::box(1.4f, 0.7f, 1.1f), woodDark);
    throneSeat->position = {0, 1.15f, daisZ - 0.6f};
    auto throneBack = W(geo::box(1.5f, 2.4f, 0.3f), gold);
    throneBack->position = {0, 2.0f, daisZ - 1.15f};
    auto cushion = W(geo::box(1.2f, 0.2f, 0.9f), carpet);
    cushion->position = {0, 1.55f, daisZ - 0.55f};
    cg->add(dais1, dais2, throneSeat, throneBack, cushion);
    w.platforms_.push_back({x, z + daisZ, 2.2f, yBase + 0.4f});
    w.platforms_.push_back({x, z + daisZ - 0.4f, 1.3f, yBase + 0.8f});
    // banners on the wall behind the throne
    for (float sx : {-4.5f, 4.5f, -9.0f, 9.0f}) {
        auto bn = makeMesh(geo::plane(1.5f, 4.2f), lam(std::abs(sx) > 5 ? "banner#2b507a#ffd76a" : "banner#a82c35#ffd76a"));
        bn->position = {sx, 4.6f, -d / 2 + T + 0.03f};
        cg->add(bn);
    }
    // two rows of stone columns
    for (float sx : {-4.2f, 4.2f})
        for (float pz : {-2.5f, 1.5f, 5.5f}) {
            auto col = W(geo::cylinder(0.55f, 0.65f, 5.5f, 8), stoneSmooth);
            col->position = {sx, 2.75f, pz};
            col->castShadow = true;
            auto capital = W(geo::box(1.4f, 0.4f, 1.4f), stone);
            capital->position = {sx, 5.6f, pz};
            cg->add(col, capital);
            w.colliders_.push_back({x + sx, z + pz, 0.6f});
        }
    // long feast tables + benches
    for (float sx : {-8.2f, 8.2f}) {
        auto table = W(geo::box(1.8f, 0.15f, 7), woodDark);
        table->position = {sx, 0.9f, 2};
        auto legs = W(geo::box(1.4f, 0.8f, 6.4f), woodDark);
        legs->position = {sx, 0.45f, 2};
        cg->add(table, legs);
        w.boxes_.push_back({x + sx, z + 2, 0.9f, 3.5f, 0, yBase + 0.98f});
        w.platforms_.push_back({x + sx, z + 2 - 2, 0.8f, yBase + 0.98f});
        w.platforms_.push_back({x + sx, z + 2 + 2, 0.8f, yBase + 0.98f});
        for (float bx : {-1.4f, 1.4f}) {
            auto bench = W(geo::box(0.5f, 0.45f, 6.6f), woodDark);
            bench->position = {sx + bx, 0.23f, 2};
            cg->add(bench);
        }
        // fruit plates
        const uint32_t fruitCols[3] = {0xd8263b, 0xffb347, 0x9be35a};
        for (float k = -2.5f; k <= 2.5f; k += 1.25f) {
            int ci = (int)std::fmod(k * 4 + 10, 3.0f);
            auto fruit = makeMesh(geo::sphere(0.14f, 6, 4), basic(hexc(fruitCols[ci])));
            fruit->position = {sx, 1.05f, 2 + k};
            cg->add(fruit);
        }
    }
    // stone stairs up to the ramparts (along the inner right wall)
    const int stairSteps = 22;
    // ~0.43 m each, walkable without jumping
    float stepRise = h / stairSteps;
    for (int s = 0; s < stairSteps; ++s) {
        float stepY = (s + 1) * stepRise;
        float sz = d / 2 - T - 0.8f - s * 0.55f;
        auto step = W(geo::box(2.0f, stepY, 0.56f), stoneSmooth);
        step->position = {cw / 2 - T - 1.0f, stepY / 2, sz};
        step->castShadow = step->receiveShadow = true;
        cg->add(step);
        w.platforms_.push_back({x + cw / 2 - T - 1.0f, z + sz, 0.75f, yBase + stepY});
    }
    // torches in the throne hall
    const float hallTorches[3][2] = {{-cw / 2 + T + 0.2f, -3}, {cw / 2 - T - 0.2f, -3}, {-cw / 2 + T + 0.2f, 4}};
    for (auto& ht : hallTorches) torch(x + ht[0], yBase + 3, z + ht[1], ht[0] > 0 ? -PI / 2 : PI / 2);
    auto hallLight = makePointLight(hexc(0xffb070), 1.4f, 22, 1.6f);
    hallLight->position = {0, 4, daisZ + 3};
    cg->add(hallLight);

    cg->position = {x, yBase, z};
    add(cg);

    // inside the castle -> the front wall fades so it does not block the camera
    frontMat->transparent = frontSlate->transparent = true;
    auto fade = std::make_shared<float>(1.0f);
    MatPtr ironM = iron;
    w.updaters_.push_back([=](float, float dt, glm::vec3 focus) {
        bool inside = std::abs(focus.x - x) < cw / 2 && focus.z < z + d / 2 + 1 && focus.z > z - d / 2;
        *fade += ((inside ? 0.0f : 1.0f) - *fade) * std::min(1.0f, dt * 5);
        for (auto& m : {frontMat, frontSlate}) {
            m->opacity = *fade;
            m->depthWrite = *fade > 0.95f;
        }
        ironM->opacity = *fade;
        bool show = *fade > 0.04f;
        for (Object3D* o : castleFront) o->visible = show;
    });
}

// ---------- swan lake + jetty ----------
void MedievalWorld::Impl::lake() {
    auto cg = geo::circle(LAKE.r * 1.32f, 40);
    cg->rotateX(-PI / 2).translate(LAKE.x, WATER_Y, LAKE.z);
    add(makeMesh(geo::worldUV(cg, 0.5f), water));
    // wooden jetty from the east shore
    float pierStart = LAKE.x + LAKE.r + 1.2f, pierEnd = BOAT_SPOT.x + 1.3f, len = pierStart - pierEnd;
    auto deck = W(geo::box(len, 0.18f, 1.8f), woodDark);
    deck->position = {(pierStart + pierEnd) / 2, 0.22f, LAKE.z};
    deck->castShadow = deck->receiveShadow = true;
    add(deck);
    for (float px = pierEnd + 0.3f; px <= pierStart; px += 1.6f)
        for (float pz : {-0.8f, 0.8f}) {
            auto p = W(geo::cylinder(0.1f, 0.1f, 1.9f, 6), woodDark);
            p->position = {px, -0.5f, LAKE.z + pz};
            add(p);
        }
    for (float px = pierEnd + 0.4f; px <= pierStart; px += 0.9f) w.platforms_.push_back({px, LAKE.z, 0.85f, 0.31f, true});
    torch(pierEnd + 0.4f, 1.3f, LAKE.z + 1.0f, 0);

    // white swans circling
    auto swanMat = lambertColor(hexc(0xfafaf5));
    auto beakMat = basic(hexc(0xff9a2e));
    struct Swan {
        Object3D* g;
        float a, rr, sp;
    };
    std::vector<Swan> swans;
    for (int i = 0; i < 3; ++i) {
        auto g = makeGroup("swan");
        auto body = makeMesh(geo::sphere(0.32f, 8, 6), swanMat);
        body->scale = {0.8f, 0.6f, 1.25f};
        auto neck = makeMesh(geo::cylinder(0.05f, 0.07f, 0.6f, 6), swanMat);
        neck->position = {0, 0.32f, 0.28f};
        neck->rotation.x = -0.25f;
        auto head = makeMesh(geo::sphere(0.09f, 6, 5), swanMat);
        head->position = {0, 0.62f, 0.36f};
        auto beak = makeMesh(geo::cone(0.04f, 0.14f, 5), beakMat);
        beak->rotation.x = PI / 2;
        beak->position = {0, 0.6f, 0.48f};
        g->add(body, neck, head, beak);
        g->traverse([](Object3D& o) {
            if (o.kind == ObjectKind::Mesh) o.castShadow = true;
        });
        add(g);
        swans.push_back({g.get(), i * 2.1f, 4 + i * 1.6f, 0.12f + i * 0.03f});
    }
    w.updaters_.push_back([swans](float t, float, glm::vec3) {
        for (auto& sw : swans) {
            float a = sw.a + t * sw.sp;
            sw.g->position = {LAKE.x - 1.5f + std::cos(a) * sw.rr, WATER_Y + 0.05f + std::sin(t * 2 + sw.a) * 0.03f, LAKE.z + std::sin(a) * sw.rr};
            sw.g->rotation.y = -a;
        }
    });
}

// ---------- the royal fountain in the square ----------
void MedievalWorld::Impl::fountain() {
    float x = FOUNTAIN.x, z = FOUNTAIN.y, r = FOUNTAIN.z, y = heightAt(x, z);
    auto f = makeGroup("fountain");
    // octagonal basin wall
    auto poolWall = W(geo::cylinder(r, r + 0.4f, 0.75f, 8), stoneSmooth);
    poolWall->position.y = 0.38f;
    // water in the basin
    auto wg = geo::circle(r - 0.2f, 16);
    wg->rotateX(-PI / 2);
    auto waterMesh = makeMesh(wg, water);
    waterMesh->position.y = 0.58f;
    // middle column
    auto pillar1 = W(geo::cylinder(0.5f, 0.65f, 1.6f, 8), stoneSmooth);
    pillar1->position.y = 1.0f;
    // second basin
    auto tier2 = W(geo::cylinder(1.5f, 1.2f, 0.35f, 8), stoneSmooth);
    tier2->position.y = 1.8f;
    // top column and golden orb
    auto pillar2 = W(geo::cylinder(0.3f, 0.4f, 1.0f, 8), stoneSmooth);
    pillar2->position.y = 2.4f;
    auto orb = makeMesh(geo::sphere(0.35f, 12, 8), gold);
    orb->position.y = 3.0f;
    // water spray
    auto spray = glowSprite(hexc(0x9fd6ff), 2.2f, 0.75f);
    spray->position.y = 3.2f;
    f->add(poolWall, waterMesh, pillar1, tier2, pillar2, orb, spray);
    f->position = {x, y, z};
    add(f);
    w.colliders_.push_back({x, z, 1.0f});
    w.platforms_.push_back({x, z, r, y + 0.75f});
    Object3D* sp = spray.get();
    w.updaters_.push_back([sp](float t, float, glm::vec3) { sp->scale = glm::vec3(2.0f + std::sin(t * 5) * 0.25f); });
}

// ---------- market stalls ----------
ObjPtr MedievalWorld::Impl::stall(float x, float z, float angle, uint32_t c1, uint32_t c2, const std::string& cwName) {
    float y = heightAt(x, z);
    auto g = makeGroup("stall");
    const float sw = 3.4f, sd = 2.4f, sh = 2.4f;
    // four wooden posts
    static GeoPtr postGeo = geo::cylinder(0.08f, 0.08f, sh, 6);
    for (float px : {-sw / 2 + 0.2f, sw / 2 - 0.2f})
        for (float pz : {-sd / 2 + 0.2f, sd / 2 - 0.2f}) {
            auto p = makeMesh(postGeo, woodDark);
            p->position = {px, sh / 2, pz};
            g->add(p);
        }
    // counter
    auto counter = W(geo::box(sw - 0.2f, 0.9f, sd * 0.55f), wood);
    counter->position = {0, 0.45f, 0.2f};
    // striped awning
    char key[64];
    std::snprintf(key, sizeof key, "awning#%06x#%06x", c1, c2);
    auto ag = geo::plane(sw + 0.4f, sd + 0.6f);
    ag->rotateX(-PI / 2 + 0.25f);
    auto awning = makeMesh(ag, lam(key));
    awning->position = {0, sh + 0.1f, 0};
    // crate and barrel
    auto crateM = makeMesh(geo::box(0.8f, 0.8f, 0.8f), crate);
    crateM->position = {-sw / 2 - 0.5f, 0.4f, 0};
    auto barrelM = makeMesh(geo::cylinder(0.4f, 0.45f, 0.9f, 8), barrel);
    barrelM->position = {sw / 2 + 0.5f, 0.45f, 0};
    CwTag ct;
    ct.name = "crate";
    ct.hasFit = true;
    ct.fit = {0.8f, 0.8f, 0.8f};
    ct.center = true;
    crateM->cw.push_back(ct);
    CwTag bt;
    bt.name = "barrel";
    bt.hasFit = true;
    bt.fit = {0.95f, 0.9f, 0.95f};
    bt.center = true;
    barrelM->cw.push_back(bt);
    g->add(counter, awning, crateM, barrelM);
    // Cube World models face -Z
    CwTag st;
    st.name = cwName;
    st.w = sw + 0.3f;
    st.rotY = PI;
    g->cw.push_back(st);
    g->position = {x, y, z};
    g->rotation.y = angle;
    add(g);
    w.boxes_.push_back({x, z, sw / 2 + 0.4f, sd / 2 + 0.4f, angle, y + 1.0f});
    torch(x, y + sh - 0.2f, z + sd / 2 + 0.2f, angle);
    return g;
}

void MedievalWorld::Impl::market() {
    // fruit / berries
    stall(-8, 3, 0.15f, 0xa82c35, 0xf6eedf, "market-stand1");
    // magic potions
    stall(8, 3, -0.15f, 0x5a3d7a, 0xffd76a, "market-stand2");
    // royal silks
    stall(-10, 16, 0.35f, 0x2b507a, 0xf6eedf, "market-stand3");
    // blacksmith forge
    stall(10, 16, -0.35f, 0x3a3a42, 0x8a4a35, "market-stand1");
    auto anvil = makeMesh(geo::box(0.5f, 0.6f, 0.9f), post);
    anvil->position = {10, heightAt(10, 16) + 0.3f, 14.5f};
    CwTag at;
    at.name = "anvil";
    at.hasFit = true;
    at.fit = {0.5f, 0.6f, 0.9f};
    at.center = true;
    anvil->cw.push_back(at);
    auto furnace = W(geo::box(1.4f, 1.8f, 1.2f), stoneSmooth);
    furnace->position = {12.5f, heightAt(12.5f, 17) + 0.9f, 17};
    CwTag ft;
    ft.name = "furnace";
    ft.hasFit = true;
    ft.fit = {1.4f, 1.8f, 1.2f};
    ft.center = true;
    furnace->cw.push_back(ft);
    auto fire = glowSprite(hexc(0xff5511), 1.6f, 0.9f);
    fire->position = {12.5f, heightAt(12.5f, 17) + 0.6f, 16.5f};
    add(anvil);
    add(furnace);
    add(fire);
}

// ---------- The Gilded Griffin Tavern ----------
void MedievalWorld::Impl::tavern() {
    float x = TAVERN.x, z = TAVERN.z, tw = TAVERN.w, d = TAVERN.d, h = TAVERN.h, y = heightAt(x, z);
    auto tg = makeGroup("tavern");
    // stone ground floor
    auto floor1 = W(geo::box(tw, h * 0.45f, d), stoneSmooth);
    floor1->position.y = (h * 0.45f) / 2;
    // half-timbered upper floor, jutting out a little
    auto floor2 = W(geo::box(tw + 0.4f, h * 0.55f, d + 0.4f), plaster);
    floor2->position.y = h * 0.45f + (h * 0.55f) / 2;
    // slate gable roof
    const float roofH = 2.6f;
    Shape rs;
    rs.moveTo(-tw / 2 - 0.6f, 0).lineTo(0, roofH).lineTo(tw / 2 + 0.6f, 0).lineTo(-tw / 2 - 0.6f, 0);
    ExtrudeOptions eo;
    eo.depth = d + 1.0f;
    eo.bevelEnabled = false;
    auto roof = W(geo::extrude(rs, eo), slate);
    roof->position = {0, h, -d / 2 - 0.5f};
    // stone chimney
    auto chim = W(geo::box(0.9f, 3.2f, 0.9f), stoneSmooth);
    chim->position = {tw / 2 - 0.8f, h + 1.2f, -d * 0.2f};
    auto doorM = makeMesh(geo::box(1.0f, 1.8f, 0.1f), door);
    doorM->position = {0, 0.9f, d / 2 + 0.05f};
    // warm windows
    for (float sx : {-1.8f, 1.8f}) {
        auto win = makeMesh(geo::plane(0.8f, 0.8f), window);
        win->position = {sx, h * 0.7f, d / 2 + 0.22f};
        tg->add(win);
    }
    // patio table and benches
    auto table = W(geo::box(2.0f, 0.75f, 1.0f), wood);
    table->position = {0, 0.38f, d / 2 + 2.2f};
    auto bench1 = W(geo::box(2.0f, 0.45f, 0.4f), woodDark);
    bench1->position = {0, 0.23f, d / 2 + 1.5f};
    auto bench2 = W(geo::box(2.0f, 0.45f, 0.4f), woodDark);
    bench2->position = {0, 0.23f, d / 2 + 2.9f};
    tg->add(floor1, floor2, roof, chim, doorM, table, bench1, bench2);
    tg->position = {x, y, z};
    tg->rotation.y = PI / 8;
    add(tg);
    w.boxes_.push_back({x, z, tw / 2 + 0.3f, d / 2 + 0.3f, PI / 8, y + h + roofH});
    torch(x + 1.4f, y + 2.2f, z + d / 2 + 0.4f, PI / 8);
}

// ---------- knight's training grounds ----------
void MedievalWorld::Impl::archery() {
    float x = ARCHERY.x, z = ARCHERY.y, y = heightAt(x, z);
    // three archery targets
    static GeoPtr legGeo = geo::cylinder(0.04f, 0.05f, 1.8f, 5);
    for (int i = -1; i <= 1; ++i) {
        float tx = x + 3.5f, tz = z + i * 2.8f, ty = heightAt(tx, tz);
        auto tgp = makeGroup("target");
        auto leg1 = makeMesh(legGeo, woodDark);
        leg1->position = {-0.35f, 0.85f, 0};
        leg1->rotation.z = 0.25f;
        auto leg2 = makeMesh(legGeo, woodDark);
        leg2->position = {0.35f, 0.85f, 0};
        leg2->rotation.z = -0.25f;
        auto leg3 = makeMesh(legGeo, woodDark);
        leg3->position = {0, 0.85f, -0.4f};
        leg3->rotation.x = -0.25f;
        auto dg = geo::cylinder(0.65f, 0.65f, 0.08f, 16);
        dg->rotateX(PI / 2);
        auto disc = makeMesh(dg, lam("target"));
        disc->position = {0, 1.25f, 0.05f};
        tgp->add(leg1, leg2, leg3, disc);
        tgp->position = {tx, ty, tz};
        tgp->rotation.y = -PI / 2;
        add(tgp);
        w.colliders_.push_back({tx, tz, 0.65f});
    }
    // training dummy
    auto dummy = makeGroup("dummy");
    auto dPole = makeMesh(geo::cylinder(0.08f, 0.1f, 1.8f, 6), woodDark);
    dPole->position.y = 0.9f;
    auto dBody = W(geo::cylinder(0.35f, 0.3f, 0.8f, 8), lam("planks", 0xc4a478));
    dBody->position.y = 1.1f;
    auto dHead = makeMesh(geo::sphere(0.25f, 8, 8), slate);
    dHead->position.y = 1.65f;
    auto dShield = makeMesh(geo::plane(0.6f, 0.8f), lam("banner#2b507a#ffd76a"));
    dShield->position = {0.35f, 1.1f, 0};
    dShield->rotation.y = PI / 2;
    dummy->add(dPole, dBody, dHead, dShield);
    dummy->position = {x - 2, y, z - 1};
    add(dummy);
    w.colliders_.push_back({x - 2, z - 1, 0.5f});
}

// ---------- the royal rose garden ----------
void MedievalWorld::Impl::garden() {
    float x = GARDEN.x, z = GARDEN.z, gw = GARDEN.w, d = GARDEN.d, y = heightAt(x, z);
    auto g = makeGroup("garden");
    // boxwood hedges all around
    auto hedgeMat = lam("needles", 0x3d7a42);
    auto h1 = W(geo::box(gw, 0.9f, 0.8f), hedgeMat);
    h1->position = {0, 0.45f, -d / 2};
    auto h2 = W(geo::box(gw, 0.9f, 0.8f), hedgeMat);
    h2->position = {0, 0.45f, d / 2};
    auto h3 = W(geo::box(0.8f, 0.9f, d), hedgeMat);
    h3->position = {-gw / 2, 0.45f, 0};
    auto h4 = W(geo::box(0.8f, 0.9f, d), hedgeMat);
    h4->position = {gw / 2, 0.45f, 0};
    g->add(h1, h2, h3, h4);
    // rose beds
    const uint32_t roseColors[4] = {0xd62246, 0xff5c8a, 0xff8fa3, 0xffffff};
    auto roseGeo = geo::sphere(0.12f, 6, 4);
    auto bushGeo = geo::worldUV(geo::sphere(0.55f, 8, 6));
    for (float rx = -gw / 2 + 2; rx <= gw / 2 - 2; rx += 2.2f)
        for (float rz = -d / 2 + 2; rz <= d / 2 - 2; rz += 2.2f) {
            if (std::hypot(rx, rz) < 2) continue;
            auto bush = makeMesh(bushGeo, hedgeMat);
            bush->position = {rx, 0.45f, rz};
            g->add(bush);
            for (int fl = 0; fl < 4; ++fl) {
                int ci = (int)(fl + rand()() * 4);
                auto rose = makeMesh(roseGeo, basic(hexc(roseColors[std::min(ci, 3)])));
                float px = rx + (rand().f() - 0.5f) * 0.6f;
                float py = 0.7f + rand().f() * 0.3f;
                float pz = rz + (rand().f() - 0.5f) * 0.6f;
                rose->position = {px, py, pz};
                g->add(rose);
            }
        }
    // stone bench
    auto bench = W(geo::box(1.8f, 0.45f, 0.55f), stoneSmooth);
    bench->position = {0, 0.23f, 0};
    g->add(bench);
    w.platforms_.push_back({x, z, 0.9f, y + 0.45f});
    g->position = {x, y, z};
    add(g);
    w.boxes_.push_back({x, z, gw / 2, d / 2, 0, y + 0.9f});
}

// ---------- the ancient bell tower ----------
void MedievalWorld::Impl::bellTower() {
    float x = BELL_TOWER.x, z = BELL_TOWER.z, s = BELL_TOWER.s, h = BELL_TOWER.h, y = heightAt(x, z);
    auto g = makeGroup("belltower");
    auto body = W(geo::box(s, h - 3, s), stone);
    body->position.y = (h - 3) / 2;
    // open belfry
    auto belfry = W(geo::box(s * 0.9f, 2.8f, s * 0.9f), stoneSmooth);
    belfry->position.y = h - 1.4f;
    // golden bell
    auto bell = makeMesh(geo::cone(0.75f, 1.2f, 10), gold);
    bell->position.y = h - 1.2f;
    // slate pyramid spire
    auto sg = geo::cone(s * 0.75f, 4.5f, 4);
    auto spire = W(sg, slate);
    spire->geometry->rotateY(PI / 4);
    spire->position.y = h + 2.25f;
    g->add(body, belfry, bell, spire);
    g->position = {x, y, z};
    add(g);
    w.boxes_.push_back({x, z, s / 2 + 0.2f, s / 2 + 0.2f, 0, y + h + 4.5f});
}

// ---------- Ancient Portal back to the snow realm ----------
void MedievalWorld::Impl::portal() {
    float x = PORTAL.x, z = PORTAL.z, y = heightAt(x, z);
    auto g = makeGroup("portal");
    auto pillarGeo = geo::worldUV(geo::box(1.2f, 5.4f, 1.2f));
    auto p1 = makeMesh(pillarGeo, stone);
    p1->position = {-2.4f, 2.7f, 0};
    auto p2 = makeMesh(pillarGeo, stone);
    p2->position = {2.4f, 2.7f, 0};
    auto lintel = W(geo::box(6.4f, 1.2f, 1.6f), stone);
    lintel->position = {0, 5.2f, 0};
    // mystic rune swirl inside the gate
    auto vm = basic(hexc(0x7fe8ff), tex::get("ice"));
    vm->transparent = true;
    vm->opacity = 0.75f;
    vm->side = Side::Double;
    auto vortex = makeMesh(geo::plane(3.8f, 4.5f), vm);
    vortex->position = {0, 2.5f, 0};
    auto glow = glowSprite(hexc(0x7fe8ff), 6.5f, 0.7f);
    glow->position = {0, 2.5f, 0};
    g->add(p1, p2, lintel, vortex, glow);
    g->position = {x, y, z};
    add(g);
    w.colliders_.push_back({x - 2.4f, z, 0.8f});
    w.colliders_.push_back({x + 2.4f, z, 0.8f});
    Object3D* gl = glow.get();
    Object3D* vo = vortex.get();
    w.updaters_.push_back([gl, vo](float t, float, glm::vec3) {
        gl->material()->opacity = 0.6f + std::sin(t * 3) * 0.25f;
        vo->rotation.z = std::sin(t * 1.2f) * 0.05f;
    });
}

// ---------- leafy oak trees ----------
void MedievalWorld::Impl::trees() {
    auto trunkGeo = geo::worldUV(geo::cylinder(0.35f, 0.5f, 2.2f, 8), 1.2f);
    trunkGeo->translate(0, 1.1f, 0);
    auto crownMat = lam("needles", 0x3f7a42);
    auto trunkMat = lam("bark");
    std::vector<glm::vec3> pos;
    const glm::vec3 avoid[] = {
        {CASTLE.x, CASTLE.z, 22},
        // rooftop / crate parkour
        {36, -17, 11},
        // swan lake
        {LAKE.x, LAKE.z, LAKE.r + 5},
        {FOUNTAIN.x, FOUNTAIN.y, 10},
        {TAVERN.x, TAVERN.z, 9},
        {ARCHERY.x, ARCHERY.y, 11},
        {GARDEN.x, GARDEN.z, 11},
        {BELL_TOWER.x, BELL_TOWER.z, 7},
        {PORTAL.x, PORTAL.z, 6},
    };
    for (int tries = 0; pos.size() < 90 && tries++ < 3000;) {
        float angle = rand().f() * PI * 2;
        float dist = 14 + std::sqrt(rand().f()) * (WORLD_R - 16);
        float tx = std::cos(angle) * dist, tz = std::sin(angle) * dist;
        if (std::hypot(tx, tz) > WORLD_R - 6) continue;
        if (distToPathsM(tx, tz) < 3.2f) continue;
        bool bad = false;
        for (auto& a : avoid)
            if (std::hypot(tx - a.x, tz - a.y) < a.z) bad = true;
        for (auto& o : pos)
            if (std::hypot(tx - o.x, tz - o.y) < 4.2f) bad = true;
        if (bad) continue;
        pos.push_back({tx, tz, 0.85f + rand().f() * 0.4f});
    }
    for (auto& p : pos) {
        float tx = p.x, tz = p.y, sc = p.z, ty = layout::footHeight(heightAt, tx, tz, 0.9f * sc);
        auto tree = makeGroup("oak");
        auto trunk = makeMesh(trunkGeo, trunkMat);
        // three cartoon crowns
        auto c1 = W(geo::dodecahedron(1.9f * sc, 1), crownMat);
        c1->position.y = 2.4f * sc;
        auto c2 = W(geo::dodecahedron(1.5f * sc, 1), crownMat);
        c2->position = {0.4f * sc, 3.4f * sc, 0.3f * sc};
        auto c3 = W(geo::dodecahedron(1.2f * sc, 1), crownMat);
        c3->position = {-0.3f * sc, 4.0f * sc, -0.2f * sc};
        tree->add(trunk, c1, c2, c3);
        // Cube World model: trunk + three leaf blobs
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
        tree->rotation.y = tx * 7.3f;
        tree->position = {tx, ty, tz};
        tree->scale = glm::vec3(sc);
        add(tree);
        w.colliders_.push_back({tx, tz, 0.5f * sc, true});
        w.treeShapes_.push_back({tx, tz, ty, 4.5f * sc, 1.8f * sc});
        TreeRef ref;
        ref.x = tx;
        ref.z = tz;
        ref.collider = (int)w.colliders_.size() - 1;
        ref.shape = (int)w.treeShapes_.size() - 1;
        Object3D* tp = tree.get();
        ref.hide = [tp] { tp->visible = false; };
        w.trees_.push_back(ref);
    }
}

// ---------- outer kingdom walls ----------
void MedievalWorld::Impl::outerWalls() {
    const float wallR = 64;
    const int SEG = 24;
    for (int i = 0; i < SEG; ++i) {
        float a0 = (float)i / SEG * PI * 2, a1 = (float)(i + 1) / SEG * PI * 2;
        float x0 = std::cos(a0) * wallR, z0 = std::sin(a0) * wallR, x1 = std::cos(a1) * wallR, z1 = std::sin(a1) * wallR;
        // leave a gap where the old road comes in from the snow village
        if (layout::corridor((x0 + x1) / 2, (z0 + z1) / 2) > 0.3f) continue;
        float len = std::hypot(x1 - x0, z1 - z0);
        auto wall = W(geo::box(len, 4.2f, 1.2f), stone);
        float mx = (x0 + x1) / 2, mz = (z0 + z1) / 2;
        wall->position = {mx, heightAt(mx, mz) + 2.1f, mz};
        wall->rotation.y = std::atan2(x1 - x0, z1 - z0) + PI / 2;
        add(wall);
        w.boxes_.push_back({mx, mz, len / 2, 0.8f, wall->rotation.y, heightAt(mx, mz) + 4.2f});
    }
}

// ---------------------------------------------------------------------------

MedievalWorld::MedievalWorld() {
    name = "medieval";
    spawn = SPAWN;
    group->name = "medieval-world";
    build();
    berrySpots = {{-6, 7}, {6, 7}, {-4, 12}, {4, 12}, {-12, 10}, {12, 10}, {-8, 22}, {8, 22}, {-14, 4}, {14, 4}, {-2, 34}, {2, 34},
                  {-20, 12}, {-22, -2}, {18, 12}, {20, -2},
                  // in the rose garden
                  {18, 24}, {22, 24}, {18, 30}, {22, 30},
                  // around the bridge and the castle
                  {-4, -14}, {4, -14}, {-8, -26}, {8, -26}, {0, 44}};
    mapData.type = "medieval";
    mapData.radius = WORLD_R;
    mapData.paths = PATHS;
    mapData.trees = &treeShapes_;
    mapData.castle = {CASTLE.x, CASTLE.z, CASTLE.w, CASTLE.d};
    mapData.drawbridge = {DRAWBRIDGE.x, DRAWBRIDGE.z, DRAWBRIDGE.w, DRAWBRIDGE.d};
    mapData.fountain = FOUNTAIN;
    mapData.tavern = {TAVERN.x, TAVERN.z, TAVERN.w, TAVERN.d};
    mapData.archery = ARCHERY;
    mapData.garden = {GARDEN.x, GARDEN.z, GARDEN.w, GARDEN.d};
    mapData.bellTower = {BELL_TOWER.x, BELL_TOWER.z, BELL_TOWER.s};
    mapData.portal = {PORTAL.x, PORTAL.z};
    mapData.lake = {LAKE.x, LAKE.z, LAKE.r};

    cur_ = goal_ = preset("day");
    timeOfDay = "day";
    applyTOD(1);
    updaters_.push_back([this](float, float dt, glm::vec3) { applyTOD(std::min(1.0f, dt * 2)); });
}

void MedievalWorld::build() {
    Impl I(*this);
    I.sky();
    // the ground is blocks now (world/terrain.cpp)
    I.moatAndBridge();
    I.castle();
    I.lake();
    I.fountain();
    I.market();
    I.tavern();
    I.archery();
    I.garden();
    I.bellTower();
    I.portal();
    I.trees();
    I.outerWalls();
}

// ---------- physics ----------
float MedievalWorld::groundAt(float x, float z, float y) const {
    // in the water you swim (half submerged, not walking on the bottom)
    float g = std::max(heightAt(x, z), WATER_Y - 0.55f);
    for (auto& p : platforms_)
        if (y >= p.top - 0.5f && std::hypot(x - p.x, z - p.z) < p.r + 0.2f) g = std::max(g, p.top);
    return extraGround(x, z, y, g);
}

void MedievalWorld::resolveCollisions(glm::vec3& pos, float radius) const {
    for (auto& c : colliders_) {
        if (c.removed || (skipTrees_ && c.tree)) continue;
        float dx = pos.x - c.x, dz = pos.z - c.z, d = std::hypot(dx, dz), m = c.r + radius;
        if (d < m && d > 1e-4f) {
            pos.x = c.x + dx / d * m;
            pos.z = c.z + dz / d * m;
        }
    }
    for (auto& p : platforms_) {
        // thin = you can stand on it but walk underneath
        if (p.thin || pos.y >= p.top - 0.45f) continue;
        float dx = pos.x - p.x, dz = pos.z - p.z, d = std::hypot(dx, dz), m = p.r + radius;
        if (d < m && d > 1e-4f) {
            pos.x = p.x + dx / d * m;
            pos.z = p.z + dz / d * m;
        }
    }
    for (auto& b : boxes_) {
        // you can stand on walls / tables
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
    float r = std::hypot(pos.x, pos.z);
    // the gate side opens onto the valley
    if (r > WORLD_R && layout::corridor(pos.x, pos.z) < 0.5f) {
        pos.x *= WORLD_R / r;
        pos.z *= WORLD_R / r;
    }
}

bool MedievalWorld::isFree(float x, float z, float r, bool ignoreTrees) const {
    if (distToPathsM(x, z) < r + 1.4f || heightAt(x, z) < WATER_Y + 0.2f) return false;
    glm::vec3 p(x, heightAt(x, z), z);
    skipTrees_ = ignoreTrees;
    resolveCollisions(p, r);
    skipTrees_ = false;
    return p.x == x && p.z == z;
}

Surface MedievalWorld::surfaceAt(float x, float z) const {
    // the raised castle base
    float qx = std::abs(x - CASTLE.x) - CASTLE.w / 2, qz = std::abs(z - CASTLE.z) - CASTLE.d / 2;
    if (qx < 0 && qz < 0) return {Ground::Stone};
    // paved town square and the cobbled roads
    if (std::hypot(x, z - 8) < 14.5f || distToPathsM(x, z) < 1.8f) return {Ground::Cobble};
    // sandy training ground
    if (std::hypot(x - ARCHERY.x, z - ARCHERY.y) < ARCHERY.z) return {Ground::Dirt, hexc(0xd8b88a)};
    return {Ground::Grass};
}

std::string MedievalWorld::zoneAt(float x, float z) const {
    if (std::hypot(x - PORTAL.x, z - PORTAL.z) < 6) return "portal";
    if (std::hypot(x - LAKE.x, z - LAKE.z) < LAKE.r + 4) return "swanlake";
    if (std::hypot(x - FOUNTAIN.x, z - FOUNTAIN.y) < 6) return "market";
    if (z < -21 && z > -43 && std::abs(x) < 14) return "citadel";
    if (std::hypot(x - DRAWBRIDGE.x, z - DRAWBRIDGE.z) < 8) return "drawbridge";
    if (std::hypot(x - TAVERN.x, z - TAVERN.z) < 8) return "tavern";
    if (std::hypot(x - ARCHERY.x, z - ARCHERY.y) < 9) return "archery";
    if (std::hypot(x - GARDEN.x, z - GARDEN.z) < 9) return "garden";
    if (std::hypot(x - BELL_TOWER.x, z - BELL_TOWER.z) < 6) return "belltower";
    if (std::hypot(x, z - 8) < 18) return "market";
    return "meadow";
}

// ---------- time of day ----------
MedievalWorld::Tod MedievalWorld::preset(const std::string& n) {
    struct P {
        uint32_t top, mid, hor, fog, hemiS, hemiG;
        float hemiI;
        uint32_t sun;
        float sunI;
        glm::vec3 dir;
        float sunVis, torches;
        uint32_t win;
    };
    P p;
    if (n == "dusk") p = {0x251c4a, 0x874068, 0xf59b58, 0x804c60, 0xe8b898, 0x3a2838, 0.5f, 0xffa060, 0.75f, {0.9f, 0.35f, -0.3f}, 0.7f, 0.85f, 0xffc27a};
    else if (n == "night") p = {0x09071c, 0x1f173d, 0x4c356e, 0x221a38, 0x5a68aa, 0x1a1628, 0.35f, 0x9fb4ff, 0.45f, {-0.4f, 0.9f, 0.3f}, 0.9f, 1.0f, 0xffd98a};
    else p = {0x3d70ba, 0x82b2ea, 0xd6e5f8, 0xc2d8f0, 0xfff0dc, 0x4a4a3a, 0.65f, 0xfffae0, 0.95f, {0.5f, 1, 0.4f}, 1, 0.25f, 0x807060};
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
    t.sunVis = p.sunVis;
    t.torches = p.torches;
    return t;
}

void MedievalWorld::setTimeOfDay(const std::string& n) {
    if (n != "day" && n != "dusk" && n != "night") return;
    timeOfDay = n;
    goal_ = preset(n);
}

void MedievalWorld::applyTOD(float k) {
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
    L(cur_.sunVis, goal_.sunVis);
    L(cur_.torches, goal_.torches);
    env.fogColor = cur_.fog;
    env.hemiSky = cur_.hemiS;
    env.hemiGround = cur_.hemiG;
    env.hemiIntensity = cur_.hemiI;
    env.sunColor = cur_.sun;
    env.sunIntensity = cur_.sunI;
    sunMesh_->visible = cur_.sunVis > 0.1f;
    sunMesh_->material()->opacity = cur_.sunVis;
    sunHalo_->material()->opacity = 0.55f * cur_.sunVis;
    for (auto& g : torchGlows_) g->material()->opacity = cur_.torches;
    mWindow_->color = cur_.win;
}

void MedievalWorld::update(float t, float dt, glm::vec3 focus) {
    for (auto& u : updaters_) u(t, dt, focus);
    env.sunPosition = focus + glm::normalize(cur_.dir) * 35.0f;
    env.sunTarget = focus;
}
