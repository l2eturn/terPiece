#pragma once
// Glider stand (walk into it to get the glider) and the rowing boat — port of vehicles.js
#include <functional>
#include <optional>
#include <vector>
#include <glm/glm.hpp>
#include "scene/object.h"

class World;
class Player;

class GliderPickup {
public:
    GliderPickup(World& world, glm::vec3 pos, bool owned);
    // true on the frame it is picked up
    bool update(float t, const Player& player);

private:
    glm::vec3 pos_;
    ObjPtr cloth_, glow_;
    bool taken_;
};

class Boat {
public:
    struct Spot {
        float x, z, heading;
    };
    Boat(World& world, Spot spot, float waterY);
    ObjPtr object;
    glm::vec3 position() const { return pos_; }
    float heading() const { return heading_; }
    bool occupied() const { return occupied_; }
    void board() {
        occupied_ = true;
        speed_ = 0;
    }
    // back to the jetty
    void reset();
    void leave() {
        occupied_ = false;
        speed_ = 0;
    }
    // rower's seat / the bow (where the dog sits)
    glm::vec3 seat() const { return {pos_.x, waterY_ + 0.05f, pos_.z}; }
    glm::vec3 bow() const;
    // nearest spot to step ashore
    std::optional<glm::vec3> findShore(const World& world) const;
    void update(float t, float dt, glm::vec2 move, float camYaw, const World& world);

private:
    void place(float t);
    Spot spot_;
    float waterY_;
    glm::vec3 pos_;
    float heading_, speed_ = 0, rowPhase_ = 0;
    bool occupied_ = false;
    ObjPtr oars_[2];
};
