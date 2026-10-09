#include "art/glow.h"
#include <cmath>
#include <vector>
#include "gfx/texture.h"

std::shared_ptr<Texture> glowTexture() {
    static std::shared_ptr<Texture> tex;
    if (tex) return tex;
    // 64x64 radial gradient: alpha 1 -> 0.8 (25%) -> 0.15 (60%) -> 0 (edge), white
    const int S = 64;
    std::vector<uint8_t> px((size_t)S * S * 4);
    for (int y = 0; y < S; ++y)
        for (int x = 0; x < S; ++x) {
            float d = std::hypot(x + 0.5f - 32, y + 0.5f - 32) / 32;
            float a;
            if (d < 0.25f) a = 1 + (0.8f - 1) * d / 0.25f;
            else if (d < 0.6f) a = 0.8f + (0.15f - 0.8f) * (d - 0.25f) / 0.35f;
            else if (d < 1) a = 0.15f * (1 - (d - 0.6f) / 0.4f);
            else a = 0;
            uint8_t* p = &px[((size_t)y * S + x) * 4];
            p[0] = p[1] = p[2] = 255;
            p[3] = (uint8_t)std::lround(a * 255);
        }
    tex = std::make_shared<Texture>();
    // CanvasTexture defaults: linear encoding, linear filtering with mipmaps
    TextureOptions o;
    o.srgb = false;
    o.nearest = false;
    o.repeat = false;
    tex->upload(S, S, px, o);
    return tex;
}

ObjPtr glowSprite(glm::vec3 color, float size, float opacity) {
    auto m = spriteMaterial(glowTexture(), color, opacity);
    m->blending = Blending::Additive;
    m->depthWrite = false;
    auto s = makeSprite(m);
    s->scale = glm::vec3(size);
    return s;
}
