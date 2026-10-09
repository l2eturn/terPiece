#pragma once
// HUD + settings popover — port of the HTML/CSS overlay in index.html / style.css.
// Drawn with Canvas2D in CSS pixels (scaled to device pixels) and composited over the 3D view.
#include <memory>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "gfx/shader.h"

struct Settings;
class Minimap;
class Canvas2D;
class Texture;

struct HudState {
    // play / fade
    std::string mode = "play";
    // top bar
    std::string worldTag = "THE WINTER REALM", realmIcon = "🏰", realmText = "สู่ยุคกลาง";
    // zone
    std::string zoneRegion = "THE SNOWLANDS", zoneName = "Village Square", zoneSub;
    // time card
    std::string time = "dusk";
    int berries = 0;
    // "E · ..." interaction prompt ("" = hidden)
    std::string prompt;
    std::string mmLabel = "⌂ WINTER VILLAGE", playtime = "00:00";
    int fps = 60;
    bool muted = false;
    bool settingsOpen = false;
    const Settings* settings = nullptr;
    // secret code box (Y)
    bool codeOpen = false;
    std::string codeText;
};

class Hud {
public:
    Hud();
    ~Hud();
    bool init();
    // draw everything over the current framebuffer (after the 3D view); sizes in device pixels, scale = content scale
    void draw(const HudState& s, int fbw, int fbh, float scale, float t, Minimap* minimap);

    // mouse in window points; returns the id of the clicked control ("" = none)
    std::string click(float x, float y);
    // slider dragging: returns "camTrack=0.42" style ids while a slider is held
    std::string drag(float x, float y, bool down);
    bool overUi(float x, float y) const;

    // one-shot animations
    void zoneSwap(float t);
    void floatHearts(int n, float t);
    void shakeBerry(float t);
    void bumpBerry(float t);
    void shakeCode(float t);
    void setFade(bool on);
    void flash(float t);
    float fadeAlpha() const { return fade_; }

private:
    struct Impl;
    std::unique_ptr<Impl> d;
    float fade_ = 0, fadeGoal_ = 0;
};
