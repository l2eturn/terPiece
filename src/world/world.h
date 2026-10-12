#pragma once
// Common shape of the two realms (the API main.js expects from world.js / medieval.js):
// groundAt / resolveCollisions / cameraClearance / zoneAt / setTimeOfDay / update / addObject / addPlatform /
// addGround / isFree / clearTrees, plus the scene group and lighting environment of the realm.
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "scene/object.h"
#include "scene/renderer.h"

// circle you cannot walk through
struct Collider {
    float x, z, r;
    bool tree = false;
    bool removed = false;
};
// rotated box you cannot walk through (houses); `top` lets you stand on it when hasTop
struct Box {
    float x, z, hw, hd, rot, top;
    bool hasTop = true;
};
// circle you can stand on; below its top it blocks like a pillar unless `thin`
struct Platform {
    float x, z, r, top;
    bool thin = false;
};
// tree cone used to keep the camera out of trees
struct TreeShape {
    float x, z, base, h, r;
    bool removed = false;
};
// extra ground from other modules (voxel buildings): f(x, z, y) -> ground height or -infinity
using GroundFn = std::function<float(float x, float z, float y)>;

// What the top of a ground block is made of (world/terrain.cpp picks the texture, tint multiplies it)
enum class Ground { Snow, Grass, Cobble, Dirt, Stone };
struct Surface {
    Ground kind = Ground::Snow;
    glm::vec3 tint{1};
};

// Data the minimap draws (mapData in the JS)
struct MapData {
    std::string type;
    float radius = 80;
    // map centre in the area's coordinates (the wilds are not centred on the origin)
    glm::vec2 center{0};
    std::vector<glm::vec2> river;
    std::vector<std::vector<glm::vec2>> paths;
    const std::vector<TreeShape>* trees = nullptr;
    // winter
    glm::vec3 pond{0};
    std::vector<glm::vec2> cabins;
    glm::vec2 home{0};
    glm::vec3 hill{0};
    glm::vec4 mountain{0};
    glm::vec2 caveMouth{0}, portal{0};
    // medieval
    glm::vec4 castle{0}, drawbridge{0}, tavern{0}, garden{0};
    glm::vec3 fountain{0}, archery{0}, bellTower{0}, lake{0};
};

class World {
public:
    virtual ~World() = default;

    std::string name;
    glm::vec3 spawn{0};
    ObjPtr group = makeGroup();
    Environment env;
    MapData mapData;
    std::string timeOfDay = "night";

    virtual float groundAt(float x, float z, float y = 0) const = 0;
    virtual void resolveCollisions(glm::vec3& pos, float radius) const = 0;
    virtual bool isIce(float, float) const { return false; }
    virtual bool isWater(float, float) const { return false; }
    // river current at a point (m/s, x/z) and the river's downstream heading; false = still water
    virtual bool waterFlow(float, float, glm::vec2&, float&) const { return false; }
    // free spot for decorations: no collision, not on a path / ice / water
    virtual bool isFree(float x, float z, float r, bool ignoreTrees = false) const = 0;
    virtual std::string zoneAt(float x, float z) const = 0;
    // ground block surface at a point (in this realm's coordinates)
    virtual Surface surfaceAt(float, float) const { return {}; }
    virtual void setTimeOfDay(const std::string& name) = 0;
    virtual void update(float t, float dt, glm::vec3 focus) = 0;

    void setVisible(bool v) { group->visible = v; }
    // sky dome / stars / sun of this realm (only the realm you are closest to draws its sky)
    virtual void setSkyVisible(bool) {}
    bool visible() const { return group->visible; }
    void addObject(const ObjPtr& o) { group->add(o); }
    // returns an index usable with platform() (moving platforms update their top)
    size_t addPlatform(const Platform& p) {
        platforms_.push_back(p);
        return platforms_.size() - 1;
    }
    Platform& platform(size_t i) { return platforms_[i]; }
    void addGround(GroundFn f) { groundFns_.push_back(std::move(f)); }
    // remove trees inside a circle (room for buildings): hide them and drop their collider / camera shape
    void clearTrees(float x, float z, float r);
    // how far (0..1) the camera can sit along from->to before entering a tree or house
    float cameraClearance(glm::vec3 from, glm::vec3 to) const;

protected:
    bool blocksView(float px, float py, float pz) const;
    float extraGround(float x, float z, float y, float g) const;

    struct TreeRef {
        float x, z;
        int collider = -1, shape = -1;
        std::function<void()> hide;
        bool gone = false;
    };
    std::vector<Collider> colliders_;
    std::vector<Box> boxes_;
    std::vector<Platform> platforms_;
    std::vector<GroundFn> groundFns_;
    std::vector<TreeShape> treeShapes_;
    std::vector<TreeRef> trees_;
    std::vector<std::function<void(float t, float dt, glm::vec3 focus)>> updaters_;
    mutable bool skipTrees_ = false;
};

// distance from a point to a segment / to a set of polylines
float distToSegment(glm::vec2 p, glm::vec2 a, glm::vec2 b);
float distToPaths(const std::vector<std::vector<glm::vec2>>& paths, float x, float z);
