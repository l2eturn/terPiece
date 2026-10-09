#pragma once
// Pixel-art character sprites — port of buildSheet() / createSpriteActor() in pixelart.js.
#include <map>
#include <memory>
#include <string>
#include "art/canvas.h"
#include "scene/object.h"

class Texture;

// All frames side by side, equal size (largest frame; smaller ones centred and bottom-aligned).
// Includes the generated walk frames (1/3), mirrored sides (sideL...) and glide poses (...G).
struct SpriteSheet {
    Canvas image{1, 1};
    int frameW = 0, frameH = 0, count = 0;
    std::map<std::string, int> index;
    std::shared_ptr<Texture> texture;
    int frame(const std::string& name) const {
        auto it = index.find(name);
        return it == index.end() ? 0 : it->second;
    }
};

// kind = patty / ter / momonga / cham / juice / puppy / slid / snowman
const SpriteSheet& spriteSheet(const std::string& kind);
bool isPlayable(const std::string& kind);

// SpriteView.cameraYaw: the game updates this every frame so sprites turn to face the camera
extern float g_spriteCameraYaw;

// A sprite character standing in the 3D scene: always faces the camera, lit, casts a shadow.
// ppm = metres per pixel.
class SpriteActor {
public:
    SpriteActor(const std::string& kind, float ppm = 0.05f);

    ObjPtr object;
    ObjPtr plane;

    void setFrame(const std::string& name);
    // front / back / side / sideL relative to the camera
    std::string viewDir(float worldYaw) const;
    // keep the plane facing the camera even when parents rotate
    void billboard();
    float facingYaw();

private:
    float parentYaw();
    const SpriteSheet* sheet_;
    int current_ = -1;
};
