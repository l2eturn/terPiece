// Port of the HTML overlay (index.html + style.css). Sizes below are the CSS values in px.
#include "ui/hud.h"
#include <cmath>
#include <cstdio>
#include <sstream>
#include "core/math.h"
#include "game/save.h"
#include "gfx/texture.h"
#include "ui/canvas2d.h"
#include "ui/minimap.h"

namespace {

// :root colours
const RGBA INK = cssColor("#241a3a");
const RGBA GOLD = cssColor("#e9c98b");
const RGBA GOLD_DEEP = cssColor("#c9a15c");
const RGBA CREAM = cssColor("#f6eedf");
const RGBA PANEL = cssColor("rgba(18,14,30,0.62)");
const RGBA PANEL_LINE = cssColor("rgba(233,201,139,0.18)");
const RGBA MUTED = cssColor("rgba(246,238,223,0.62)");
const char* SERIF = "Cormorant Garamond";
const char* SANS = "Inter";
const char* THAI = "Mali";


RGBA withAlpha(RGBA c, float a) {
    c.a *= a;
    return c;
}

struct Rect {
    float x, y, w, h;
    bool contains(float px, float py) const { return px >= x && py >= y && px <= x + w && py <= y + h; }
};

struct Heart {
    std::string emoji;
    float x, size, dur, start, rot;
};

}  // namespace

struct Hud::Impl {
    std::unique_ptr<Canvas2D> canvas;
    // two textures used in turn: rewriting the one the GPU may still be drawing would stall the frame
    std::shared_ptr<Texture> texs[2];
    int texFront = 0;
    std::shared_ptr<Texture> badge;
    float badgeScale = 0;
    Shader overlay;
    GLuint vao = 0;
    std::vector<std::pair<Rect, std::string>> regions;
    Rect minimapRect{0, 0, 0, 0};
    std::string lastSig;
    float scale = 2;
    float W = 0, H = 0;
    // animations (start times; < 0 = off)
    float zoneAt = -10, berryShakeAt = -10, berryBumpAt = -10, codeShakeAt = -10, flashAt = -10, hurtAt = -10;
    std::vector<Heart> hearts;
    float promptAlpha = 0;
    std::string promptText;
    // slider being dragged ("camTrack" / "camSmooth")
    std::string dragging;
    Rect trackRect{}, smoothRect{};
    float mx = -1, my = -1;

    Canvas2D& g() { return *canvas; }
    void region(Rect r, const std::string& id) { regions.push_back({r, id}); }
    void shadow(float blur, RGBA c, float dx, float dy) { g().setShadow(blur * scale, c, dx * scale, dy * scale); }
    bool hover(Rect r) const { return r.contains(mx, my); }

    // .panel: translucent dark card with a thin gold border and a soft drop shadow
    void panel(Rect r, float radius = 12) {
        Canvas2D& c = g();
        c.save();
        shadow(40, cssColor("rgba(0,0,0,0.35)"), 0, 10);
        c.setFill(PANEL);
        c.beginPath();
        c.roundRect(r.x, r.y, r.w, r.h, radius);
        c.fill();
        c.restore();
        c.setStroke(PANEL_LINE);
        c.setLineWidth(1);
        c.beginPath();
        c.roundRect(r.x + 0.5f, r.y + 0.5f, r.w - 1, r.h - 1, radius);
        c.stroke();
    }

    float textW(const std::string& s, const char* fam, float size, int weight, float tracking = 0) {
        Canvas2D& c = g();
        c.setFont(fam, size, weight);
        c.setLetterSpacing(tracking);
        float w = c.measureText(s);
        c.setLetterSpacing(0);
        return w;
    }

    void text(const std::string& s, float x, float y, const char* fam, float size, int weight, RGBA color, TextAlign a = TextAlign::Left,
              TextBaseline b = TextBaseline::Middle, float tracking = 0) {
        Canvas2D& c = g();
        c.setFont(fam, size, weight);
        c.setFill(color);
        c.setTextAlign(a);
        c.setTextBaseline(b);
        c.setLetterSpacing(tracking);
        c.fillText(s, x, y);
        c.setLetterSpacing(0);
    }

    // .icon-btn / .pill-btn frame
    void buttonFrame(Rect r, bool hot, float alpha = 1) {
        Canvas2D& c = g();
        c.setFill(cssColor(hot ? "rgba(255,255,255,0.1)" : "rgba(255,255,255,0.04)"));
        c.beginPath();
        c.roundRect(r.x, r.y, r.w, r.h, 9);
        c.fill();
        c.setStroke(withAlpha(cssColor("rgba(246,238,223,0.16)"), alpha));
        c.setLineWidth(1);
        c.beginPath();
        c.roundRect(r.x + 0.5f, r.y + 0.5f, r.w - 1, r.h - 1, 9);
        c.stroke();
    }

    // the 24x24 stroke icons from index.html, drawn at (x, y) with size 18
    void icon(const std::string& name, float x, float y, float alpha = 1, bool waves = true) {
        Canvas2D& c = g();
        c.save();
        c.translate(x, y);
        c.scale(18.0f / 24, 18.0f / 24);
        c.setStroke(withAlpha(CREAM, alpha));
        c.setLineWidth(1.6f);
        c.setLineCap(true);
        auto line = [&](std::initializer_list<glm::vec2> pts) {
            c.beginPath();
            bool first = true;
            for (auto p : pts) {
                if (first) c.moveTo(p.x, p.y);
                else c.lineTo(p.x, p.y);
                first = false;
            }
            c.stroke();
        };
        if (name == "menu") {
            line({{4, 7}, {20, 7}});
            line({{4, 12}, {20, 12}});
            line({{4, 17}, {20, 17}});
        } else if (name == "music") {
            c.beginPath();
            c.moveTo(4, 9);
            c.lineTo(8, 9);
            c.lineTo(13, 5);
            c.lineTo(13, 19);
            c.lineTo(8, 15);
            c.lineTo(4, 15);
            c.closePath();
            c.stroke();
            if (waves) {
                c.beginPath();
                c.arc(13.354f, 12, 4, -std::atan2(3.0f, 2.646f), std::atan2(3.0f, 2.646f));
                c.stroke();
                c.beginPath();
                c.arc(13.4f, 12, 7.5f, -std::atan2(5.5f, 5.099f), std::atan2(5.5f, 5.099f));
                c.stroke();
            }
        } else if (name == "shot") {
            c.beginPath();
            c.moveTo(4, 8);
            c.lineTo(7, 8);
            c.lineTo(9, 5.5f);
            c.lineTo(15, 5.5f);
            c.lineTo(17, 8);
            c.lineTo(20, 8);
            c.lineTo(20, 19);
            c.lineTo(4, 19);
            c.closePath();
            c.stroke();
            c.beginPath();
            c.arc(12, 13, 3.5f, 0, PI * 2);
            c.stroke();
        } else if (name == "full") {
            line({{4, 9}, {4, 4}, {9, 4}});
            line({{15, 4}, {20, 4}, {20, 9}});
            line({{20, 15}, {20, 20}, {15, 20}});
            line({{9, 20}, {4, 20}, {4, 15}});
        } else if (name == "settings") {
            line({{4, 7}, {14, 7}});
            line({{18, 7}, {20, 7}});
            line({{4, 17}, {8, 17}});
            line({{12, 17}, {20, 17}});
            c.beginPath();
            c.arc(16, 7, 2, 0, PI * 2);
            c.stroke();
            c.beginPath();
            c.arc(10, 17, 2, 0, PI * 2);
            c.stroke();
        }
        c.restore();
    }

    // .seg: segmented control; returns the button rects
    void seg(Rect r, const std::vector<std::pair<std::string, std::string>>& items, const std::string& on, bool small) {
        Canvas2D& c = g();
        c.setFill(cssColor("rgba(0,0,0,0.25)"));
        c.beginPath();
        c.roundRect(r.x, r.y, r.w, r.h, 8);
        c.fill();
        float n = (float)items.size(), gap = 4, bw = (r.w - 6 - gap * (n - 1)) / n;
        for (size_t i = 0; i < items.size(); ++i) {
            Rect b{r.x + 3 + i * (bw + gap), r.y + 3, bw, r.h - 6};
            bool isOn = items[i].first == on;
            if (isOn) {
                c.setFill(cssColor("rgba(233,201,139,0.16)"));
                c.beginPath();
                c.roundRect(b.x, b.y, b.w, b.h, 6);
                c.fill();
                c.setStroke(cssColor("rgba(233,201,139,0.35)"));
                c.setLineWidth(1);
                c.beginPath();
                c.roundRect(b.x + 0.5f, b.y + 0.5f, b.w - 1, b.h - 1, 6);
                c.stroke();
            }
            text(items[i].second, b.x + b.w / 2, b.y + b.h / 2, SANS, small ? 11 : 12, 500, isOn ? CREAM : MUTED, TextAlign::Center);
            region(b, items[i].first);
        }
    }

    void checkbox(Rect r, bool on, const std::string& id) {
        Canvas2D& c = g();
        c.setFill(on ? GOLD_DEEP : cssColor("rgba(255,255,255,0.9)"));
        c.beginPath();
        c.roundRect(r.x, r.y, r.w, r.h, 3);
        c.fill();
        if (on) {
            c.setStroke(cssColor("#fff"));
            c.setLineWidth(2);
            c.setLineCap(true);
            c.beginPath();
            c.moveTo(r.x + 3.5f, r.y + 8.5f);
            c.lineTo(r.x + 6.5f, r.y + 11.5f);
            c.lineTo(r.x + 12.5f, r.y + 4.5f);
            c.stroke();
        }
        region({r.x - 4, r.y - 4, r.w + 8, r.h + 8}, id);
    }

    void slider(Rect r, float v) {
        Canvas2D& c = g();
        float cy = r.y + r.h / 2;
        c.setFill(cssColor("rgba(255,255,255,0.25)"));
        c.beginPath();
        c.roundRect(r.x, cy - 2, r.w, 4, 2);
        c.fill();
        c.setFill(GOLD_DEEP);
        c.beginPath();
        c.roundRect(r.x, cy - 2, r.w * v, 4, 2);
        c.fill();
        c.beginPath();
        c.arc(r.x + r.w * v, cy, 7, 0, PI * 2);
        c.fill();
    }

    void drawHud(const HudState& s, float t);
    void drawSettings(const HudState& s);
    void drawHearts(float t);
};

Hud::Hud() : d(std::make_unique<Impl>()) {}
Hud::~Hud() = default;

bool Hud::init() {
    registerFonts();
    if (!d->overlay.load("overlay")) return false;
    glGenVertexArrays(1, &d->vao);
    d->canvas = std::make_unique<Canvas2D>(16, 16);
    for (auto& t : d->texs) t = std::make_shared<Texture>();
    return true;
}

void Hud::zoneSwap(float t) { d->zoneAt = t; }
void Hud::shakeBerry(float t) { d->berryShakeAt = t; }
void Hud::bumpBerry(float t) { d->berryBumpAt = t; }
void Hud::shakeCode(float t) { d->codeShakeAt = t; }
void Hud::setFade(bool on) { fadeGoal_ = on ? 1.0f : 0.0f; }
void Hud::flash(float t) { d->flashAt = t; }
void Hud::hurt(float t) { d->hurtAt = t; }

void Hud::floatHearts(int n, float t) {
    static const char* emojis[6] = {"💖", "💕", "❄️", "✨", "💗", "🎄"};
    for (int i = 0; i < n; ++i)
        d->hearts.push_back({emojis[(int)(frand() * 6) % 6], frand() * 100, 18 + frand() * 22, 4 + frand() * 4, t + i * 0.18f, 0});
}

bool Hud::overUi(float x, float y) const {
    for (auto& [r, id] : d->regions)
        if (r.contains(x, y)) return true;
    for (auto& [r, id] : d->regions)
        if (id == "@panel" && r.contains(x, y)) return true;
    return false;
}

std::string Hud::click(float x, float y) {
    for (auto it = d->regions.rbegin(); it != d->regions.rend(); ++it)
        if (it->first.contains(x, y) && it->second[0] != '@') {
            if (it->second == "camTrack" || it->second == "camSmooth") d->dragging = it->second;
            return it->second;
        }
    return "";
}

std::string Hud::drag(float x, float y, bool down) {
    d->mx = x;
    d->my = y;
    if (!down) {
        d->dragging.clear();
        return "";
    }
    if (d->dragging.empty()) return "";
    Rect r = d->dragging == "camTrack" ? d->trackRect : d->smoothRect;
    float v = std::clamp((x - r.x) / r.w, 0.0f, 1.0f);
    char buf[64];
    std::snprintf(buf, sizeof buf, "%s=%.2f", d->dragging.c_str(), v);
    return buf;
}

// ---------- in-game HUD ----------
void Hud::Impl::drawHud(const HudState& s, float t) {
    Canvas2D& c = g();
    // top bar
    c.beginPath();
    c.rect(0, 0, W, 64);
    c.fillLinearGradient(0, 0, 0, 64, {{0, cssColor("rgba(10,7,22,0.78)")}, {1, cssColor("rgba(10,7,22,0)")}});
    // brand
    {
        float x = 22, cy = 32;
        c.setStroke(GOLD);
        c.setLineWidth(1);
        c.beginPath();
        c.arc(x + 17, cy, 16.5f, 0, PI * 2);
        c.stroke();
        text("❄", x + 17, cy + 1, SANS, 16, 400, GOLD, TextAlign::Center);
        text("TER'S REALMS", x + 34 + 12, cy - 6, SERIF, 20, 500, CREAM, TextAlign::Left, TextBaseline::Middle, 0.18f);
        text("HD-2D INTERACTIVE ADVENTURE", x + 34 + 12, cy + 12, SANS, 8.5f, 500, MUTED, TextAlign::Left, TextBaseline::Middle, 0.24f);
    }
    // centre tag
    {
        float tw = textW(s.worldTag, SANS, 11, 500, 0.22f);
        float total = 6 + 8 + tw + 8 + 26;
        float x = W / 2 - total / 2, cy = 32;
        c.save();
        shadow(8, GOLD, 0, 0);
        c.setFill(GOLD);
        c.beginPath();
        c.arc(x + 3, cy, 3, 0, PI * 2);
        c.fill();
        c.restore();
        text(s.worldTag, x + 14, cy, SANS, 11, 500, MUTED, TextAlign::Left, TextBaseline::Middle, 0.22f);
        Rect tag{x + 14 + tw + 8, cy - 9, 26, 18};
        c.setStroke(cssColor("rgba(246,238,223,0.25)"));
        c.beginPath();
        c.roundRect(tag.x + 0.5f, tag.y + 0.5f, tag.w - 1, tag.h - 1, 4);
        c.stroke();
        text("01", tag.x + tag.w / 2, cy, SANS, 11, 500, MUTED, TextAlign::Center, TextBaseline::Middle, 0.22f);
    }
    // right-hand buttons
    {
        float x = W - 22, y = 14;
        float sw = textW("Engine settings", SANS, 13, 500) + 18 + 8 + 28;
        x -= sw;
        Rect set{x, y, sw, 36};
        buttonFrame(set, hover(set));
        icon("settings", set.x + 14, y + 9);
        text("Engine settings", set.x + 14 + 18 + 8, y + 18, SANS, 13, 500, CREAM);
        region(set, "settings");
        x -= 6 + 1 + 6 + 8;
        c.setFill(cssColor("rgba(246,238,223,0.15)"));
        c.fillRect(x + 8 + 6, y + 7, 1, 22);
        for (const char* id : {"full", "shot", "music"}) {
            x -= 36;
            Rect r{x, y, 36, 36};
            bool off = std::string(id) == "music" && s.muted;
            buttonFrame(r, hover(r), off ? 0.55f : 1);
            icon(id, x + 9, y + 9, off ? 0.55f : 1, !off);
            region(r, id);
            x -= 8;
        }
        float rw = textW(s.realmText, SANS, 13, 500) + textW(s.realmIcon, SANS, 13, 500) + 8 + 28;
        x -= rw;
        Rect realm{x, y, rw, 36};
        buttonFrame(realm, hover(realm));
        text(s.realmIcon + "  " + s.realmText, realm.x + 14, y + 18, SANS, 13, 500, CREAM);
        region(realm, "realm");
    }

    // zone name (swap animation: fade + slide in from the left)
    {
        float k = std::clamp((t - zoneAt) / 0.8f, 0.0f, 1.0f), ease = 1 - (1 - k) * (1 - k) * (1 - k);
        c.save();
        c.setGlobalAlpha(ease);
        c.translate(-12 * (1 - ease), 0);
        shadow(12, cssColor("rgba(0,0,0,0.6)"), 0, 2);
        float x = 28, y = 86;
        std::string eb = s.zoneRegion;
        float ew = textW(eb, SANS, 10.5f, 400, 0.22f);
        text(eb, x, y + 6, SANS, 10.5f, 400, MUTED, TextAlign::Left, TextBaseline::Middle, 0.22f);
        text("/", x + ew + 6, y + 6, SANS, 10.5f, 400, withAlpha(MUTED, 0.5f), TextAlign::Left, TextBaseline::Middle, 0.22f);
        text("EXPLORATION", x + ew + 6 + 10 + 6, y + 6, SANS, 10.5f, 400, MUTED, TextAlign::Left, TextBaseline::Middle, 0.22f);
        text(s.zoneName, x, y + 13 + 6 + 24, SERIF, 44, 500, CREAM);
        text(s.zoneSub, x, y + 13 + 6 + 48 + 4 + 9, THAI, 13, 400, cssColor("rgba(246,238,223,0.8)"));
        c.restore();
    }

    // time card
    {
        Rect r{W - 22 - 236, 84, 236, 0};
        r.h = 14 + 40 + 12 + 30 + 14;
        panel(r);
        region(r, "@panel");
        std::string icon = s.time == "day" ? "☀" : s.time == "dusk" ? "◒" : "☾";
        std::string name = s.time == "day" ? "Bright morning" : s.time == "dusk" ? "Golden hour" : "Silent night";
        std::string clock = s.time == "day" ? "10:20 · CLEAR DAY" : s.time == "dusk" ? "17:24 · TWILIGHT" : "23:40 · STARRY NIGHT";
        float x = r.x + 16, y = r.y + 14;
        text(icon, x + 14, y + 20, SANS, 24, 400, GOLD, TextAlign::Center);
        text(name, x + 28 + 12, y + 11, SERIF, 22, 500, CREAM);
        text(clock, x + 28 + 12, y + 31, SANS, 11, 400, MUTED, TextAlign::Left, TextBaseline::Middle, 0.14f);
        seg({x, y + 40 + 12, r.w - 32, 30}, {{"time:day", "☀ Day"}, {"time:dusk", "◒ Dusk"}, {"time:night", "☾ Night"}}, "time:" + s.time, false);
    }

    // berry pill (shake when empty, bump when picking)
    {
        float shake = 0, bump = 0;
        float ks = (t - berryShakeAt) / 0.4f;
        if (ks >= 0 && ks <= 1) shake = std::sin(ks * PI * 2) * -5;
        float kb = (t - berryBumpAt) / 0.6f;
        if (kb >= 0 && kb <= 1) bump = kb < 0.4f ? kb / 0.4f : (1 - kb) / 0.6f;
        std::string n = std::to_string(s.berries);
        float nw = std::max(18.0f, textW(n, SANS, 20, 600));
        Rect r{22 + shake, H - 54 - 40, 14 + 16 + 10 + nw + 14, 40};
        c.save();
        c.translate(r.x + r.w / 2, r.y + r.h / 2);
        c.scale(1 + 0.04f * bump, 1 + 0.04f * bump);
        c.translate(-(r.x + r.w / 2), -(r.y + r.h / 2));
        if (bump > 0) {
            c.save();
            shadow(40 * bump, cssColor("rgba(233,201,139,0.4)"), 0, 0);
            c.setStroke(withAlpha(GOLD, bump));
            c.beginPath();
            c.roundRect(r.x, r.y, r.w, r.h, 12);
            c.stroke();
            c.restore();
        }
        panel(r);
        float cx = r.x + 14 + 8, cy = r.y + r.h / 2;
        c.save();
        shadow(10, cssColor("rgba(255,60,90,0.6)"), 0, 0);
        c.beginPath();
        c.arc(cx, cy, 8, 0, PI * 2);
        c.fillRadialGradient(cx - 2.4f, cy - 3.2f, 0, cx - 2.4f, cy - 3.2f, 11,
                             {{0, cssColor("#ffd0d8")}, {0.18f, cssColor("#ffd0d8")}, {0.22f, cssColor("#e0263f")}, {1, cssColor("#8e1426")}});
        c.restore();
        text(n, r.x + 14 + 16 + 10, cy, SANS, 20, 600, CREAM);
        c.restore();
        region(r, "@panel");
    }

    // combat panel above the berry pill: level, health, xp, spin attack cooldown
    if (s.combat) {
        Rect r{22, H - 54 - 40 - 12 - 58, 250, 58};
        panel(r);
        region(r, "@panel");
        float x = r.x + 14, y = r.y + 10;
        text("Lv " + std::to_string(s.level), x, y + 9, SANS, 15, 700, GOLD);
        char hpText[32];
        std::snprintf(hpText, sizeof hpText, "%d / %d", (int)std::ceil(s.hp), (int)std::round(s.maxHp));
        text(hpText, r.x + r.w - 14 - 34, y + 9, SANS, 11, 600, CREAM, TextAlign::Right);
        // health, then xp underneath
        float bw = r.x + r.w - 14 - 34 - x, by = y + 22;
        auto bar = [&](float yy, float h, float k, bool hpBar) {
            c.setFill(cssColor("rgba(0,0,0,0.35)"));
            c.beginPath();
            c.roundRect(x, yy, bw, h, h / 2);
            c.fill();
            if (k <= 0) return;
            c.beginPath();
            c.roundRect(x, yy, bw * std::min(1.0f, k), h, h / 2);
            if (hpBar) {
                c.fillLinearGradient(0, yy, 0, yy + h, {{0, cssColor("#ff8a8a")}, {1, cssColor("#c8203a")}});
            } else {
                c.setFill(GOLD);
                c.fill();
            }
        };
        bar(by, 10, std::clamp(s.hp / std::max(1.0f, s.maxHp), 0.0f, 1.0f), true);
        bar(by + 15, 4, s.xpK, false);
        // spin attack: a ring that fills up
        float cx = r.x + r.w - 14 - 12, cy = r.y + r.h / 2;
        c.setStroke(cssColor("rgba(0,0,0,0.35)"));
        c.setLineWidth(3);
        c.beginPath();
        c.arc(cx, cy, 11, 0, PI * 2);
        c.stroke();
        c.setStroke(s.specialK >= 1 ? GOLD : withAlpha(CREAM, 0.6f));
        c.beginPath();
        c.arc(cx, cy, 11, -PI / 2, -PI / 2 + PI * 2 * s.specialK);
        c.stroke();
        text("✦", cx, cy, SANS, 11, 600, s.specialK >= 1 ? GOLD : withAlpha(CREAM, 0.5f), TextAlign::Center);
    }

    // minimap panel (the map texture itself is composited on top)
    {
        Rect r{W - 22 - 168, H - 54 - (10 + 148 + 6 + 12 + 8), 168, 10 + 148 + 6 + 12 + 8};
        panel(r);
        region(r, "@panel");
        minimapRect = {r.x + 10, r.y + 10, 148, 148};
        text(s.mmLabel, r.x + 10, r.y + 10 + 148 + 6 + 6, SANS, 9, 400, MUTED, TextAlign::Left, TextBaseline::Middle, 0.14f);
        text(s.playtime, r.x + r.w - 10, r.y + 10 + 148 + 6 + 6, SANS, 9, 400, MUTED, TextAlign::Right, TextBaseline::Middle, 0.14f);
    }

    // E prompt (fades in / out)
    if (promptAlpha > 0.01f && !promptText.empty()) {
        float tw = textW(promptText, THAI, 14, 600, 0.02f);
        Rect r{W / 2 - (tw + 32) / 2, H - 100 - 32, tw + 32, 32};
        c.save();
        c.setGlobalAlpha(promptAlpha);
        panel(r, 16);
        text(promptText, W / 2, r.y + 16, THAI, 14, 600, CREAM, TextAlign::Center, TextBaseline::Middle, 0.02f);
        c.restore();
    }

    // foot bar
    {
        c.beginPath();
        c.rect(0, H - 32, W, 32);
        c.fillLinearGradient(0, H, 0, H - 32, {{0, cssColor("rgba(10,7,22,0.75)")}, {1, cssColor("rgba(10,7,22,0)")}});
        RGBA col = cssColor("rgba(246,238,223,0.45)");
        float y = H - 16;
        c.save();
        shadow(8, GOLD, 0, 0);
        c.setFill(GOLD);
        c.beginPath();
        c.arc(27, y, 3, 0, PI * 2);
        c.fill();
        c.restore();
        text("SNOWFALL · HD-2D ENGINE", 38, y, SANS, 9.5f, 500, col, TextAlign::Left, TextBaseline::Middle, 0.2f);
        char fps[64];
        std::snprintf(fps, sizeof fps, "%d FPS · OPENGL", s.fps);
        text(fps, W - 24, y, SANS, 9.5f, 500, col, TextAlign::Right, TextBaseline::Middle, 0.2f);
        // the two middle labels sit at the thirds like justify-content: space-between
        text("BUILT WITH C++ · OPENGL", W * 0.39f, y, SANS, 9.5f, 500, col, TextAlign::Center, TextBaseline::Middle, 0.2f);
        text("A LOVE LETTER IN 3D", W * 0.66f, y, SANS, 9.5f, 500, col, TextAlign::Center, TextBaseline::Middle, 0.2f);
    }

    if (s.settingsOpen) drawSettings(s);

    // secret code box
    if (s.codeOpen) {
        float shake = 0, ks = (t - codeShakeAt) / 0.4f;
        if (ks >= 0 && ks <= 1) shake = std::sin(ks * PI * 2) * -5;
        Rect r{W / 2 - 60 + shake, H - 96 - 32, 120, 32};
        c.setFill(cssColor("rgba(42,29,74,0.72)"));
        c.beginPath();
        c.roundRect(r.x, r.y, r.w, r.h, 16);
        c.fill();
        c.setStroke(cssColor("rgba(255,246,234,0.35)"));
        c.beginPath();
        c.roundRect(r.x + 0.5f, r.y + 0.5f, r.w - 1, r.h - 1, 16);
        c.stroke();
        bool caret = std::fmod(t, 1.0f) < 0.5f;
        text(s.codeText + (caret ? "|" : " "), W / 2 + shake, r.y + 16, THAI, 15, 600, cssColor("#fff6ea"), TextAlign::Center, TextBaseline::Middle, 0.15f);
    }
}

void Hud::Impl::drawSettings(const HudState& s) {
    const Settings& st = *s.settings;
    Canvas2D& c = g();
    Rect r{W - 22 - 250, 70, 250, 0};
    // rows: eyebrow, pixel, tilt, shadow, snow, eyebrow, track(2 lines), smooth(2 lines), avoid, reset
    float rowH = 24, gap = 12;
    float h = 14 + 13 + gap + rowH * 4 + gap * 3 + gap + 13 + gap + 44 + gap + 44 + gap + rowH + gap + 30 + 14 + gap + 30;
    r.h = h;
    panel(r);
    region(r, "@panel");
    float x = r.x + 16, w = r.w - 32, y = r.y + 14;
    text("ENGINE SETTINGS", x, y + 6, SANS, 10.5f, 400, GOLD, TextAlign::Left, TextBaseline::Middle, 0.22f);
    y += 13 + gap;
    auto label = [&](const std::string& l, float yy) { text(l, x, yy, THAI, 13, 400, CREAM); };
    label("Pixel art", y + rowH / 2);
    seg({x + w - 110, y, 110, rowH}, {{"pixel:1", "Off"}, {"pixel:2", "2×"}, {"pixel:3", "3×"}}, "pixel:" + std::to_string(st.pixel), true);
    y += rowH + gap;
    label("Tilt-shift (เบลอขอบ)", y + rowH / 2);
    checkbox({x + w - 16, y + 4, 16, 16}, st.tilt, "opt:tilt");
    y += rowH + gap;
    label("เงา (Shadows)", y + rowH / 2);
    checkbox({x + w - 16, y + 4, 16, 16}, st.shadows, "opt:shadows");
    y += rowH + gap;
    label("หิมะตก", y + rowH / 2);
    checkbox({x + w - 16, y + 4, 16, 16}, st.snow, "opt:snow");
    y += rowH + gap;
    text("CAMERA", x, y + 6, SANS, 10.5f, 400, GOLD, TextAlign::Left, TextBaseline::Middle, 0.22f);
    y += 13 + gap;
    auto range = [&](const std::string& l, const std::string& lo, const std::string& hi, float v, const std::string& id, Rect& out) {
        label(l, y + 9);
        float lw = textW(lo, THAI, 10, 400), hw = textW(hi, THAI, 10, 400);
        float ry = y + 18 + 6;
        text(lo, x, ry + 10, THAI, 10, 400, MUTED);
        text(hi, x + w, ry + 10, THAI, 10, 400, MUTED, TextAlign::Right);
        out = {x + lw + 8, ry, w - lw - hw - 16, 20};
        slider(out, v);
        region(out, id);
        y += 44 + gap;
    };
    range("หมุนตามทิศที่เดิน", "คงที่", "ตามติด", st.camTrack, "camTrack", trackRect);
    range("ความนุ่มของกล้อง", "ไว", "นุ่ม", st.camSmooth, "camSmooth", smoothRect);
    label("หลบต้นไม้/บ้านอัตโนมัติ", y + rowH / 2);
    checkbox({x + w - 16, y + 4, 16, 16}, st.camAvoid, "opt:camAvoid");
    y += rowH + gap;
    // .btn.ghost.small
    {
        std::string l = "รีเซ็ตมุมกล้อง";
        float tw = textW(l, THAI, 13, 500);
        Rect b{x, y, tw + 28, 30};
        c.setFill(cssColor("rgba(255,255,255,0.06)"));
        c.beginPath();
        c.roundRect(b.x, b.y, b.w, b.h, 15);
        c.fill();
        c.setStroke(cssColor("rgba(246,238,223,0.28)"));
        c.beginPath();
        c.roundRect(b.x + 0.5f, b.y + 0.5f, b.w - 1, b.h - 1, 15);
        c.stroke();
        text(l, b.x + b.w / 2, b.y + 15, THAI, 13, 500, CREAM, TextAlign::Center);
        region(b, "camReset");
    }
    y += 30 + gap;
    // start over (wipes progress)
    {
        std::string l = "เริ่มใหม่ ↺";
        float tw = textW(l, THAI, 13, 500);
        Rect b{x, y, tw + 28, 30};
        c.setFill(cssColor("rgba(255,255,255,0.06)"));
        c.beginPath();
        c.roundRect(b.x, b.y, b.w, b.h, 15);
        c.fill();
        c.setStroke(cssColor("rgba(255,143,199,0.45)"));
        c.beginPath();
        c.roundRect(b.x + 0.5f, b.y + 0.5f, b.w - 1, b.h - 1, 15);
        c.stroke();
        text(l, b.x + b.w / 2, b.y + 15, THAI, 13, 500, CREAM, TextAlign::Center);
        region(b, "restart");
    }
}

void Hud::Impl::drawHearts(float t) {
    Canvas2D& c = g();
    for (auto& h : hearts) {
        float k = (t - h.start) / h.dur;
        if (k < 0 || k > 1) continue;
        // rise: translateY(-115vh) rotate(30deg), fading to 0.2
        float y = H + 40 - h.size - k * 1.15f * H;
        float x = h.x / 100 * W;
        c.save();
        c.setGlobalAlpha(1 - 0.8f * k);
        c.translate(x, y);
        c.rotate(k * 30 * PI / 180);
        text(h.emoji, 0, 0, SANS, h.size, 400, CREAM, TextAlign::Left, TextBaseline::Top);
        c.restore();
    }
    hearts.erase(std::remove_if(hearts.begin(), hearts.end(), [&](const Heart& h) { return t > h.start + h.dur; }), hearts.end());
}

void Hud::draw(const HudState& s, int fbw, int fbh, float scale, float t, Minimap* minimap) {
    Impl& I = *d;
    static float lastT = t;
    float dt = std::max(0.0f, t - lastT);
    lastT = t;
    I.scale = scale;
    I.W = fbw / scale;
    I.H = fbh / scale;
    // fade transition (1 s) and prompt opacity (0.3 s)
    fade_ += std::clamp(fadeGoal_ - fade_, -dt, dt);
    if (!s.prompt.empty()) I.promptText = s.prompt;
    float pg = s.prompt.empty() ? 0.0f : 1.0f;
    I.promptAlpha += std::clamp(pg - I.promptAlpha, -dt / 0.3f, dt / 0.3f);

    bool hudVisible = s.mode != "loading";
    bool animating = !I.hearts.empty() || t - I.zoneAt < 0.9f || t - I.berryShakeAt < 0.5f || t - I.berryBumpAt < 0.7f ||
                     t - I.codeShakeAt < 0.5f || (I.promptAlpha > 0 && I.promptAlpha < 1) || s.codeOpen;
    std::ostringstream sig;
    sig << fbw << 'x' << fbh << s.mode << s.worldTag << s.realmIcon << s.realmText << s.zoneRegion << s.zoneName << s.zoneSub << s.time
        << s.berries << s.prompt << s.mmLabel << s.playtime << s.fps << s.muted << s.settingsOpen << s.combat << (int)std::ceil(s.hp) << (int)s.maxHp << s.level << (int)(s.xpK * 200)
        << (int)(s.specialK * 60) << s.codeOpen << s.codeText << I.mx << ',' << I.my;
    if (s.settings)
        sig << s.settings->pixel << s.settings->tilt << s.settings->shadows << s.settings->snow << s.settings->camAvoid << s.settings->camTrack
            << s.settings->camSmooth;
    std::string sg = sig.str();
    if (sg != I.lastSig || animating) {
        I.lastSig = sg;
        I.canvas->resize(fbw, fbh);
        I.canvas->clear();
        I.regions.clear();
        I.canvas->save();
        I.canvas->scale(scale, scale);
        if (hudVisible) I.drawHud(s, t);
        I.drawHearts(t);
        I.canvas->restore();
        I.texFront ^= 1;
        I.canvas->uploadPremultiplied(*I.texs[I.texFront]);
    }

    // ---------- composite ----------
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glViewport(0, 0, fbw, fbh);
    glBindVertexArray(I.vao);
    I.overlay.use();
    I.overlay.set("uScreen", glm::vec2(fbw, fbh));
    I.overlay.set("uTex", 0);
    auto quad = [&](glm::vec4 rect, Texture* tex, glm::vec4 color, glm::vec4 uv = {0, 0, 1, 1}, float radius = 0) {
        I.overlay.set("uRect", rect);
        I.overlay.set("uUv", uv);
        I.overlay.set("uColor", color);
        I.overlay.set("uRadius", radius);
        I.overlay.set("uHasTex", tex ? 1 : 0);
        if (tex) tex->bind(0);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    };
    // the HUD canvas is uploaded as is: premultiplied, top row first
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    quad({0, 0, (float)fbw, (float)fbh}, I.texs[I.texFront].get(), {1, 1, 1, 1}, {0, 0, 1, 1});
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    if (hudVisible && minimap) {
        Rect m = I.minimapRect;
        quad({m.x * scale, m.y * scale, m.w * scale, m.h * scale}, &minimap->texture(), {1, 1, 1, 1}, {0, 1, 1, 0}, 6 * scale);
        // the "N" badge sits on top of the map (.mm-n)
        if (!I.badge || I.badgeScale != scale) {
            I.badgeScale = scale;
            Canvas2D b((int)(20 * scale), (int)(16 * scale));
            b.scale(scale, scale);
            b.setFill(PANEL);
            b.beginPath();
            b.roundRect(0, 0, 20, 16, 3);
            b.fill();
            b.setFont(SANS, 10, 600);
            b.setFill(CREAM);
            b.setTextAlign(TextAlign::Center);
            b.setTextBaseline(TextBaseline::Middle);
            b.fillText("N", 10, 8.5f);
            if (!I.badge) I.badge = std::make_shared<Texture>();
            b.upload(*I.badge, false, false);
        }
        quad({(m.x + m.w / 2 - 10) * scale, (m.y - 6) * scale, 20 * scale, 16 * scale}, I.badge.get(), {1, 1, 1, 1}, {0, 1, 1, 0});
    }
    if (fade_ > 0) quad({0, 0, (float)fbw, (float)fbh}, nullptr, {1, 248 / 255.f, 236 / 255.f, fade_});
    // hit: red flash that fades
    float hk = (t - I.hurtAt) / 0.4f;
    if (hk >= 0 && hk < 1) quad({0, 0, (float)fbw, (float)fbh}, nullptr, {0.85f, 0.05f, 0.1f, 0.28f * (1 - hk)});
    float fk = (t - I.flashAt);
    if (fk >= 0 && fk < 0.48f) {
        float a = fk < 0.08f ? 0.8f * fk / 0.08f : 0.8f * (1 - (fk - 0.08f) / 0.4f);
        quad({0, 0, (float)fbw, (float)fbh}, nullptr, {1, 1, 1, std::max(0.0f, a)});
    }
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}
