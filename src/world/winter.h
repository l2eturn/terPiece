#pragma once
// Snow village — port of buildWorld() in patty-project/xmas-app/web/js/world.js (metres, -Z = north)
#include <map>
#include "world/world.h"

namespace winter {
constexpr float WORLD_R = 80;
// village square in front of the Christmas tree
const glm::vec3 SPAWN(0, 0, 8);
// stone gate to the medieval realm
const glm::vec3 WINTER_PORTAL(0, 0, 36);
struct Circle {
    float x, z, r;
};
const Circle POND{30, -2, 7.5f};
struct HomeRect {
    float x, z, w, d;
};
const HomeRect HOME{0, -32, 8, 6.5f};
const glm::vec3 HOME_DOOR(HOME.x, 0, HOME.z + 5.2f);
struct Mountain {
    float x, z, r, h, step;
};
// terraces are two ground blocks tall (the web build uses 1.1 m)
const Mountain MOUNTAIN{12, -66, 26, 22, 2 * 0.45f};
// crystal cave interior lives far outside the main map (x > 150) and is reached by warping
const glm::vec2 CAVE(220, 0);
const glm::vec2 CAVE_MOUTH(MOUNTAIN.x - 23, MOUNTAIN.z + 6);
const glm::vec3 CAVE_ENTRY(CAVE_MOUTH.x - 2.2f, 0, CAVE_MOUTH.y + 1.2f);
const glm::vec3 CAVE_SPAWN(CAVE.x, 0, CAVE.y - 1.5f);
inline bool isInCave(float x) { return x > 150; }

float heightAt(float x, float z);
bool isIceAt(float x, float z);
}  // namespace winter

class WinterWorld : public World {
public:
    // Builds the scene graph on the CPU; textures are created lazily (needs a GL context).
    WinterWorld();

    float groundAt(float x, float z, float y = 0) const override;
    void resolveCollisions(glm::vec3& pos, float radius) const override;
    bool isIce(float x, float z) const override { return winter::isIceAt(x, z); }
    bool isFree(float x, float z, float r, bool ignoreTrees = false) const override;
    std::string zoneAt(float x, float z) const override;
    Surface surfaceAt(float x, float z) const override;
    void setTimeOfDay(const std::string& name) override;
    void update(float t, float dt, glm::vec3 focus) override;

    // 0..1 lights on the Christmas tree (main.js sets 1)
    void setTreeProgress(float f) { treeProgress_ = f; }
    // door glow + warm light of the old Christmas room
    void setHomeReady(bool on);
    void setSkyVisible(bool v) override { skyGroup_->visible = v; }

    // where the old Christmas room (home.cpp) is placed
    glm::vec3 homeAnchorPos;
    float homeAnchorRotY = -PI / 4;
    std::map<std::string, glm::vec3> petSpots;
    glm::vec3 snowmanSpot;

private:
    struct Tod {
        glm::vec3 top, mid, hor, fog, hemiS, hemiG, sun, win, dir;
        float hemiI, sunI, stars, aurora, moon, lamps;
    };
    static Tod preset(const std::string& name);
    void applyTOD(float k);

    void buildSky();
    void buildPond();
    void buildTrees();
    void lamp(float x, float z);
    void buildLamps();
    void cabin(float x, float z, float w, float d, float h, bool lights, const MatPtr& roof);
    void buildHomeAndProps();
    void buildPortal();
    float crate(float x, float z, int level);
    float rock(float x, float z, float h, float r = 0.8f);
    void buildChristmasTree();
    void buildSummit();
    void buildCaveMouth();
    void buildCave();

    Rng rand_{1225};
    Tod cur_, goal_;
    float caveK_ = 0;
    float treeProgress_ = 0, treeShown_ = 0;

    // materials shared like M in world.js
    MatPtr mSnow, mWood, mWoodDark, mPlaster, mRoof, mRoofSnow, mStone, mWindow, mBulbWarm, mPost, mCrate, mDoor;
    MatPtr skyMat_, starsMat_, auroraMat_;
    ObjPtr skyGroup_, sky_, moon_, moonHalo_, doorGlow_, homeLight_, lantern_;
    std::vector<ObjPtr> lampGlows_, caveLights_;
    struct Bulb {
        MatPtr mat;
        ObjPtr glow;
        glm::vec3 color;
        float phase;
    };
    std::vector<Bulb> bulbs_;
    ObjPtr treeLight_, star_, starGlow_;
};
