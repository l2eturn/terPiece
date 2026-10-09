#pragma once
// CPU-side RGBA canvas used to paint the pixel-art textures, mirroring the 2D canvas calls in
// pixelart.js (fillRect / px / clearRect / arc fill / translucent overlays).
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include "core/math.h"

struct Canvas {
    int w = 0, h = 0;
    std::vector<uint8_t> rgba;

    Canvas(int w_, int h_) : w(w_), h(h_), rgba((size_t)w_ * h_ * 4, 0) {}

    // fillRect with an opaque colour (alpha replaces, like fillStyle with no transparency)
    void fill(int x, int y, int fw, int fh, uint32_t rgb, uint8_t a = 255) {
        for (int j = y; j < y + fh; ++j)
            for (int i = x; i < x + fw; ++i) {
                if (i < 0 || j < 0 || i >= w || j >= h) continue;
                uint8_t* p = &rgba[((size_t)j * w + i) * 4];
                p[0] = rgb >> 16 & 255;
                p[1] = rgb >> 8 & 255;
                p[2] = rgb & 255;
                p[3] = a;
            }
    }

    void px(int x, int y, uint32_t rgb) { fill(x, y, 1, 1, rgb); }

    void clear(int x, int y, int fw, int fh) { fill(x, y, fw, fh, 0, 0); }

    // Source-over blend of one pixel (canvas compositing with a translucent fillStyle)
    void blendPx(int i, int j, uint32_t rgb, float alpha) {
        if (i < 0 || j < 0 || i >= w || j >= h || alpha <= 0) return;
        uint8_t* p = &rgba[((size_t)j * w + i) * 4];
        float da = p[3] / 255.f, oa = alpha + da * (1 - alpha);
        if (oa <= 0) return;
        const int sh[3] = {16, 8, 0};
        for (int k = 0; k < 3; ++k) {
            float s = (rgb >> sh[k] & 255), d = p[k];
            p[k] = (uint8_t)std::lround((s * alpha + d * da * (1 - alpha)) / oa);
        }
        p[3] = (uint8_t)std::lround(oa * 255);
    }

    void blend(int x, int y, int fw, int fh, uint32_t rgb, float alpha) {
        for (int j = y; j < y + fh; ++j)
            for (int i = x; i < x + fw; ++i) blendPx(i, j, rgb, alpha);
    }

    // arc(cx, cy, r, 0, 2PI) + fill(), anti-aliased like the browser (4x4 coverage samples)
    void circle(float cx, float cy, float r, uint32_t rgb) {
        for (int j = 0; j < h; ++j)
            for (int i = 0; i < w; ++i) {
                int hit = 0;
                for (int sy = 0; sy < 4; ++sy)
                    for (int sx = 0; sx < 4; ++sx) {
                        float x = i + (sx + 0.5f) / 4, y = j + (sy + 0.5f) / 4;
                        if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r) ++hit;
                    }
                if (hit) blendPx(i, j, rgb, hit / 16.f);
            }
    }
};

// THREE.Color(c).multiplyScalar(k).getHexString(): same double maths, truncated by the bit shift
inline uint32_t shade(uint32_t c, double k) {
    auto ch = [&](int sh) {
        double v = ((c >> sh) & 255) / 255.0 * k * 255.0;
        return (uint32_t)(v < 0 ? 0 : v > 255 ? 255 : v);
    };
    return ch(16) << 16 | ch(8) << 8 | ch(0);
}
