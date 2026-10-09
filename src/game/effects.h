#pragma once
// Particle effects — port of effects.js: snowfall, fireworks, footprints in the snow.
#include <vector>
#include <glm/glm.hpp>
#include "scene/object.h"

class Snowfall {
public:
    explicit Snowfall(int count = 2600);
    ObjPtr object;
    void update(float t, glm::vec3 center);

private:
    float time_ = 0;
    glm::vec3 center_{0};
};

class Fireworks {
public:
    explicit Fireworks(int capacity = 3000);
    ObjPtr object;
    // burst of sparks; color < 0 picks a random palette colour
    void burst(glm::vec3 p, glm::vec3 color = glm::vec3(-1), int n = 140);
    void launch(glm::vec3 from, float height = 16);
    void update(float dt);

private:
    struct Rocket {
        glm::vec3 p;
        float vy, t, target;
    };
    int capacity_, cursor_ = 0;
    GeoPtr geo_;
    std::vector<glm::vec3> vel_;
    std::vector<float> life_;
    std::vector<Rocket> rockets_;
};

class Footprints {
public:
    explicit Footprints(int count = 180);
    ObjPtr object;
    void add(glm::vec3 p, float yaw, float t);
    void update(float t);

private:
    struct Print {
        glm::vec3 p{0};
        float yaw = 0;
    };
    std::vector<Print> data_;
    std::vector<float> born_;
    int cursor_ = 0;
};
