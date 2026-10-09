#pragma once
// The old Christmas room (last year's gift) placed in the village as "home" — port of home.js
#include <memory>
#include "scene/object.h"

class AnimationMixer;

class HomeDiorama {
public:
    // characters: Patt / Cham / Juice / snowman sitting in the room; darken: dim them like the original scene
    explicit HomeDiorama(bool characters = true, bool darken = false);
    ~HomeDiorama();
    ObjPtr group;
    void update(float dt);

private:
    struct Fire;
    std::unique_ptr<AnimationMixer> mixer_;
    std::unique_ptr<Fire> fire_;
    MatPtr neon1_, neon2_;
    float neon1Time_ = 1, neon2Time_ = 1;
};
