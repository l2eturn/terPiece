// Port of player.js — same constants and order of operations as the web build.
#include "game/player.h"
#include <cmath>
#include "core/math.h"
#include "world/world.h"

namespace {
constexpr float GRAVITY = 24, JUMP_V = 8.4f;
}

// The tanks sit 0.55 m behind the body: the sprite turns to the camera, closer tanks would cover the face
Jetpack buildJetpack() {
    Jetpack j;
    j.group = makeGroup();
    auto metal = lambertColor(hexc(0x8a8f9a));
    auto red = lambertColor(hexc(0xd84a3a));
    auto strap = makeMesh(geo::box(0.36f, 0.3f, 0.06f), lambertColor(hexc(0x3a3440)));
    strap->position = {0, 0.78f, -0.42f};
    j.group->add(strap);
    for (float sx : {-0.13f, 0.13f}) {
        auto tank = makeMesh(geo::cylinder(0.09f, 0.09f, 0.48f, 8), metal);
        tank->position = {sx, 0.74f, -0.55f};
        auto cap = makeMesh(geo::sphere(0.09f, 8, 6, 0, PI * 2, 0, PI / 2), red);
        cap->position = {sx, 0.98f, -0.55f};
        auto nozzle = makeMesh(geo::cylinder(0.06f, 0.08f, 0.1f, 8), metal);
        nozzle->position = {sx, 0.45f, -0.55f};
        auto fm = basic(hexc(0xffa236));
        fm->transparent = true;
        fm->opacity = 0.9f;
        auto flame = makeMesh(geo::cone(0.07f, 0.35f, 8), fm);
        flame->rotation.x = PI;
        flame->position.z = -0.55f;
        flame->visible = false;
        j.flames.push_back(flame);
        j.group->add(tank, cap, nozzle, flame);
    }
    j.group->traverse([](Object3D& o) {
        if (o.kind == ObjectKind::Mesh) o.castShadow = true;
    });
    return j;
}

void setFlames(std::vector<ObjPtr>& flames, float f) {
    for (auto& fl : flames) {
        fl->visible = f > 0;
        fl->scale = {1, std::max(0.01f, f * (0.8f + frand() * 0.4f)), 1};
        fl->position.y = 0.4f - 0.16f * fl->scale.y;
    }
}

Player::Player(const World* world, const std::string& kind) : world_(world), kind_(kind) {
    actor_ = std::make_unique<SpriteActor>(kind, 0.06f);
    object = makeGroup("player");
    object->add(actor_->object);
    showGlider_ = kind != "momonga";

    // ---------- hang glider (Cube World style) ----------
    glider_ = makeGroup();
    {
        // two delta wings
        auto wing = geo::fromPositions({0, 0, 1.0f, -1.7f, -0.15f, -0.7f, 0, 0.12f, -0.45f, 0, 0, 1.0f, 0, 0.12f, -0.45f, 1.7f, -0.15f, -0.7f});
        wing->computeVertexNormals();
        auto cm = lambertColor(hexc(0xff5c9a));
        cm->side = Side::Double;
        auto cloth = makeMesh(wing, cm);
        auto stripeGeo = geo::fromPositions({0, 0.01f, 0.75f, -0.55f, -0.04f, -0.2f, 0, 0.11f, -0.3f, 0, 0.01f, 0.75f, 0, 0.11f, -0.3f, 0.55f, -0.04f, -0.2f});
        stripeGeo->computeVertexNormals();
        auto sm = lambertColor(hexc(0xfff1c4));
        sm->side = Side::Double;
        auto stripe = makeMesh(stripeGeo, sm);
        auto frameMat = lambertColor(hexc(0x3a2a24));
        auto keel = makeMesh(geo::cylinder(0.025f, 0.025f, 1.5f, 5), frameMat);
        keel->rotation.x = PI / 2;
        keel->position = {0, 0.04f, 0.25f};
        auto bar = makeMesh(geo::cylinder(0.02f, 0.02f, 0.9f, 5), frameMat);
        bar->rotation.z = PI / 2;
        bar->position = {0, -0.55f, 0.15f};
        for (float sx : {-0.4f, 0.4f}) {
            auto strut = makeMesh(geo::cylinder(0.015f, 0.015f, 0.62f, 4), frameMat);
            strut->position = {sx * 0.5f, -0.27f, 0.15f};
            strut->rotation.z = sx * 0.9f;
            glider_->add(strut);
        }
        cloth->castShadow = true;
        glider_->add(cloth, stripe, keel, bar);
        glider_->position.y = 2.15f;
        glider_->visible = false;
        // Cube World model when available: nose points -Z so turn it around; the grip (model base) sits at ~1.0 m
        // = the hands beside the ears in the G pose; pushed behind the head so it never covers the face
        if (showGlider_) {
            CwTag tag;
            tag.name = "glider";
            tag.w = 2.4f;
            tag.rotY = PI;
            tag.offset = {0, -1.15f, -0.55f};
            glider_->cw.push_back(tag);
        }
        object->add(glider_);
    }

    // ---------- jetpack (Ter / Momonga) ----------
    jet_ = buildJetpack();
    jet_.group->visible = false;
    object->add(jet_.group);
}

void Player::ride(glm::vec3 p, float facing) {
    position = p;
    velocity = glm::vec3(0);
    yaw = facing;
    grounded = true;
    gliding = false;
    glider_->visible = false;
    object->position = position;
    actor_->object->rotation.y = yaw;
    actor_->billboard();
    actor_->plane->position.y = 0;
    actor_->plane->scale = glm::vec3(1);
    actor_->setFrame(actor_->viewDir(yaw));
    actor_->plane->rotation.z = lying ? PI / 2 : 0;
}

void Player::setLying(bool v) {
    lying = v;
    if (!v) actor_->plane->rotation.z = 0;
}

void Player::teleport(glm::vec3 p, float facing) {
    position = p;
    position.y = world_->groundAt(p.x, p.z, 99);
    velocity = glm::vec3(0);
    yaw = facing;
}

void Player::setJetpack(bool v) {
    jetpack_ = v;
    jet_.group->visible = v;
    if (v && gliding) {
        gliding = false;
        glider_->visible = false;
    }
}

void Player::update(float dt, glm::vec2 move, float camYaw, bool sprint, bool up) {
    clock_ += dt;
    float mag = std::min(1.0f, std::hypot(move.x, move.y));
    // camera-relative input -> world direction
    float s = std::sin(camYaw), c = std::cos(camYaw);
    float dx = move.x * c + move.y * s, dz = -move.x * s + move.y * c;
    float dl = std::hypot(dx, dz);
    if (dl > 0) {
        dx /= dl;
        dz /= dl;
    }

    bool onIce = grounded && world_->isIce(position.x, position.z);
    if (gliding) {
        // gliding: keep flying forward, steer with the stick
        float fx = mag > 0.1f ? dx : std::sin(yaw), fz = mag > 0.1f ? dz : std::cos(yaw);
        float speed = sprint ? 11 : 8.5f;
        velocity.x += (fx * speed - velocity.x) * std::min(1.0f, 1.6f * dt);
        velocity.z += (fz * speed - velocity.z) * std::min(1.0f, 1.6f * dt);
    } else {
        bool swimming = world_->isWater(position.x, position.z) && grounded && position.y < -0.6f;
        float maxSpeed = (sprint ? 7.5f : 4.6f) * mag * (swimming ? 0.5f : 1) * moveScale;
        float accel = !grounded ? 6 : onIce ? 1.6f : 14;
        velocity.x += (dx * maxSpeed - velocity.x) * std::min(1.0f, accel * dt);
        velocity.z += (dz * maxSpeed - velocity.z) * std::min(1.0f, accel * dt);
    }

    // jump with coyote time + jump buffer
    jumpBuffer_ -= dt;
    coyote_ = grounded ? 0.12f : coyote_ - dt;
    if (jumpBuffer_ > 0 && coyote_ <= 0 && !grounded && hasGlider && !jetpack_) {
        // jump in mid-air = open / close the glider
        gliding = !gliding;
        jumpBuffer_ = 0;
        if (gliding) velocity.y = std::max(velocity.y, -1.0f);
        glider_->visible = gliding && showGlider_;
        if (onGlide) onGlide(gliding);
    } else if (jumpBuffer_ > 0 && coyote_ > 0) {
        velocity.y = JUMP_V;
        grounded = false;
        coyote_ = 0;
        jumpBuffer_ = 0;
        ++jumpCount_;
        if (onJump) onJump();
    }

    thrusting = jetpack_ && up;
    if (thrusting) {
        // jetpack: fly up
        velocity.y += (6 - velocity.y) * std::min(1.0f, dt * 4);
        grounded = false;
    } else if (jetpack_ && !grounded) {
        // released: drift down slowly
        velocity.y += (-2.2f - velocity.y) * std::min(1.0f, dt * 2.5f);
    } else if (gliding) {
        velocity.y += (-1.7f - velocity.y) * std::min(1.0f, dt * 2.5f);
    } else {
        velocity.y -= GRAVITY * dt;
    }
    // dodge roll overrides the walking velocity
    if (dashTime_ > 0) {
        dashTime_ -= dt;
        velocity.x = dashVel_.x;
        velocity.z = dashVel_.y;
        yaw = std::atan2(dashVel_.x, dashVel_.y);
    }
    float ox = position.x, oz = position.z;
    position += velocity * dt;
    world_->resolveCollisions(position, 0.28f);
    // ledges taller than 0.5 m need a jump (try sliding along each axis first)
    const float STEP = 0.5f;
    auto tooHigh = [&](float x, float z) { return world_->groundAt(x, z, position.y) > position.y + STEP; };
    if (tooHigh(position.x, position.z)) {
        if (!tooHigh(position.x, oz)) {
            position.z = oz;
            velocity.z = 0;
        } else if (!tooHigh(ox, position.z)) {
            position.x = ox;
            velocity.x = 0;
        } else {
            position.x = ox;
            position.z = oz;
            velocity.x = velocity.z = 0;
        }
    }

    float g = world_->groundAt(position.x, position.z, position.y);
    bool wasGrounded = grounded;
    if (position.y <= g) {
        if (gliding) {
            gliding = false;
            glider_->visible = false;
            if (onGlide) onGlide(false);
        }
        if (!wasGrounded && velocity.y < -6 && onLand) onLand();
        position.y = g;
        velocity.y = 0;
        grounded = true;
    } else if (wasGrounded && velocity.y <= 0 && position.y - g < 0.5f) {
        // walking down a slope or one ground block: stay glued to the ground
        position.y = g;
        velocity.y = 0;
    } else {
        grounded = false;
    }

    // face the walking direction
    float hs = std::hypot(velocity.x, velocity.z);
    if (mag > 0.1f && hs > 0.3f) yaw += wrapAngle(std::atan2(dx, dz) - yaw) * std::min(1.0f, dt * 12);
    object->position = position;
    object->rotation.y = 0;
    actor_->object->rotation.y = yaw;
    if (gliding) {
        // glider follows the heading and banks into turns
        float turn = wrapAngle(yaw - glider_->rotation.y);
        glider_->rotation.y += turn;
        glider_->rotation.z += (-turn * 8 - glider_->rotation.z) * std::min(1.0f, dt * 4);
        glider_->rotation.x = -0.12f;
    } else {
        glider_->rotation = {0, yaw, 0};
    }

    // jetpack turns with the body; strong flame going up, soft while drifting down
    if (jetpack_) {
        jet_.group->rotation.y = yaw;
        setFlames(jet_.flames, thrusting ? 1 : !grounded ? 0.45f : 0);
    }

    // ---------- animation: frame by facing relative to the camera ----------
    float walk = grounded ? std::min(1.0f, hs / 4.6f) : 0;
    speedAmt_ += (walk - speedAmt_) * std::min(1.0f, dt * 10);
    phase_ += dt * (5 + hs * 1.6f) * (onIce ? 0.3f : 1);
    actor_->billboard();
    actor_->plane->position.y = 0;
    std::string dir = actor_->viewDir(yaw);
    std::string step;
    // arms up holding the glider
    if (gliding) step = "G";
    else if (!grounded) step = "1";
    else if (speedAmt_ > 0.2f && !onIce) {
        int k = (int)std::floor(phase_ / (PI / 2)) % 4;
        step = k == 1 ? "1" : k == 3 ? "3" : "";
    }
    actor_->setFrame(dir + step);
    // wobble while sliding on ice, squash and stretch while jumping
    actor_->plane->rotation.z = onIce && hs > 1 ? std::sin((float)clock_ * 1000 / 300) * 0.12f : 0;
    // one full spin during a dodge roll
    if (dashTime_ > 0) actor_->plane->rotation.z = (1 - dashTime_ / dashTotal_) * PI * 2;
    float squash = !grounded ? (velocity.y > 0 ? 1.08f : 0.97f) : 1;
    actor_->plane->scale = {2 - squash, squash, 1};

    // footprint signal
    if (grounded && !onIce && speedAmt_ > 0.3f && (int)std::floor(phase_ / PI) != lastStepPhase_) {
        lastStepPhase_ = (int)std::floor(phase_ / PI);
        float side = lastStepPhase_ % 2 ? 1.0f : -1.0f;
        if (onStep)
            onStep({position.x + std::cos(yaw) * 0.09f * side, position.y + 0.02f, position.z - std::sin(yaw) * 0.09f * side}, yaw);
    }
}

void Player::dash(glm::vec2 dir, float speed, float time) {
    float l = glm::length(dir);
    if (l < 1e-4f) dir = {std::sin(yaw), std::cos(yaw)};
    else dir /= l;
    dashVel_ = dir * speed;
    dashTime_ = dashTotal_ = time;
}

void Player::tint(glm::vec3 color, float amount) {
    auto& m = *actor_->plane->material();
    if (baseEmissive_ == glm::vec3(0)) baseEmissive_ = m.emissive;
    m.color = glm::mix(glm::vec3(1), color, amount);
    m.emissive = glm::mix(baseEmissive_, color, amount);
}

void Player::cheer(float t) { actor_->plane->position.y = std::abs(std::sin(t * 14)) * 0.15f; }
