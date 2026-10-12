#pragma once
// Medieval citadel — port of buildMedievalWorld() in medieval.js (castle you can walk into, moat, market,
// tavern, training grounds, rose garden, bell tower, swan lake with a jetty and a rowing boat, time portal)
#include "world/world.h"

namespace medieval {
constexpr float WORLD_R = 75;
// market square in front of the royal fountain
const glm::vec3 SPAWN(0, 0, 14);
// time portal back to the snow realm
const glm::vec3 PORTAL(0, 0, 52);
// water level of the moat and the lake
constexpr float WATER_Y = -0.45f;
struct Rect4 {
    float x, z, w, d, h;
};
const Rect4 CASTLE{0, -32, 26, 20, 9.5f};
struct Bridge {
    float x, z, w, d, top;
};
const Bridge DRAWBRIDGE{0, -17, 4.6f, 11, 0.25f};
struct Lake {
    float x, z, r;
};
const Lake LAKE{-40, -26, 11};
struct BoatSpot {
    float x, z, heading;
};
// end of the jetty
const BoatSpot BOAT_SPOT{LAKE.x + LAKE.r - 6.6f, LAKE.z, -3.14159265f / 2};
float heightAt(float x, float z);
}  // namespace medieval

class MedievalWorld : public World {
public:
    MedievalWorld();

    float groundAt(float x, float z, float y = 0) const override;
    void resolveCollisions(glm::vec3& pos, float radius) const override;
    bool isWater(float x, float z) const override { return medieval::heightAt(x, z) < medieval::WATER_Y - 0.15f; }
    bool isFree(float x, float z, float r, bool ignoreTrees = false) const override;
    std::string zoneAt(float x, float z) const override;
    Surface surfaceAt(float x, float z) const override;
    void setTimeOfDay(const std::string& name) override;
    void update(float t, float dt, glm::vec3 focus) override;

    std::vector<glm::vec2> berrySpots;
    void setSkyVisible(bool v) override { skyGroup_->visible = v; }

private:
    struct Tod {
        glm::vec3 top, mid, hor, fog, hemiS, hemiG, sun, win, dir;
        float hemiI, sunI, sunVis, torches;
    };
    static Tod preset(const std::string& name);
    void applyTOD(float k);
    void build();

    Rng rand_{420};
    Tod cur_, goal_;
    MatPtr skyMat_, mWindow_;
    ObjPtr skyGroup_, sunMesh_, sunHalo_;
    std::vector<ObjPtr> torchGlows_;
    std::vector<ObjPtr> torchLights_;
    struct Impl;
    friend struct Impl;
};
