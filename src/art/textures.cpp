// Port of TEX in patty-project/xmas-app/web/js/pixelart.js.
// Same seeds and the same order of rng() calls, so every texture comes out pixel-identical.
#include "art/textures.h"
#include <cstdlib>
#include <map>
#include "gfx/texture.h"

namespace tex {

namespace {
// (r() * n) | 0
int ri(Rng& r, int n) { return (int)(r() * n); }

// px(g, (r() * w) | 0, (r() * h) | 0, color) — JS evaluates x, y, then the colour expression
template <class F>
void sprinkle(Canvas& g, Rng& r, int count, F color) {
    for (int i = 0; i < count; ++i) {
        int x = ri(r, g.w), y = ri(r, g.h);
        g.px(x, y, color());
    }
}
}  // namespace

Canvas snow() {
    Canvas g(16, 16);
    Rng r(7);
    g.fill(0, 0, 16, 16, 0xe4e8f6);
    sprinkle(g, r, 40, [&] { return r() < 0.5 ? 0xd3d8eeu : 0xc8cde6u; });
    sprinkle(g, r, 6, [] { return 0xffffffu; });
    sprinkle(g, r, 2, [] { return 0xb9c6f2u; });
    return g;
}

Canvas path() {
    Canvas g(16, 16);
    Rng r(11);
    g.fill(0, 0, 16, 16, 0xb9a99a);
    const uint32_t cols[4] = {0xa8968a, 0xc9bbae, 0x9c8a7e, 0xd8d2d0};
    sprinkle(g, r, 60, [&] { return cols[ri(r, 4)]; });
    sprinkle(g, r, 18, [] { return 0xeef0f8u; });
    return g;
}

Canvas cobble() {
    Canvas g(16, 16);
    Rng r(23);
    const int w = 16;
    g.fill(0, 0, 16, 16, 0x5d5866);
    // stones laid in staggered rows
    const uint32_t cols[4] = {0x8d8796, 0x9a94a2, 0x857f8e, 0xa49eab};
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 3; ++col) {
            int x = col * 6 + (row % 2 ? 3 : 0), y = row * 4;
            uint32_t base = cols[ri(r, 4)];
            g.fill(x % w, y, 5, 3, base);
            g.fill((x + 5 - w) % w, y, x + 5 > w ? x + 5 - w : 0, 3, base);
            g.px(x % w, y, 0xb5afbc);
            g.px((x + 4) % w, y + 2, 0x6c6676);
        }
    sprinkle(g, r, 14, [] { return 0xe8ebf6u; });
    return g;
}

Canvas plaster() {
    // cream plaster wall with a dark timber frame (European cottage)
    Canvas g(16, 16);
    Rng r(31);
    g.fill(0, 0, 16, 16, 0xe9dcc4);
    sprinkle(g, r, 24, [] { return 0xddcdb2u; });
    g.fill(0, 0, 16, 2, 0x4a3226);
    g.fill(0, 0, 2, 16, 0x4a3226);
    g.fill(0, 8, 16, 1, 0x4a3226);
    for (int i = 0; i < 8; ++i) {
        g.px(2 + i, 9 + (i >> 1), 0x4a3226);
        g.px(3 + i, 9 + (i >> 1), 0x4a3226);
    }
    return g;
}

Canvas planks() {
    Canvas g(16, 16);
    Rng r(41);
    const uint32_t cols[3] = {0x7a4a35, 0x6e412e, 0x84523b};
    for (int y = 0; y < 16; y += 4) {
        g.fill(0, y, 16, 4, cols[(y / 4) % 3]);
        g.fill(0, y + 3, 16, 1, 0x4e2e22);
        g.px(ri(r, 16), y + 1, 0x94634a);
        g.px((y * 3) % 16, y, 0x4e2e22);
    }
    return g;
}

Canvas roof(uint32_t color) {
    Canvas g(16, 16);
    Rng r(53);
    g.fill(0, 0, 16, 16, shade(color, 0.6));
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col) {
            int x = col * 4 + (row % 2 ? 2 : 0), y = row * 4;
            uint32_t c = shade(color, 0.85 + r() * 0.3);
            g.fill(x % 16, y, 3, 3, c);
            if (x + 3 > 16) g.fill(0, y, x + 3 - 16, 3, c);
            g.px(x % 16, y, shade(color, 1.25));
        }
    return g;
}

Canvas roofSnow() {
    Canvas g(16, 16);
    Rng r(59);
    g.fill(0, 0, 16, 16, 0xf1f3fb);
    sprinkle(g, r, 20, [] { return 0xd9def0u; });
    for (int x = 0; x < 16; ++x)
        if (r() < 0.5) g.px(x, 15, 0xc3c9e3);
    return g;
}

Canvas stone() {
    Canvas g(16, 16);
    Rng r(61);
    g.fill(0, 0, 16, 16, 0x6f6d80);
    const uint32_t cols[3] = {0x7f7d90, 0x5f5d70, 0x8a889a};
    sprinkle(g, r, 50, [&] { return cols[ri(r, 3)]; });
    for (int y = 3; y < 16; y += 5)
        for (int x = 0; x < 16; ++x)
            if (r() < 0.7) g.px(x, y, 0x525064);
    return g;
}

Canvas crate() {
    Canvas g(16, 16);
    g.fill(0, 0, 16, 16, 0xa8714a);
    for (int y = 2; y < 16; y += 4) g.fill(1, y, 14, 2, 0xc48a5c);
    g.fill(0, 0, 16, 1, 0x6b4128);
    g.fill(0, 15, 16, 1, 0x6b4128);
    g.fill(0, 0, 1, 16, 0x6b4128);
    g.fill(15, 0, 1, 16, 0x6b4128);
    for (int i = 1; i < 15; ++i) g.px(i, i, 0x6b4128);
    return g;
}

Canvas ice() {
    Canvas g(32, 32);
    Rng r(71);
    g.fill(0, 0, 32, 32, 0x8fd0f2);
    const uint32_t cols[3] = {0xa8dcf6, 0x7fc2ea, 0xb9e6fb};
    sprinkle(g, r, 80, [&] { return cols[ri(r, 3)]; });
    for (int i = 0; i < 5; ++i) {
        int x = ri(r, 32), y = ri(r, 32);
        for (int k = 0; k < 7; ++k) {
            g.px(x, y, 0xe6f6ff);
            x = (x + 1) % 32;
            if (r() < 0.4) y = (y + 1) % 32;
        }
    }
    return g;
}

Canvas needles() {
    Canvas g(16, 16);
    Rng r(89);
    g.fill(0, 0, 16, 16, 0x1f5a46);
    const uint32_t cols[4] = {0x2a7056, 0x174a3a, 0x2f7d60, 0x14412f};
    sprinkle(g, r, 90, [&] { return cols[ri(r, 4)]; });
    sprinkle(g, r, 6, [] { return 0xdfe6f6u; });
    return g;
}

Canvas bark() {
    Canvas g(16, 16);
    g.fill(0, 0, 16, 16, 0x5a3a2a);
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; x += 3) g.px(x + (y % 2), y, 0x46291d);
    return g;
}

Canvas window() {
    Canvas g(8, 8);
    g.fill(0, 0, 8, 8, 0x3a2618);
    g.fill(1, 1, 3, 3, 0xffd98a);
    g.fill(4, 1, 3, 3, 0xffd98a);
    g.fill(1, 4, 3, 3, 0xffd98a);
    g.fill(4, 4, 3, 3, 0xffd98a);
    g.px(1, 1, 0xfff2c4);
    g.px(4, 1, 0xfff2c4);
    g.fill(1, 6, 6, 1, 0xf2b45c);
    return g;
}

Canvas door() {
    Canvas g(8, 16);
    g.fill(0, 0, 8, 16, 0x5a3524);
    g.fill(1, 1, 3, 14, 0x6d4430);
    g.fill(4, 1, 3, 14, 0x6d4430);
    g.fill(0, 5, 8, 1, 0x3a2016);
    g.fill(0, 11, 8, 1, 0x3a2016);
    g.px(6, 8, 0xe9c98b);
    return g;
}

// ---------- medieval textures ----------

Canvas grass() {
    Canvas g(16, 16);
    Rng r(97);
    g.fill(0, 0, 16, 16, 0x4c8c4a);
    const uint32_t cols[4] = {0x5aa358, 0x427d40, 0x63b360, 0x3b7039};
    sprinkle(g, r, 70, [&] { return cols[ri(r, 4)]; });
    // tiny white / yellow flowers
    sprinkle(g, r, 4, [&] { return r() < 0.5 ? 0xfff4a3u : 0xffffffu; });
    return g;
}

Canvas dirt() {
    Canvas g(16, 16);
    Rng r(103);
    g.fill(0, 0, 16, 16, 0x8a6e4b);
    const uint32_t cols[4] = {0x785e3e, 0x9c7e57, 0x6c5335, 0xb08f65};
    sprinkle(g, r, 60, [&] { return cols[ri(r, 4)]; });
    sprinkle(g, r, 8, [] { return 0x544028u; });
    return g;
}

Canvas castleStone() {
    Canvas g(32, 32);
    Rng r(107);
    const int w = 32;
    g.fill(0, 0, 32, 32, 0x68667a);
    // large castle blocks, staggered rows
    const uint32_t cols[4] = {0x7c7a90, 0x88869c, 0x727084, 0x626074};
    for (int row = 0; row < 4; ++row) {
        int y = row * 8, shift = (row % 2) * 8;
        for (int col = 0; col < 3; ++col) {
            int x = (col * 16 + shift) % w;
            uint32_t base = cols[ri(r, 4)];
            g.fill(x, y + 1, 15, 6, base);
            if (x + 15 > w) g.fill(0, y + 1, (x + 15) % w, 6, base);
            // bevelled edge highlights
            g.px(x, y + 1, 0xa6a4ba);
            g.px((x + 14) % w, y + 6, 0x464456);
        }
        // mortar lines
        g.fill(0, y, w, 1, 0x3e3c4e);
        g.fill(0, y + 7, w, 1, 0x3e3c4e);
    }
    return g;
}

Canvas water() {
    Canvas g(32, 32);
    Rng r(109);
    g.fill(0, 0, 32, 32, 0x2d6596);
    const uint32_t cols[3] = {0x3877ae, 0x245580, 0x4a8bc4};
    sprinkle(g, r, 80, [&] { return cols[ri(r, 3)]; });
    for (int i = 0; i < 10; ++i) {
        int x = ri(r, 32), y = ri(r, 32);
        for (int k = 0; k < 5; ++k) {
            g.px(x, y, 0x9fd6ff);
            x = (x + 1) % 32;
        }
    }
    return g;
}

Canvas roofSlate() {
    Canvas g(16, 16);
    Rng r(113);
    g.fill(0, 0, 16, 16, 0x393c4e);
    const uint32_t cols[3] = {0x44485e, 0x343748, 0x4f536e};
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col) {
            int x = col * 4 + (row % 2 ? 2 : 0), y = row * 4;
            uint32_t c = cols[ri(r, 3)];
            g.fill(x % 16, y, 3, 3, c);
            if (x + 3 > 16) g.fill(0, y, x + 3 - 16, 3, c);
            g.px(x % 16, y, 0x626888);
        }
    return g;
}

Canvas banner(uint32_t bg, uint32_t crest) {
    Canvas g(16, 32);
    g.fill(0, 0, 16, 32, bg);
    // gold border
    g.fill(0, 0, 16, 2, crest);
    g.fill(0, 0, 1, 30, crest);
    g.fill(15, 0, 1, 30, crest);
    // swallow-tail cut at the bottom
    g.clear(0, 28, 4, 4);
    g.clear(12, 28, 4, 4);
    // geometric crown / lion crest
    g.fill(5, 8, 6, 2, crest);
    g.fill(4, 10, 8, 3, crest);
    g.fill(6, 13, 4, 4, crest);
    g.fill(7, 17, 2, 3, crest);
    g.px(4, 7, crest);
    g.px(7, 6, crest);
    g.px(11, 7, crest);
    return g;
}

Canvas awning(uint32_t c1, uint32_t c2) {
    Canvas g(16, 16);
    for (int i = 0; i < 4; ++i) g.fill(i * 4, 0, 4, 16, i % 2 ? c1 : c2);
    g.blend(0, 14, 16, 2, 0x000000, 0.15f);
    return g;
}

Canvas barrel() {
    Canvas g(16, 16);
    g.fill(0, 0, 16, 16, 0x6b4128);
    for (int x = 0; x < 16; x += 4) {
        g.fill(x + 1, 0, 2, 16, 0x825032);
        g.fill(x, 0, 1, 16, 0x4c2d1b);
    }
    // two iron hoops
    g.fill(0, 3, 16, 2, 0x424552);
    g.fill(0, 11, 16, 2, 0x424552);
    g.px(4, 3, 0x7a7e94);
    g.px(10, 3, 0x7a7e94);
    g.px(4, 11, 0x7a7e94);
    g.px(10, 11, 0x7a7e94);
    return g;
}

Canvas target() {
    Canvas g(24, 24);
    const uint32_t rings[5] = {0xf6eedf, 0x32323a, 0x3b70b3, 0xa82c35, 0xe9c98b};
    for (int i = 0; i < 5; ++i) g.circle(12, 12, 11 - i * 2.2f, rings[i]);
    return g;
}

Canvas ironGrate() {
    Canvas g(16, 16);
    for (int x = 2; x < 16; x += 4) g.fill(x, 0, 2, 16, 0x2b2d38);
    for (int y = 3; y < 16; y += 6) g.fill(0, y, 16, 2, 0x2b2d38);
    for (int x = 2; x < 16; x += 4) {
        g.px(x, 3, 0x8d92a8);
        g.px(x, 9, 0x8d92a8);
        g.px(x, 15, 0x8d92a8);
    }
    return g;
}

Canvas carpet() {
    Canvas g(16, 16);
    g.fill(0, 0, 16, 16, 0x8a1f26);
    for (int y = 2; y < 14; ++y)
        for (int x = 2; x < 14; x += 2) g.px(x + (y % 2), y, 0xb32d36);
    // gold trim
    g.fill(0, 0, 16, 1, 0xe9c98b);
    g.fill(0, 15, 16, 1, 0xe9c98b);
    return g;
}

std::shared_ptr<Texture> get(const std::string& key) {
    static std::map<std::string, std::shared_ptr<Texture>> cache;
    auto& t = cache[key];
    if (t) return t;
    // keys with arguments look like "roof#a8423f" or "banner#a82c35#e9c98b"
    std::string name = key.substr(0, key.find('#'));
    uint32_t a1 = 0, a2 = 0;
    size_t p = key.find('#');
    if (p != std::string::npos) {
        a1 = (uint32_t)std::strtoul(key.c_str() + p + 1, nullptr, 16);
        size_t q = key.find('#', p + 1);
        if (q != std::string::npos) a2 = (uint32_t)std::strtoul(key.c_str() + q + 1, nullptr, 16);
    }
    bool repeat = true;
    Canvas c(1, 1);
    if (name == "snow") c = snow();
    else if (name == "path") c = path();
    else if (name == "cobble") c = cobble();
    else if (name == "plaster") c = plaster();
    else if (name == "planks") c = planks();
    else if (name == "roof") c = roof(p == std::string::npos ? 0xa8423f : a1);
    else if (name == "roofSnow") c = roofSnow();
    else if (name == "stone") c = stone();
    else if (name == "crate") { c = crate(); repeat = false; }
    else if (name == "ice") c = ice();
    else if (name == "needles") c = needles();
    else if (name == "bark") c = bark();
    else if (name == "window") { c = window(); repeat = false; }
    else if (name == "door") { c = door(); repeat = false; }
    else if (name == "grass") c = grass();
    else if (name == "dirt") c = dirt();
    else if (name == "castleStone") c = castleStone();
    else if (name == "water") c = water();
    else if (name == "roofSlate") c = roofSlate();
    else if (name == "banner") { c = p == std::string::npos ? banner() : banner(a1, a2); repeat = false; }
    else if (name == "awning") c = p == std::string::npos ? awning() : awning(a1, a2);
    else if (name == "barrel") c = barrel();
    else if (name == "target") { c = target(); repeat = false; }
    else if (name == "ironGrate") c = ironGrate();
    else if (name == "carpet") c = carpet();
    else c.fill(0, 0, 1, 1, 0xffffff);
    t = std::make_shared<Texture>();
    TextureOptions o;
    o.repeat = repeat;
    t->upload(c.w, c.h, c.rgba, o);
    return t;
}

}  // namespace tex
