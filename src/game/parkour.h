#pragma once
// Parkour courses: platforms to climb with a gold star at the top — port of parkour.js
#include <functional>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "scene/object.h"

class World;

class Parkour {
public:
    // done = ids already completed (saved); onComplete(id, star position)
    Parkour(World& world, const std::string& worldName, const std::vector<std::string>& done,
            std::function<void(const std::string&, glm::vec3)> onComplete);
    // pos = the player's position in this course's world coordinates
    void update(float t, glm::vec3 pos);

private:
    struct Step {
        ObjPtr body;
        size_t plat;
        float baseTop, colH, bob, x;
    };
    struct Course {
        std::string id;
        std::vector<Step> steps;
        ObjPtr starGroup, star;
        MatPtr starMat;
        glm::vec3 starPos;
        bool done;
    };
    World& world_;
    std::vector<Course> courses_;
    std::function<void(const std::string&, glm::vec3)> onComplete_;
};
