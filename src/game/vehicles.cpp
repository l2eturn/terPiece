// Port of vehicles.js
#include "game/vehicles.h"
#include <cmath>
#include "art/glow.h"
#include "art/textures.h"
#include "core/math.h"
#include "game/player.h"
#include "world/world.h"

namespace {
MatPtr lam(const std::string& t, uint32_t color = 0xffffff) { return lambert(tex::get(t), hexc(color)); }
ObjPtr W(const GeoPtr& g, const MatPtr& m) { return makeMesh(geo::worldUV(g), m); }
}  // namespace

// ---------- glider stand ----------
GliderPickup::GliderPickup(World& world, glm::vec3 pos, bool owned) : pos_(pos), taken_(owned) {
    auto g = makeGroup("glider-stand");
    auto wood = lam("planks", 0x9a7058);
    for (float sx : {-0.7f, 0.7f}) {
        auto leg = W(geo::box(0.12f, 1.4f, 0.12f), wood);
        leg->position = {sx, 0.7f, 0};
        g->add(leg);
    }
    auto rail = W(geo::box(1.6f, 0.12f, 0.12f), wood);
    rail->position.y = 1.4f;
    g->add(rail);
    // a display glider resting on the stand
    auto wing = geo::fromPositions({0, 0, 1.0f, -1.7f, -0.15f, -0.7f, 0, 0.12f, -0.45f, 0, 0, 1.0f, 0, 0.12f, -0.45f, 1.7f, -0.15f, -0.7f});
    wing->computeVertexNormals();
    auto cm = lambertColor(hexc(0xff5c9a));
    cm->side = Side::Double;
    cloth_ = makeMesh(wing, cm);
    cloth_->position.y = 1.55f;
    cloth_->rotation.x = -0.25f;
    // Cube World model when available (grip resting on the rail)
    CwTag tag;
    tag.name = "glider";
    tag.w = 2.4f;
    tag.rotY = PI;
    tag.offset = {0, -0.15f, 0};
    cloth_->cw.push_back(tag);
    glow_ = glowSprite(hexc(0xffd76a), 3.2f, 0.7f);
    glow_->position.y = 1.6f;
    g->add(cloth_, glow_);
    g->traverse([](Object3D& o) {
        if (o.kind == ObjectKind::Mesh) o.castShadow = true;
    });
    g->position = pos;
    world.addObject(g);
    if (taken_) cloth_->visible = glow_->visible = false;
}

bool GliderPickup::update(float t, const Player& player) {
    if (taken_) return false;
    cloth_->position.y = 1.55f + std::sin(t * 2) * 0.06f;
    glow_->material()->opacity = 0.5f + std::sin(t * 3) * 0.2f;
    glm::vec3 p = player.position;
    if (std::hypot(p.x - pos_.x, p.z - pos_.z) < 1.4f && std::abs(p.y - pos_.y) < 1.6f) {
        taken_ = true;
        cloth_->visible = glow_->visible = false;
        return true;
    }
    return false;
}

// ---------- rowing boat ----------
Boat::Boat(World& world, Spot spot, float waterY) : spot_(spot), waterY_(waterY), pos_(spot.x, waterY, spot.z), heading_(spot.heading) {
    object = makeGroup("boat");
    auto hullMat = lam("planks", 0xb07a52);
    auto trimMat = lam("planks", 0x6e4630);
    // hull: boat outline extruded downwards
    Shape shape;
    shape.moveTo(0, 1.6f);
    shape.quadTo(0.75f, 0.9f, 0.7f, -0.6f);
    shape.quadTo(0.6f, -1.25f, 0, -1.3f);
    shape.quadTo(-0.6f, -1.25f, -0.7f, -0.6f);
    shape.quadTo(-0.75f, 0.9f, 0, 1.6f);
    ExtrudeOptions eo;
    eo.depth = 0.45f;
    eo.bevelEnabled = false;
    auto hullGeo = geo::extrude(shape, eo);
    // flat, depth pointing down
    hullGeo->rotateX(PI / 2);
    hullGeo->translate(0, 0.3f, 0);
    auto hull = makeMesh(geo::worldUV(hullGeo, 1.5f), hullMat);
    auto innerGeo = geo::shape(shape);
    innerGeo->scale(0.85f, 0.85f, 1).rotateX(PI / 2).translate(0, 0.14f, 0);
    auto inner = makeMesh(innerGeo, trimMat);
    auto seatM = W(geo::box(1.2f, 0.08f, 0.3f), trimMat);
    seatM->position = {0, 0.22f, 0.1f};
    object->add(hull, inner, seatM);
    // oars on both sides (pivot at the gunwale)
    for (int i = 0; i < 2; ++i) {
        float s = i ? 1.0f : -1.0f;
        auto pivot = makeGroup();
        pivot->position = {s * 0.62f, 0.32f, 0.05f};
        auto shaft = makeMesh(geo::cylinder(0.025f, 0.025f, 1.6f, 5), trimMat);
        shaft->rotation.z = PI / 2;
        shaft->position.x = s * 0.6f;
        auto blade = makeMesh(geo::box(0.35f, 0.04f, 0.16f), trimMat);
        blade->position.x = s * 1.35f;
        pivot->add(shaft, blade);
        object->add(pivot);
        oars_[i] = pivot;
    }
    object->traverse([](Object3D& o) {
        if (o.kind == ObjectKind::Mesh) o.castShadow = o.receiveShadow = true;
    });
    // Cube World model when available (keeps our oars)
    CwTag tag;
    tag.name = "boat";
    tag.d = 3.0f;
    tag.offset = {0, -0.35f, 0.15f};
    object->cw.push_back(tag);
    object->cwKeep = {oars_[0].get(), oars_[1].get()};
    world.addObject(object);
    place(0);
}

void Boat::reset() {
    occupied_ = false;
    speed_ = 0;
    pos_ = {spot_.x, waterY_, spot_.z};
    heading_ = spot_.heading;
}

glm::vec3 Boat::bow() const {
    glm::vec3 f(std::sin(heading_), 0, std::cos(heading_));
    glm::vec3 b = pos_ + f * 0.85f;
    b.y = waterY_ + 0.08f;
    return b;
}

std::optional<glm::vec3> Boat::findShore(const World& world) const {
    for (float r = 1.6f; r <= 3.6f; r += 0.5f)
        for (int i = 0; i < 16; ++i) {
            float a = heading_ + (i / 16.0f) * PI * 2;
            float x = pos_.x + std::sin(a) * r, z = pos_.z + std::cos(a) * r;
            // land on the bank or the jetty (ground above the water)
            float gy = world.groundAt(x, z, 5);
            if (gy > waterY_ + 0.25f) return glm::vec3(x, gy, z);
        }
    return std::nullopt;
}

void Boat::place(float t) {
    object->position = {pos_.x, waterY_ - 0.12f + std::sin(t * 1.6f) * 0.04f, pos_.z};
    object->rotation = {std::sin(t * 1.3f) * 0.03f, heading_, std::sin(t * 1.1f) * 0.04f};
}

void Boat::update(float t, float dt, glm::vec2 move, float camYaw, const World& world) {
    if (occupied_) {
        // stick direction relative to the camera -> the boat turns that way and moves
        float mag = std::min(1.0f, std::hypot(move.x, move.y));
        if (mag > 0.1f) {
            float s = std::sin(camYaw), c = std::cos(camYaw);
            float dx = move.x * c + move.y * s, dz = -move.x * s + move.y * c;
            heading_ += wrapAngle(std::atan2(dx, dz) - heading_) * std::min(1.0f, dt * 1.8f);
        }
        speed_ += (mag * 3.6f - speed_) * std::min(1.0f, dt * 1.2f);
        // on a river: drift with the current and ease the bow along the channel (up- or downstream)
        glm::vec2 flow;
        float along;
        if (world.waterFlow(pos_.x, pos_.z, flow, along)) {
            float down = wrapAngle(along - heading_), up = wrapAngle(along + PI - heading_);
            if (speed_ > 0.3f) {
                if (std::abs(down) < 1.0f) heading_ += down * std::min(1.0f, dt * 0.9f);
                else if (std::abs(up) < 1.0f) heading_ += up * std::min(1.0f, dt * 0.9f);
            }
            float cx = pos_.x + flow.x * dt, cz = pos_.z + flow.y * dt;
            if (world.isWater(cx, cz)) {
                pos_.x = cx;
                pos_.z = cz;
            }
        }
        glm::vec3 f(std::sin(heading_), 0, std::cos(heading_));
        float nx = pos_.x + f.x * speed_ * dt, nz = pos_.z + f.z * speed_ * dt;
        // never run aground (check the bow)
        if (world.isWater(nx + f.x * 1.4f, nz + f.z * 1.4f)) {
            pos_.x = nx;
            pos_.z = nz;
        } else {
            speed_ *= 0.5f;
        }
        // row in time with the speed
        rowPhase_ += dt * (1.5f + speed_ * 2.2f);
        float sw = speed_ > 0.2f ? std::sin(rowPhase_) : 0;
        oars_[0]->rotation = {0, sw * 0.6f, -0.35f + std::cos(rowPhase_) * 0.25f * std::min(1.0f, speed_)};
        oars_[1]->rotation = {0, -sw * 0.6f, 0.35f - std::cos(rowPhase_) * 0.25f * std::min(1.0f, speed_)};
    } else {
        oars_[0]->rotation = {0, 0, -0.35f};
        oars_[1]->rotation = {0, 0, 0.35f};
    }
    place(t);
}
