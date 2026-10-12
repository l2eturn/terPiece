// Block terrain — see terrain.h.
#include "world/terrain.h"
#include <array>
#include <cmath>
#include <vector>
#include "art/textures.h"
#include "scene/geometry.h"
#include "scene/material.h"
#include "world/layout.h"

namespace {

using layout::BLOCK;

// no ground here
constexpr int VOID_LEVEL = -32768;
// where the walls at the edge of the world stop
constexpr int BASE_LEVEL = -10;
constexpr int GROUND_KINDS = 5;

const char* textureOf(Ground g) {
    switch (g) {
    case Ground::Snow: return "snow";
    case Ground::Grass: return "grass";
    case Ground::Cobble: return "cobble";
    case Ground::Dirt: return "dirt";
    default: return "stone";
    }
}

// what shows on the side of a column below its top block
Ground underOf(Ground g) {
    switch (g) {
    case Ground::Grass:
    case Ground::Dirt: return Ground::Dirt;
    default: return Ground::Stone;
    }
}

// one mesh per ground texture
struct Builder {
    std::array<GeoPtr, GROUND_KINDS> geo;
    Builder() {
        for (auto& g : geo) g = std::make_shared<Geometry>();
    }
    // quad a-b-c-d (any winding: it is turned to face `n`), uv per corner
    void quad(Ground kind, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 n, glm::vec2 ua, glm::vec2 ub, glm::vec2 uc, glm::vec2 ud,
              glm::vec3 tint) {
        Geometry& g = *geo[(int)kind];
        uint32_t base = (uint32_t)g.position.size();
        g.position.insert(g.position.end(), {a, b, c, d});
        g.uv.insert(g.uv.end(), {ua, ub, uc, ud});
        for (int i = 0; i < 4; ++i) {
            g.normal.push_back(n);
            g.color.push_back(tint);
        }
        bool flip = glm::dot(glm::cross(b - a, c - a), n) < 0;
        if (flip) g.index.insert(g.index.end(), {base, base + 2, base + 1, base, base + 3, base + 2});
        else g.index.insert(g.index.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
};

}  // namespace

ObjPtr buildBlockTerrain(const TerrainSource& src) {
    int i0 = (int)std::floor(src.bounds.x / BLOCK), j0 = (int)std::floor(src.bounds.y / BLOCK);
    int i1 = (int)std::ceil(src.bounds.z / BLOCK), j1 = (int)std::ceil(src.bounds.w / BLOCK);
    int W = i1 - i0, D = j1 - j0;
    auto cx = [&](int i) { return (i0 + i + 0.5f) * BLOCK; };
    auto cz = [&](int j) { return (j0 + j + 0.5f) * BLOCK; };

    // ---------- sample the columns ----------
    std::vector<int> level((size_t)W * D, VOID_LEVEL);
    // index into a small palette of (kind, tint) so equal tops can be merged
    std::vector<uint8_t> key((size_t)W * D, 0);
    std::vector<Surface> palette;
    auto keyOf = [&](const Surface& s) {
        for (size_t k = 0; k < palette.size(); ++k)
            if (palette[k].kind == s.kind && glm::length(palette[k].tint - s.tint) < 1e-3f) return (uint8_t)k;
        palette.push_back(s);
        return (uint8_t)(palette.size() - 1);
    };
    auto at = [&](int i, int j) { return (size_t)j * W + i; };
    std::vector<Surface> surf((size_t)W * D);
    for (int j = 0; j < D; ++j)
        for (int i = 0; i < W; ++i) {
            float x = cx(i), z = cz(j);
            if (!src.inside(x, z)) continue;
            level[at(i, j)] = (int)std::lround(src.height(x, z) / BLOCK);
            surf[at(i, j)] = src.surface(x, z);
        }
    auto lv = [&](int i, int j) { return i < 0 || j < 0 || i >= W || j >= D ? VOID_LEVEL : level[at(i, j)]; };
    const int di[4] = {1, -1, 0, 0}, dj[4] = {0, 0, 1, -1};
    for (int j = 0; j < D; ++j)
        for (int i = 0; i < W; ++i) {
            int l = level[at(i, j)];
            if (l == VOID_LEVEL) continue;
            Surface s = surf[at(i, j)];
            // under water: a sandy bed
            if (l * BLOCK < layout::WATER_LEVEL - 0.01f) s = {Ground::Dirt, hexc(0xd8b88a)};
            // cliffs: bare rock on top (paved ground stays paved)
            int drop = 0;
            for (int k = 0; k < 4; ++k) {
                int n = lv(i + di[k], j + dj[k]);
                if (n != VOID_LEVEL) drop = std::max(drop, std::abs(l - n));
            }
            if (drop >= 3 && s.kind != Ground::Cobble) s = {Ground::Stone, glm::vec3(1)};
            key[at(i, j)] = keyOf(s);
        }

    Builder out;
    // ---------- tops: greedy merge of equal (level, surface) rectangles ----------
    std::vector<uint8_t> done((size_t)W * D, 0);
    for (int j = 0; j < D; ++j)
        for (int i = 0; i < W; ++i) {
            size_t c = at(i, j);
            int l = level[c];
            if (l == VOID_LEVEL || done[c]) continue;
            uint8_t k = key[c];
            auto same = [&](int a, int b) {
                size_t q = at(a, b);
                return level[q] == l && key[q] == k && !done[q];
            };
            int w = 1;
            while (i + w < W && same(i + w, j)) ++w;
            int d = 1;
            for (bool grow = true; grow && j + d < D; ++d) {
                for (int a = i; a < i + w; ++a)
                    if (!same(a, j + d)) {
                        grow = false;
                        break;
                    }
                if (!grow) break;
            }
            for (int b = j; b < j + d; ++b)
                for (int a = i; a < i + w; ++a) done[at(a, b)] = 1;
            float x0 = (i0 + i) * BLOCK, x1 = x0 + w * BLOCK, z0 = (j0 + j) * BLOCK, z1 = z0 + d * BLOCK, y = l * BLOCK;
            const Surface& s = palette[k];
            out.quad(s.kind, {x0, y, z0}, {x0, y, z1}, {x1, y, z1}, {x1, y, z0}, {0, 1, 0}, {x0, z0}, {x0, z1}, {x1, z1}, {x1, z0}, s.tint);
        }

    // ---------- sides: where the neighbour is lower, merged along the edge ----------
    // the top block shows its own surface on the side, the blocks below show what lies underneath
    auto wall = [&](glm::vec3 p0, glm::vec3 p1, int lo, int hi, glm::vec3 n, const Surface& s) {
        // p0 -> p1 runs along the edge at y = 0
        float along0 = n.x != 0 ? p0.z : p0.x, along1 = n.x != 0 ? p1.z : p1.x;
        auto face = [&](int a, int b, Ground kind, glm::vec3 tint) {
            float ya = a * BLOCK, yb = b * BLOCK;
            out.quad(kind, {p0.x, ya, p0.z}, {p1.x, ya, p1.z}, {p1.x, yb, p1.z}, {p0.x, yb, p0.z}, n, {along0, ya}, {along1, ya}, {along1, yb}, {along0, yb},
                     tint);
        };
        int split = std::max(lo, hi - 1);
        face(split, hi, s.kind, s.tint);
        if (lo < split) face(lo, split, underOf(s.kind), glm::vec3(1));
    };
    auto lowerOf = [&](int i, int j, int k) {
        int n = lv(i + di[k], j + dj[k]);
        return n == VOID_LEVEL ? BASE_LEVEL : n;
    };
    for (int k = 0; k < 4; ++k) {
        bool alongZ = di[k] != 0;
        int outer = alongZ ? W : D, inner = alongZ ? D : W;
        for (int o = 0; o < outer; ++o)
            for (int p = 0; p < inner;) {
                int i = alongZ ? o : p, j = alongZ ? p : o;
                int l = level[at(i, j)];
                int n = l == VOID_LEVEL ? 0 : lowerOf(i, j, k);
                if (l == VOID_LEVEL || n >= l) {
                    ++p;
                    continue;
                }
                uint8_t key0 = key[at(i, j)];
                int run = 1;
                while (p + run < inner) {
                    int ii = alongZ ? o : p + run, jj = alongZ ? p + run : o;
                    if (level[at(ii, jj)] != l || key[at(ii, jj)] != key0 || lowerOf(ii, jj, k) != n) break;
                    ++run;
                }
                glm::vec3 nrm(di[k], 0, dj[k]);
                glm::vec3 p0, p1;
                if (alongZ) {
                    float x = (i0 + i + (di[k] > 0 ? 1 : 0)) * BLOCK;
                    p0 = {x, 0, (j0 + j) * BLOCK};
                    p1 = {x, 0, (j0 + j + run) * BLOCK};
                } else {
                    float z = (j0 + j + (dj[k] > 0 ? 1 : 0)) * BLOCK;
                    p0 = {(i0 + i) * BLOCK, 0, z};
                    p1 = {(i0 + i + run) * BLOCK, 0, z};
                }
                wall(p0, p1, n, l, nrm, palette[key0]);
                p += run;
            }
    }

    auto group = makeGroup("block-terrain");
    for (int g = 0; g < GROUND_KINDS; ++g) {
        if (out.geo[g]->position.empty()) continue;
        auto m = lambert(tex::get(textureOf((Ground)g)));
        m->vertexColors = true;
        auto mesh = makeMesh(out.geo[g], m);
        // receives shadows only: casting doubles the shadow pass cost for little gain
        mesh->receiveShadow = true;
        group->add(mesh);
    }
    return group;
}
