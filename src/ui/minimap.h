#pragma once
// Bottom-right minimap for both realms — port of minimap.js
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include "ui/canvas2d.h"

struct MapData;
class Texture;

class Minimap {
public:
    explicit Minimap(int size = 300);
    void setMapData(const MapData& data);
    // targets = companions still lost (blinking dots)
    void draw(glm::vec3 player, float yaw, float camYaw, bool inCave, const std::vector<glm::vec3>& targets, float time);
    Texture& texture() { return *tex_; }

private:
    void drawBackground();
    glm::vec2 toPx(float x, float z) const;
    const MapData* data_ = nullptr;
    float R_ = 84;
    int W_;
    Canvas2D bg_, fg_;
    std::shared_ptr<Texture> tex_;
};
