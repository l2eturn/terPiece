// Port of berries.js
#include "game/berries.h"
#include <algorithm>
#include "art/glow.h"
#include "art/textures.h"
#include "core/math.h"
#include "world/world.h"

namespace {
GeoPtr berryGeo() {
    static GeoPtr g = geo::sphere(0.075f, 6, 4);
    return g;
}
MatPtr berryMat() {
    static MatPtr m = basic(hexc(0xd8263b));
    return m;
}
MatPtr shineMat() {
    static MatPtr m = basic(hexc(0xffd0d8));
    return m;
}
}  // namespace

BerryBushes::BerryBushes(Object3D& scene, const World& world, const std::vector<glm::vec2>& spots) {
    auto bushGeo = geo::worldUV(geo::icosahedron(0.5f), 2);
    auto bushMat = lambert(tex::get("needles"), hexc(0x9ad0a8));
    Rng rand(77);
    object = makeGroup("berries");
    scene.add(object);
    auto shineGeo = geo::sphere(0.025f, 4, 3);
    for (auto s : spots) {
        float x = s.x, z = s.y, y = world.groundAt(x, z, 99);
        auto g = makeGroup();
        auto bush = makeMesh(bushGeo, bushMat);
        bush->scale = {1, 0.75f, 1};
        bush->position.y = 0.3f;
        bush->rotation.y = rand.f() * 6;
        bush->castShadow = true;
        g->add(bush);
        auto berries = makeGroup();
        for (int i = 0; i < 4; ++i) {
            float a = (i / 4.0f) * PI * 2 + rand.f();
            auto b = makeMesh(berryGeo(), berryMat());
            b->position = {std::cos(a) * 0.4f, 0.3f + rand.f() * 0.25f, std::sin(a) * 0.4f};
            auto sh = makeMesh(shineGeo, shineMat());
            sh->position = {0.03f, 0.04f, 0.05f};
            b->add(sh);
            berries->add(b);
        }
        g->add(berries);
        g->position = {x, y, z};
        object->add(g);
        bushes_.push_back({x, z, y, berries});
    }
}

int BerryBushes::update(float t, glm::vec3 p) {
    int picked = 0;
    for (auto& b : bushes_) {
        if (!b.ripe) {
            if (t > b.regrowAt) {
                b.ripe = true;
                b.berries->visible = true;
                b.berries->scale = glm::vec3(0.01f);
            }
            continue;
        }
        if (b.berries->scale.x < 1) b.berries->scale = glm::vec3(std::min(1.0f, b.berries->scale.x + 0.03f));
        if (std::hypot(p.x - b.x, p.z - b.z) < 1.0f && std::abs(p.y - b.y) < 1.4f) {
            b.ripe = false;
            b.berries->visible = false;
            b.regrowAt = t + 45;
            picked += (int)b.berries->children.size();
        }
    }
    return picked;
}

CherryThrower::CherryThrower(Object3D& scene, const World* world) : scene_(scene), world_(world) {}

void CherryThrower::setWorld(const World* w) {
    world_ = w;
    while (!cherries.empty()) remove(cherries[0]);
}

void CherryThrower::throwCherry(glm::vec3 from, float yaw) {
    static auto stemMat = basic(hexc(0x3f7a2a));
    auto g = makeGroup("cherry");
    auto ball = makeMesh(geo::sphere(0.11f, 8, 6), berryMat());
    auto shine = makeMesh(geo::sphere(0.035f, 4, 3), shineMat());
    shine->position = {0.04f, 0.05f, 0.06f};
    auto stem = makeMesh(geo::cylinder(0.008f, 0.008f, 0.14f, 4), stemMat);
    stem->position.y = 0.13f;
    stem->rotation.z = 0.3f;
    auto glow = glowSprite(hexc(0xff5070), 0.6f, 0.5f);
    g->add(ball, shine, stem, glow);
    g->position = from;
    scene_.add(g);
    auto c = std::make_shared<Cherry>();
    c->obj = g;
    c->vel = {std::sin(yaw) * 6.5f, 5.5f, std::cos(yaw) * 6.5f};
    cherries.push_back(c);
}

void CherryThrower::remove(const std::shared_ptr<Cherry>& c) {
    auto keep = c;
    scene_.remove(keep->obj.get());
    cherries.erase(std::remove(cherries.begin(), cherries.end(), keep), cherries.end());
}

void CherryThrower::update(float dt) {
    for (int i = (int)cherries.size() - 1; i >= 0; --i) {
        auto c = cherries[i];
        c->age += dt;
        if (!c->landed) {
            c->vel.y -= 14 * dt;
            c->obj->position += c->vel * dt;
            c->obj->rotation.z += dt * 10;
            glm::vec3& p = c->obj->position;
            float g = world_->groundAt(p.x, p.z, p.y) + 0.1f;
            if (p.y <= g) {
                p.y = g;
                c->landed = true;
                c->obj->rotation.z = 0;
            }
        }
        if (c->age > 25 && !c->claimed) remove(c);
    }
}
