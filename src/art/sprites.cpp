// Port of buildSheet() / walkFrame() / glideFrame() / createSpriteActor() in pixelart.js
#include "art/sprites.h"
#include "gfx/texture.h"
#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>

namespace {

struct PaletteEntry { char key; uint32_t rgb; };
struct FrameDef { const char* name; const char* const* rows; };
struct CharDef { const char* name; const FrameDef* frames; };
#include "art/sprite_data.inc"

using Rows = std::vector<std::string>;

Rows toRows(const char* const* r) {
    Rows out;
    for (; *r; ++r) out.emplace_back(*r);
    return out;
}

// Walk frame: shift the two leg rows and bob the body by one pixel
Rows walkFrame(Rows rows, int step) {
    size_t n = rows.size();
    std::string legs = rows[n - 2];
    if (step == 1) rows[n - 2] = legs.substr(1) + ".";
    else if (step == 3) rows[n - 2] = "." + legs.substr(0, legs.size() - 1);
    if (step == 1 || step == 3) {
        rows.insert(rows.begin(), std::string(rows[0].size(), '.'));
        rows.pop_back();
    }
    return rows;
}

// Glide frame: short arms raised beside the ears holding the glider bar (widened by 2 columns per side)
struct GlideCfg { int shoulder, hand; char sleeve; int sleeveLen; std::vector<int> handRows; char fill; };
const GlideCfg* glideCfg(const std::string& kind) {
    static const GlideCfg patty{16, 7, 'J', 99, {16, 17, 18, 19, 20}, 'J'};
    static const GlideCfg ter{16, 7, 'a', 3, {18}, 'a'};
    if (kind == "patty") return &patty;
    if (kind == "ter") return &ter;
    return nullptr;
}

Rows glideFrame(const Rows& rows, const std::string& view, const GlideCfg& c) {
    const int pad = 2;
    int w = (int)rows[0].size() + pad * 2;
    Rows out;
    for (auto& r : rows) out.push_back(std::string(pad, '.') + r + std::string(pad, '.'));
    for (int y : c.handRows)
        for (char& ch : out[y]) if (ch == 'S') ch = c.fill;
    auto arm = [&](int x0, int outer) {
        out[c.hand - 1][x0] = out[c.hand - 1][x0 + 1] = 'K';
        for (int y = c.hand; y <= c.shoulder; ++y) {
            char ch = (y < c.hand + 2 || c.shoulder - y >= c.sleeveLen) ? 'S' : c.sleeve;
            out[y][x0] = out[y][x0 + 1] = ch;
            out[y][outer] = 'K';
        }
    };
    if (view == "side") arm(pad + 13, pad + 15);
    else { arm(1, 0); arm(w - 3, w - 1); }
    return out;
}

uint32_t paletteColor(char k, bool& ok) {
    for (auto& p : kPalette) if (p.key == k) { ok = true; return p.rgb; }
    ok = false;
    return 0;
}

std::unique_ptr<SpriteSheet> build(const std::string& kind) {
    const FrameDef* def = nullptr;
    for (const CharDef* c = kCharDefs; c->name; ++c) if (kind == c->name) def = c->frames;
    auto sheet = std::make_unique<SpriteSheet>();
    if (!def) return sheet;

    struct F { std::string name; Rows rows; bool flip; };
    std::vector<F> frames;
    bool playable = isPlayable(kind);
    for (const FrameDef* f = def; f->name; ++f) {
        std::string name = f->name;
        Rows rows = toRows(f->rows);
        frames.push_back({name, rows, false});
        if (name == "side") frames.push_back({"sideL", rows, true});
        if (name == "sideG") frames.push_back({"sideLG", rows, true});
        if ((name == "front" || name == "back" || name == "side") && playable) {
            for (int st : {1, 3}) {
                frames.push_back({name + std::to_string(st), walkFrame(rows, st), false});
                if (name == "side") frames.push_back({"sideL" + std::to_string(st), walkFrame(rows, st), true});
            }
            if (const GlideCfg* g = glideCfg(kind)) {
                frames.push_back({name + "G", glideFrame(rows, name, *g), false});
                if (name == "side") frames.push_back({"sideLG", glideFrame(rows, name, *g), true});
            }
        }
        if ((kind == "puppy" || kind == "slid") && name == "side") {
            frames.push_back({"side1", walkFrame(rows, 1), false});
            frames.push_back({"sideL1", walkFrame(rows, 1), true});
        }
    }
    int fw = 0, fh = 0;
    for (auto& f : frames) { fw = std::max(fw, (int)f.rows[0].size()); fh = std::max(fh, (int)f.rows.size()); }
    sheet->frameW = fw;
    sheet->frameH = fh;
    sheet->count = (int)frames.size();
    sheet->image = Canvas(fw * sheet->count, fh);
    for (int i = 0; i < (int)frames.size(); ++i) {
        auto& f = frames[i];
        // duplicate names keep the first frame
        sheet->index.emplace(f.name, i);
        int rw = (int)f.rows[0].size();
        int dx = i * fw + (fw - rw) / 2, dy = fh - (int)f.rows.size();
        for (int y = 0; y < (int)f.rows.size(); ++y)
            for (int x = 0; x < rw; ++x) {
                char ch = f.rows[y][f.flip ? rw - 1 - x : x];
                bool ok = false;
                uint32_t c = ch == '.' ? 0 : paletteColor(ch, ok);
                if (ok) sheet->image.px(dx + x, dy + y, c);
            }
    }
    sheet->texture = std::make_shared<Texture>();
    TextureOptions o;
    o.repeat = false;
    o.mipmaps = false;
    sheet->texture->upload(sheet->image.w, sheet->image.h, sheet->image.rgba, o);
    return sheet;
}

}  // namespace

bool isPlayable(const std::string& kind) { return kind == "patty" || kind == "ter" || kind == "momonga"; }

const SpriteSheet& spriteSheet(const std::string& kind) {
    static std::map<std::string, std::unique_ptr<SpriteSheet>> cache;
    auto& s = cache[kind];
    if (!s) s = build(kind);
    return *s;
}

float g_spriteCameraYaw = 0;

SpriteActor::SpriteActor(const std::string& kind, float ppm) : sheet_(&spriteSheet(kind)) {
    auto mat = lambert(sheet_->texture);
    mat->alphaTest = 0.5f;
    mat->side = Side::Double;
    mat->emissive = hexc(0x4a4466);
    mat->emissiveMap = sheet_->texture;
    mat->mapRepeat = {1.0f / sheet_->count, 1};
    float w = sheet_->frameW * ppm, h = sheet_->frameH * ppm;
    auto g = geo::plane(w, h);
    g->translate(0, h / 2, 0);
    plane = makeMesh(g, mat);
    plane->castShadow = true;
    object = makeGroup();
    object->add(plane);
    setFrame(sheet_->index.empty() ? "" : sheet_->index.begin()->first);
    // JS starts on the first frame in insertion order, which is "front" for every character
    setFrame("front");
}

void SpriteActor::setFrame(const std::string& name) {
    int i = sheet_->frame(name);
    if (i == current_) return;
    current_ = i;
    plane->material()->mapOffset.x = (float)i / sheet_->count;
}

std::string SpriteActor::viewDir(float worldYaw) const {
    float d = wrapAngle(worldYaw - g_spriteCameraYaw), a = std::abs(d);
    if (a < PI / 4) return "front";
    if (a > PI * 3 / 4) return "back";
    return d > 0 ? "side" : "sideL";
}

float SpriteActor::parentYaw() {
    if (!object->parent) return 0;
    object->parent->updateWorldFromParents();
    return object->parent->worldYaw();
}

void SpriteActor::billboard() { plane->rotation.y = g_spriteCameraYaw - parentYaw() - object->rotation.y; }

float SpriteActor::facingYaw() { return parentYaw() + object->rotation.y; }
