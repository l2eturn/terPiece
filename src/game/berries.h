#pragma once
// Berry bushes (walk past to pick, they regrow) and the cherry thrower — port of berries.js
#include <vector>
#include <glm/glm.hpp>
#include "scene/object.h"

class World;

class BerryBushes {
public:
    BerryBushes(Object3D& scene, const World& world, const std::vector<glm::vec2>& spots);
    ObjPtr object;
    // berries picked this frame
    int update(float t, glm::vec3 p);

private:
    struct Bush {
        float x, z, y;
        ObjPtr berries;
        bool ripe = true;
        float regrowAt = 0;
    };
    std::vector<Bush> bushes_;
};

class CherryThrower {
public:
    explicit CherryThrower(Object3D& scene, const World* world);
    struct Cherry {
        ObjPtr obj;
        glm::vec3 vel;
        bool landed = false, claimed = false;
        float age = 0;
    };
    std::vector<std::shared_ptr<Cherry>> cherries;
    void setWorld(const World* w);
    void throwCherry(glm::vec3 from, float yaw);
    void remove(const std::shared_ptr<Cherry>& c);
    void update(float dt);

private:
    Object3D& scene_;
    const World* world_;
};
