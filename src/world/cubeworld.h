#pragma once
// Cube World voxel models — port of cubeworld.js.
// The models are NOT part of this repo (they are from a cracked copy of Cube World): they are read at runtime
// from $CW_ASSETS, or ../patty-project/xmas-app/web/assets/cubeworld by default. Without them the game still
// runs with its own low-poly models. Never ship a build together with those files.
//
// Three jobs: (1) replace tagged objects (Object3D::cw) with voxel models, (2) scatter decorations (LAYOUT),
// (3) walking animals you can ride (HERDS), plus enterable buildings with furniture (bed / cooking pot).
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "scene/object.h"

class World;

class CubeWorld {
public:
    // Reads manifest.json; returns false when the asset folder is missing
    bool init();
    bool available() const { return ok_; }

    // cwGeometry(): scaled / centred / rotated / snow-dusted copy of a model, cached by options
    GeoPtr geometry(const std::string& name, const CwTag& opt = {});
    ObjPtr mesh(const std::string& name, const CwTag& opt = {});
    bool has(const std::string& name);
    // model size in voxels [w, h, d]
    glm::vec3 size(const std::string& name) const;

    // swap every tagged object under root
    void applyTags(Object3D& root);
    // decorations, buildings and herds for one realm. avoid = {x, z, r} spots buildings must not cover
    void populate(World& world, float waterY, const std::vector<glm::vec3>& avoid);

    void update(float t, float dt, World& world, glm::vec3 playerPos);
    // drop the source models once the world is built (sized copies stay cached; has() reloads a source on demand)
    void releaseSources() { base_.clear(); }

    // ---------- riding (pony in the town / alpaca in the snow village) ----------
    struct Ride {
        virtual ~Ride() = default;
        virtual bool active() const = 0;
        virtual std::string kind() const = 0;
        // x, y, z, heading of the animal being ridden (sent to other players)
        virtual glm::vec4 pose() const = 0;
        virtual void* nearest(glm::vec3 pos, float r = 1.9f) = 0;
        virtual void mount(void* animal) = 0;
        // returns a standing spot next to the animal
        virtual glm::vec3 dismount() = 0;
        virtual void jump() = 0;
        // returns the yaw for player.ride(); writes the seat position
        virtual float update(float t, float dt, glm::vec2 move, float camYaw, bool sprint, glm::vec3& seat) = 0;
    };
    Ride* ride(World& world);

    // ---------- things inside the house you can press E on ----------
    struct Table;
    struct Act {
        std::string kind;
        glm::vec3 pos;
        Table* table = nullptr;
    };
    const Act* nearestAct(World& world, glm::vec3 pos, float r = 1.9f);
    // cook a random dish onto the table; returns its name or "" when not possible
    std::string cook(const Act& act);

    // bare animal model without AI (for other players' mounts)
    ObjPtr animalModel(const std::string& kind);

    // head / body / feet model of an animal or monster kind (g = root, rig = bobbing body, legs swing)
    struct AnimalModel {
        ObjPtr g, rig, head;
        std::vector<ObjPtr> legs;
        float height, seatY, bodyD;
    };
    // true when every part of that kind is in the asset folder
    bool hasAnimal(const std::string& kind);
    AnimalModel buildAnimal(const std::string& kind);

    // a flickering voxel fire with its own point light
    static ObjPtr createFire(glm::vec3 p);

private:
    struct PerWorld;
    struct Animal;
    struct Herds;
    struct Building;
    void placeBuildings(World& world, PerWorld& pw, std::vector<struct Placed>& placed, const std::vector<glm::vec3>& avoid);
    void placeProps(World& world, PerWorld& pw, float waterY, std::vector<struct Placed>& placed);
    void createHerds(World& world, PerWorld& pw, const std::vector<glm::vec3>& blocked);
    void furnish(World& world, PerWorld& pw, glm::vec3 floor, float rot);

    bool ok_ = false;
    std::string dir_;
    struct Info {
        glm::vec3 size;
        bool cols = false;
    };
    std::map<std::string, Info> manifest_;
    std::map<std::string, GeoPtr> base_;
    std::map<std::string, GeoPtr> cache_;
    MatPtr mat_;
    std::map<std::string, std::unique_ptr<PerWorld>> per_;
};

CubeWorld& cubeWorld();
