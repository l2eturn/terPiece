#pragma once
// Player controller (pixel sprite, HD-2D) — port of createPlayer() / buildJetpack() in player.js.
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "art/sprites.h"

class World;

// Twin tanks + flames on the back (also drawn for other players in multiplayer)
struct Jetpack {
    ObjPtr group;
    std::vector<ObjPtr> flames;
};
Jetpack buildJetpack();
// f = 0 (off) .. 1 (full thrust)
void setFlames(std::vector<ObjPtr>& flames, float f);

class Player {
public:
    Player(const World* world, const std::string& kind);

    ObjPtr object;
    glm::vec3 position{0}, velocity{0};
    float yaw = 0;
    bool grounded = true;
    bool gliding = false;
    bool hasGlider = false;
    bool thrusting = false;
    bool lying = false;

    std::function<void(bool)> onGlide;
    std::function<void(glm::vec3, float)> onStep;
    std::function<void()> onJump, onLand;

    // sitting in a boat / on an animal / in bed: place directly, no physics
    void ride(glm::vec3 p, float facing);
    void setLying(bool v);
    void setWorld(const World* w) { world_ = w; }
    void teleport(glm::vec3 p, float facing = 3.14159265f);
    void jump() { jumpBuffer_ = 0.15f; }
    bool jetpack() const { return jetpack_; }
    void setJetpack(bool v);
    // move = joystick / keys (-1..1, z forward = -1), camYaw = camera yaw
    void update(float dt, glm::vec2 move, float camYaw, bool sprint, bool up = false);
    // happy hop when something is picked up
    void cheer(float t);

    // ---------- combat (game/combat.cpp) ----------
    // walking speed multiplier (slowed while swinging)
    float moveScale = 1;
    // dodge roll: fixed ground velocity for `time` seconds, the sprite spins
    void dash(glm::vec2 dir, float speed, float time);
    bool dashing() const { return dashTime_ > 0; }
    // turn to face a direction at once (aiming an attack)
    void face(float y) { yaw = y; }
    // tint the sprite (red when hurt); amount 0 = normal
    void tint(glm::vec3 color, float amount);
    // the sprite group (things held in hand hang off this)
    Object3D& body() { return *actor_->object; }

private:
    const World* world_;
    std::string kind_;
    std::unique_ptr<SpriteActor> actor_;
    ObjPtr glider_;
    Jetpack jet_;
    bool showGlider_;
    bool jetpack_ = false;
    float coyote_ = 0, jumpBuffer_ = 0, phase_ = 0, speedAmt_ = 0;
    int lastStepPhase_ = 0, jumpCount_ = 0;
    double clock_ = 0;
    float dashTime_ = 0, dashTotal_ = 0;
    glm::vec2 dashVel_{0};
    glm::vec3 baseEmissive_{0};
};
