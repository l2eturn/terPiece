#pragma once
// Soft glow sprite (glowTexture / glowSprite in effects.js) used for lamps, bulbs, portals, crystals, snow...
#include <memory>
#include <glm/glm.hpp>
#include "scene/object.h"

class Texture;

std::shared_ptr<Texture> glowTexture();
// Additive sprite, `size` metres wide, depthWrite off
ObjPtr glowSprite(glm::vec3 color, float size, float opacity = 1);
