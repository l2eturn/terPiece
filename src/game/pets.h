#pragma once
// Companions — port of createPet() in entities.js and createCritterRig() in pixelart.js.
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "art/sprites.h"

class World;
class Player;
class Canvas2D;
class Texture;

// Speech bubble above a head (the parrots talk!)
class Bubble {
public:
    Bubble();
    ~Bubble();
    ObjPtr sprite;
    bool busy() const { return sprite->visible; }
    void say(const std::string& text, float t, float dur = 2.6f);
    void update(float t);

private:
    std::unique_ptr<Canvas2D> canvas_;
    std::shared_ptr<Texture> tex_;
    float until_ = 0;
};

// Sprite rig for birds / dog / cat with its animate()
class CritterRig {
public:
    explicit CritterRig(const std::string& kind);
    ObjPtr object() const { return actor_.object; }
    // state: perch / fly / sleep / walk / walk-moving
    void animate(float t, float dt, const std::string& state, float excite = 0);

private:
    std::string kind_;
    SpriteActor actor_;
    float flapT_ = 0;
};

struct PetOptions {
    // look while still lost: perch / sleep / walk
    std::string idle = "perch";
    std::vector<std::string> lines, lostLines, eatLines{"อร่อย! ♥"};
    bool canFly = true;
};

// mode: shoulder = sits on the shoulder, fly = circles around, walk = follows on foot
class Pet {
public:
    Pet(std::unique_ptr<CritterRig> rig, glm::vec3 spot, const std::string& mode, const PetOptions& opt);

    std::string key;
    ObjPtr object;
    // spot where it waits while lost (used by the minimap)
    glm::vec3 position;
    bool found() const { return found_; }
    void find(float t);
    void say(const std::string& text, float t, float dur = 2.6f);
    bool talking() const { return bubble_.busy(); }
    void cheer() { excite_ = 1.2f; }
    bool busy() const { return fetching_; }
    bool canFly() const { return opt_.canFly; }
    // fly to the target, eat it
    void fetch(glm::vec3 target, std::function<void()> onArrive);
    void update(float t, float dt, const Player& player, int slot, const World& world);

private:
    std::unique_ptr<CritterRig> rig_;
    std::string mode_;
    PetOptions opt_;
    Bubble bubble_;
    ObjPtr heart_;
    bool found_ = false;
    float foundAt_ = 0, nextLine_, excite_ = 0;
    bool fetching_ = false;
    std::string fetchPhase_;
    glm::vec3 fetchTarget_{0};
    float fetchTimer_ = 0;
    std::function<void()> onArrive_;
};
