#pragma once
// Pixel-art scene textures (16 px = 1 metre) — port of TEX in pixelart.js.
// Each function paints a Canvas; tex::get() caches the uploaded GPU texture by name.
#include <memory>
#include <string>
#include "art/canvas.h"

class Texture;

namespace tex {

Canvas snow();
Canvas path();
Canvas cobble();
Canvas plaster();
Canvas planks();
Canvas roof(uint32_t color);
Canvas roofSnow();
Canvas stone();
Canvas crate();
Canvas ice();
Canvas needles();
Canvas bark();
Canvas window();
Canvas door();
// medieval
Canvas grass();
Canvas dirt();
Canvas castleStone();
Canvas water();
Canvas roofSlate();
Canvas banner(uint32_t bg = 0xa82c35, uint32_t crest = 0xe9c98b);
Canvas awning(uint32_t c1 = 0xa82c35, uint32_t c2 = 0xf6eedf);
Canvas barrel();
Canvas target();
Canvas ironGrate();
Canvas carpet();

// Cached GPU textures, keyed like the JS once() cache ("snow", "roof" + color, ...).
// All are NEAREST + sRGB like canvasTex(); repeat follows the JS option.
std::shared_ptr<Texture> get(const std::string& key);

}  // namespace tex
