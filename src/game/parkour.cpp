// Port of parkour.js
#include "game/parkour.h"
#include <cmath>
#include "art/glow.h"
#include "art/textures.h"
#include "core/math.h"
#include "world/world.h"

namespace {

struct StepDef {
    float x, z, h, r;
    float bob = 0;
    std::string kind;
};
struct CourseDef {
    std::string id, world, style;
    bool hasCore = false;
    glm::vec4 core{0};
    std::vector<StepDef> steps;
};

// ice tower: a spiral around an ice pillar (snow map, east of the lake)
std::vector<StepDef> spiral(float cx, float cz, int n, float radius, float dh, float da, float start = 0.5f) {
    std::vector<StepDef> v;
    for (int i = 0; i < n; ++i) v.push_back({cx + std::cos(i * da) * radius, cz + std::sin(i * da) * radius, start + i * dh, 0.75f});
    return v;
}

const std::vector<CourseDef>& courses() {
    static const std::vector<CourseDef> C = {
        {"icespire", "winter", "ice", true, {46, -14, 1.4f, 9.6f}, spiral(46, -14, 12, 3.6f, 0.78f, 0.62f)},
        // the cave belongs to the snow map (x > 150)
        {"crystal", "winter", "crystal", false, {}, {
            {234.6f, -18.6f, 0.6f, 0.7f}, {236.2f, -16.9f, 1.2f, 0.65f}, {237.8f, -15.3f, 1.8f, 0.65f},
            {239.4f, -14.2f, 2.4f, 0.6f, 0.35f}, {241.1f, -14.9f, 3.0f, 0.65f}, {242.2f, -16.7f, 3.6f, 0.8f}}},
        {"rooftops", "medieval", "medieval", false, {}, {
            {30, -9, 0.6f, 0.7f, 0, "crate"}, {31.6f, -10.8f, 1.2f, 0.7f, 0, "crate"}, {33.4f, -12.2f, 1.8f, 0.7f, 0, "crate"},
            {35.4f, -13.4f, 2.4f, 0.55f, 0, "post"}, {37.4f, -14.8f, 3.0f, 0.55f, 0, "post"}, {38.6f, -17, 3.6f, 0.9f, 0, "hay"},
            {38.2f, -19.6f, 4.2f, 0.8f, 0.6f, "float"}, {36.6f, -21.6f, 4.8f, 0.55f, 0, "post"}, {34.4f, -22.8f, 5.4f, 1.2f, 0, "tower"}}},
    };
    return C;
}

MatPtr lam(const std::string& t, uint32_t color = 0xffffff, uint32_t emissive = 0) {
    auto m = lambert(tex::get(t), hexc(color));
    m->emissive = hexc(emissive);
    return m;
}
ObjPtr W(const GeoPtr& g, const MatPtr& m) { return makeMesh(geo::worldUV(g), m); }

}  // namespace

Parkour::Parkour(World& world, const std::string& worldName, const std::vector<std::string>& done,
                 std::function<void(const std::string&, glm::vec3)> onComplete)
    : world_(world), onComplete_(std::move(onComplete)) {
    for (const CourseDef& c : courses()) {
        if (c.world != worldName) continue;
        Course course;
        course.id = c.id;
        for (const StepDef& s : c.steps) {
            float gy = world.groundAt(s.x, s.z, -99);
            float top = gy + s.h;
            float colH = s.kind == "float" ? 0.4f : s.h + 0.3f;
            ObjPtr body;
            if (c.style == "ice") {
                body = W(geo::cylinder(s.r, s.r * 1.08f, colH, 7), lam("ice", 0xffffff, 0x1a3a55));
                auto cap = W(geo::cylinder(s.r * 1.02f, s.r * 1.02f, 0.12f, 7), lam("roofSnow"));
                cap->position.y = colH / 2 + 0.04f;
                body->add(cap);
            } else if (c.style == "crystal") {
                body = W(geo::cylinder(s.r, s.r * 1.15f, colH, 6), lam("stone", 0x9a92ae));
                const uint32_t cols[3] = {0x7fe8ff, 0xb58bff, 0x9dffd8};
                uint32_t col = cols[(int)std::floor(s.h * 10) % 3];
                auto cap = makeMesh(geo::cylinder(s.r * 0.95f, s.r, 0.1f, 6), basic(hexc(col)));
                cap->position.y = colH / 2 + 0.03f;
                auto glow = glowSprite(hexc(col), 1.6f, 0.6f);
                glow->position.y = colH / 2 + 0.2f;
                body->add(cap, glow);
            } else {
                std::string kind = s.kind.empty() ? "crate" : s.kind;
                if (kind == "crate") {
                    body = makeGroup();
                    float size = s.r * 1.6f;
                    int n = std::max(1, (int)std::lround(colH / size));
                    for (int i = 0; i < n; ++i) {
                        auto cm = makeMesh(geo::box(size, colH / n, size), lam("crate"));
                        cm->position.y = -colH / 2 + (i + 0.5f) * (colH / n);
                        cm->rotation.y = i * 0.35f;
                        cm->castShadow = cm->receiveShadow = true;
                        CwTag tag;
                        tag.name = "crate";
                        tag.hasFit = true;
                        tag.fit = {size, colH / n, size};
                        tag.center = true;
                        cm->cw.push_back(tag);
                        body->add(cm);
                    }
                } else if (kind == "hay") {
                    body = W(geo::cylinder(s.r, s.r, colH, 10), lam("planks", 0xe8c86a));
                } else if (kind == "tower") {
                    body = W(geo::cylinder(s.r, s.r * 1.1f, colH, 8), lam("stone", 0xc9b8a8));
                    auto ring = W(geo::cylinder(s.r * 1.1f, s.r * 1.1f, 0.2f, 8), lam("planks"));
                    ring->position.y = colH / 2;
                    body->add(ring);
                } else {
                    // post / float
                    body = W(geo::cylinder(s.r, s.r, colH, 8), lam("planks"));
                }
            }
            body->traverse([](Object3D& o) {
                if (o.kind == ObjectKind::Mesh) o.castShadow = o.receiveShadow = true;
            });
            body->position.y = top - colH / 2;
            auto g = makeGroup();
            g->add(body);
            g->position = {s.x, 0, s.z};
            world.addObject(g);
            size_t plat = world.addPlatform({s.x, s.z, s.r, top});
            course.steps.push_back({body, plat, top, colH, s.bob, s.x});
        }
        // central pillar (ice tower)
        if (c.hasCore) {
            float gy = world.groundAt(c.core.x, c.core.y, -99);
            float r = c.core.z, h = c.core.w;
            auto core = W(geo::cylinder(r * 0.85f, r, h, 8), lam("ice", 0xdff4ff, 0x16344d));
            core->position = {c.core.x, gy + h / 2, c.core.y};
            core->castShadow = true;
            auto cap = W(geo::cylinder(r * 0.9f, r * 0.9f, 0.15f, 8), lam("roofSnow"));
            cap->position = {c.core.x, gy + h + 0.05f, c.core.y};
            world.addObject(core);
            world.addObject(cap);
            world.addPlatform({c.core.x, c.core.y, r * 0.85f, gy + h + 0.1f});
            course.starPos = {c.core.x, gy + h + 0.1f, c.core.y};
        } else {
            const StepDef& last = c.steps.back();
            course.starPos = {last.x, course.steps.back().baseTop, last.z};
        }
        // the gold star
        Shape shape;
        for (int i = 0; i < 10; ++i) {
            float a = (i / 10.0f) * PI * 2 + PI / 2, rr = i % 2 ? 0.16f : 0.4f;
            if (i) shape.lineTo(std::cos(a) * rr, std::sin(a) * rr);
            else shape.moveTo(std::cos(a) * rr, std::sin(a) * rr);
        }
        ExtrudeOptions eo;
        eo.depth = 0.12f;
        eo.bevelEnabled = false;
        auto sg = geo::extrude(shape, eo);
        sg->center();
        course.starMat = basic(hexc(0xffd76a));
        course.star = makeMesh(sg, course.starMat);
        auto glow = glowSprite(hexc(0xffe08a), 2.4f, 0.8f);
        course.starGroup = makeGroup();
        course.starGroup->add(course.star, glow);
        course.starGroup->position = course.starPos + glm::vec3(0, 0.9f, 0);
        world.addObject(course.starGroup);
        course.done = std::find(done.begin(), done.end(), c.id) != done.end();
        if (course.done) course.starMat->color = hexc(0xd8dce8);
        courses_.push_back(course);
    }
}

void Parkour::update(float t, glm::vec3 p) {
    for (auto& c : courses_) {
        for (auto& s : c.steps) {
            if (!s.bob) continue;
            float y = std::sin(t * 1.2f + s.x) * s.bob;
            Platform& p = world_.platform(s.plat);
            p.top = s.baseTop + y;
            s.body->position.y = p.top - s.colH / 2;
        }
        c.star->rotation.y = t * 1.8f;
        c.starGroup->position.y = c.starPos.y + 0.9f + std::sin(t * 2) * 0.12f;
        if (!c.done && std::hypot(p.x - c.starPos.x, p.z - c.starPos.z) < 1.3f && p.y > c.starPos.y - 0.6f) {
            c.done = true;
            c.starMat->color = hexc(0xfff4c2);
            if (onComplete_) onComplete_(c.id, c.starPos);
        }
    }
}
