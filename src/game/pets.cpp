// Port of createPet() / makeBubble() in entities.js and createCritterRig() in pixelart.js
#include "game/pets.h"
#include <cmath>
#include "art/glow.h"
#include "core/math.h"
#include "game/player.h"
#include "gfx/texture.h"
#include "ui/canvas2d.h"
#include "world/world.h"

namespace {
const std::string& pick(const std::vector<std::string>& v) { return v[(size_t)(frand() * v.size()) % v.size()]; }
}

// ---------- speech bubble ----------
Bubble::Bubble() : canvas_(std::make_unique<Canvas2D>(512, 128)), tex_(std::make_shared<Texture>()) {
    canvas_->upload(*tex_);
    auto m = spriteMaterial(tex_);
    m->depthTest = false;
    sprite = makeSprite(m);
    sprite->scale = {1.8f, 0.45f, 1};
    sprite->renderOrder = 20;
    sprite->visible = false;
}

Bubble::~Bubble() = default;

void Bubble::say(const std::string& text, float t, float dur) {
    Canvas2D& g = *canvas_;
    g.clear();
    g.setFont("Mali", 52, 600);
    float w = std::min(500.0f, g.measureText(text) + 48);
    float x = (512 - w) / 2;
    g.setFill("rgba(255,249,242,0.96)");
    g.setStroke("rgba(42,29,74,0.35)");
    g.setLineWidth(3);
    g.beginPath();
    g.roundRect(x, 6, w, 86, 40);
    g.moveTo(240, 90);
    g.lineTo(256, 120);
    g.lineTo(274, 90);
    g.closePath();
    g.fill();
    g.beginPath();
    g.roundRect(x, 6, w, 86, 40);
    g.moveTo(240, 90);
    g.lineTo(256, 120);
    g.lineTo(274, 90);
    g.stroke();
    g.setFill("#2a1d4a");
    g.setTextAlign(TextAlign::Center);
    g.setTextBaseline(TextBaseline::Middle);
    g.fillText(text, 256, 50);
    g.upload(*tex_);
    sprite->visible = true;
    until_ = t + dur;
}

void Bubble::update(float t) {
    if (sprite->visible && t > until_) sprite->visible = false;
}

// ---------- critter rig ----------
static float critterPpm(const std::string& kind) { return kind == "puppy" ? 0.055f : kind == "slid" ? 0.045f : 0.05f; }

CritterRig::CritterRig(const std::string& kind) : kind_(kind), actor_(kind, critterPpm(kind)) {}

void CritterRig::animate(float t, float dt, const std::string& state, float excite) {
    actor_.billboard();
    std::string dir = actor_.viewDir(actor_.facingYaw());
    bool flipL = dir == "sideL" || (dir == "back" && std::sin(t) > 0);
    auto& plane = actor_.plane;
    if (kind_ == "puppy" || kind_ == "slid") {
        // the cat curls up asleep until found
        if (state == "sleep") {
            actor_.setFrame("sleep");
            plane->scale = {1, 1 + std::sin(t * 1.5f) * 0.04f, 1};
            return;
        }
        plane->scale = glm::vec3(1);
        bool moving = state == "walk-moving";
        actor_.setFrame(std::string(flipL ? "sideL" : "side") + (moving && std::sin(t * 14) > 0 ? "1" : ""));
        plane->position.y = moving ? std::abs(std::sin(t * 14)) * 0.05f : 0;
        return;
    }
    if (state == "sleep") {
        actor_.setFrame("sleep");
        plane->scale = {1, 1 + std::sin(t * 1.5f) * 0.04f, 1};
        return;
    }
    if (state == "fly" || excite > 0.5f) {
        flapT_ += dt * 14;
        actor_.setFrame(std::sin(flapT_) > 0 ? "fly" : "perch");
        plane->position.y = std::sin(flapT_) * 0.03f;
    } else {
        actor_.setFrame("perch");
        plane->position.y = std::max(0.0f, std::sin(t * 5)) * 0.02f;
    }
    plane->scale.x = flipL ? -1 : 1;
}

// ---------- pet ----------
Pet::Pet(std::unique_ptr<CritterRig> rig, glm::vec3 spot, const std::string& mode, const PetOptions& opt)
    : rig_(std::move(rig)), mode_(mode), opt_(opt) {
    object = makeGroup("pet");
    object->add(rig_->object());
    object->position = spot;
    position = spot;
    heart_ = glowSprite(hexc(0xff7ab8), 1.6f, 0.6f);
    heart_->position.y = 0.6f;
    object->add(heart_);
    bubble_.sprite->position.y = mode == "walk" ? 1.0f : 0.8f;
    object->add(bubble_.sprite);
    nextLine_ = 3 + frand() * 5;
}

void Pet::find(float t) {
    found_ = true;
    foundAt_ = t;
    heart_->visible = false;
    excite_ = 1.5f;
}

void Pet::say(const std::string& text, float t, float dur) {
    bubble_.say(text, t, dur);
    nextLine_ = t + 7 + frand() * 6;
}

void Pet::fetch(glm::vec3 target, std::function<void()> onArrive) {
    fetching_ = true;
    fetchPhase_ = "go";
    fetchTarget_ = target;
    fetchTimer_ = 0;
    onArrive_ = std::move(onArrive);
}

void Pet::update(float t, float dt, const Player& player, int slot, const World& world) {
    glm::vec3& pos = object->position;
    bubble_.update(t);
    excite_ = std::max(0.0f, excite_ - dt);
    bool near = std::hypot(player.position.x - pos.x, player.position.z - pos.z) < 18;
    if (t > nextLine_ && !fetching_) {
        if (found_ && !opt_.lines.empty()) {
            nextLine_ = t + 7 + frand() * 6;
            bubble_.say(pick(opt_.lines), t);
        } else if (!found_ && near && !opt_.lostLines.empty() && opt_.idle != "sleep") {
            // still lost: calls out so you can hear it
            nextLine_ = t + 4 + frand() * 3;
            bubble_.say(pick(opt_.lostLines), t);
        }
    }
    if (fetching_) {
        if (fetchPhase_ == "go") {
            glm::vec3 tmp = fetchTarget_ + glm::vec3(0, 0.05f, 0);
            float d = glm::length(tmp - pos);
            float step = std::min(d, dt * 8);
            float dx = tmp.x - pos.x, dz = tmp.z - pos.z;
            if (dx * dx + dz * dz > 1e-4f) object->rotation.y = std::atan2(dx, dz);
            pos = glm::mix(pos, tmp, d > 0 ? step / d : 1.0f);
            // arc up mid-way so it looks like flying
            pos.y += std::min(0.6f, d * 0.15f) * dt * 4;
            rig_->animate(t, dt, "fly", 1);
            if (d < 0.2f) {
                fetchPhase_ = "eat";
                fetchTimer_ = 1.4f;
                if (onArrive_) onArrive_();
                bubble_.say(pick(opt_.eatLines), t, 1.8f);
            }
        } else {
            fetchTimer_ -= dt;
            rig_->animate(t, dt, "perch", 1);
            if (fetchTimer_ <= 0) fetching_ = false;
        }
        return;
    }
    if (!found_) {
        heart_->material()->opacity = 0.35f + std::sin(t * 4) * 0.25f;
        rig_->animate(t, dt, opt_.idle, 0);
        if (opt_.idle == "sleep" && (int)std::floor(t) % 4 == 0 && !bubble_.sprite->visible) {
            bool near2 = std::hypot(player.position.x - pos.x, player.position.z - pos.z) < 18;
            bubble_.say(near2 && !opt_.lostLines.empty() && frand() < 0.4f ? pick(opt_.lostLines) : "Zzz…", t, 1.8f);
        }
        return;
    }
    float yaw = player.yaw;
    float fx = std::sin(yaw), fz = std::cos(yaw);
    float k = std::min(1.0f, (t - foundAt_) * 2);
    if (mode_ == "shoulder") {
        // right shoulder — the one that cannot fly (Juice) stays glued there, no flapping
        glm::vec3 tmp(player.position.x - fz * 0.42f, player.position.y + 0.92f, player.position.z + fx * 0.42f);
        bool flying = opt_.canFly && glm::length(pos - tmp) > 0.15f;
        pos = glm::mix(pos, tmp, flying ? std::min(1.0f, dt * 5) : 1.0f);
        object->rotation.y = yaw;
        rig_->animate(t, dt, flying ? "fly" : "perch", opt_.canFly ? (player.grounded ? excite_ : 1) : 0);
        return;
    }
    if (mode_ == "fly") {
        float a = t * 1.4f;
        glm::vec3 tmp(player.position.x - fx * 1.1f + std::cos(a) * 0.9f, player.position.y + 1.7f + std::sin(t * 3) * 0.2f,
                      player.position.z - fz * 1.1f + std::sin(a) * 0.9f);
        float px = pos.x, pz = pos.z;
        pos = glm::mix(pos, tmp, std::min(1.0f, dt * (k < 1 ? 2 : 3)));
        float vx = pos.x - px, vz = pos.z - pz;
        if (vx * vx + vz * vz > 1e-7f) object->rotation.y = std::atan2(vx, vz);
        rig_->animate(t, dt, "fly", excite_);
        return;
    }
    // walk: trot along behind
    glm::vec3 tmp(player.position.x - fx * (1.2f + slot * 0.6f) + fz * 0.5f, 0, player.position.z - fz * (1.2f + slot * 0.6f) - fx * 0.5f);
    float d = std::hypot(tmp.x - pos.x, tmp.z - pos.z);
    if (d > 0.15f) {
        float sp = std::min(d * 3, 9.0f) * dt;
        float dx = (tmp.x - pos.x) / d, dz = (tmp.z - pos.z) / d;
        pos.x += dx * sp;
        pos.z += dz * sp;
        object->rotation.y = std::atan2(dx, dz);
    }
    // teleport after the player when left far behind
    if (d > 12) pos = {tmp.x, pos.y, tmp.z};
    world.resolveCollisions(pos, 0.25f);
    float g = world.groundAt(pos.x, pos.z, player.position.y + 0.5f);
    pos.y += (g - pos.y) * std::min(1.0f, dt * 12);
    rig_->animate(t, dt, d > 0.3f ? "walk-moving" : "walk", 0);
}
