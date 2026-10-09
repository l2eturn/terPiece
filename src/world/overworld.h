#pragma once
// The open world: the snow village + the medieval citadel + "the wilds" valley between them.
// Physics calls are dispatched to whichever area a point is in (converting to that town's local
// coordinates); the wilds themselves — road, river, forest, jetties — are built here in global coordinates.
#include <memory>
#include <string>
#include "world/medieval.h"
#include "world/winter.h"

class Overworld : public World {
public:
    // area names: "winter" / "medieval" / "wilds"
    struct Area {
        World* world;
        glm::vec2 offset;
        float rot;
    };

    Overworld(WinterWorld& winter, MedievalWorld& medieval);

    float groundAt(float x, float z, float y = 0) const override;
    void resolveCollisions(glm::vec3& pos, float radius) const override;
    bool isIce(float x, float z) const override;
    bool isWater(float x, float z) const override;
    bool waterFlow(float x, float z, glm::vec2& flow, float& heading) const override;
    bool isFree(float x, float z, float r, bool ignoreTrees = false) const override;
    std::string zoneAt(float x, float z) const override;
    void setTimeOfDay(const std::string& name) override;
    void update(float t, float dt, glm::vec3 focus) override;

    // which area a global point belongs to
    std::string areaAt(float x, float z) const;
    Area area(const std::string& name) const;
    // global <-> area-local helpers (y and yaw included)
    glm::vec3 toLocal(const std::string& area, glm::vec3 p) const;
    glm::vec3 toGlobal(const std::string& area, glm::vec3 p) const;
    float yawToLocal(const std::string& area, float yaw) const;
    float yawToGlobal(const std::string& area, float yaw) const;
    // 0 = snow village weather, 1 = citadel weather (blended along the valley)
    float southness(float z) const;

    // boat jetties (global): spring pond near the village, harbour near the citadel
    glm::vec3 springDock, harbourDock;
    glm::vec2 springBoat;
    float springBoatHeading;
    MapData wildsMap;
    // berry bushes along the valley (global coordinates; picked like the towns' bushes)
    std::vector<glm::vec2> berrySpots;

private:
    float wildsGround(float x, float z, float y) const;
    void wildsCollide(glm::vec3& pos, float radius) const;
    void buildTerrain();
    void buildRoad();
    void buildRiver();
    void buildForest();
    void buildCamp();
    void buildBridge();
    void placeBerries();
    // walking height of the bridge deck at x (only meaningful inside the bridge's span)
    float bridgeDeck(float x) const;

    WinterWorld& winter_;
    MedievalWorld& medieval_;
    Rng rand_{908};
    std::vector<ObjPtr> lampGlows_;
};
