// Port of minimap.js
#include "ui/minimap.h"
#include <cmath>
#include <cstdio>
#include "core/math.h"
#include "gfx/texture.h"
#include "world/world.h"

Minimap::Minimap(int size) : W_(size), bg_(size, size), fg_(size, size), tex_(std::make_shared<Texture>()) {}

glm::vec2 Minimap::toPx(float x, float z) const {
    glm::vec2 c = data_ ? data_->center : glm::vec2(0);
    return {W_ / 2.0f + ((x - c.x) / R_) * (W_ / 2.0f), W_ / 2.0f + ((z - c.y) / R_) * (W_ / 2.0f)};
}

void Minimap::setMapData(const MapData& data) {
    data_ = &data;
    R_ = data.radius + 4;
    drawBackground();
}

void Minimap::drawBackground() {
    Canvas2D& g = bg_;
    const MapData& d = *data_;
    float W = (float)W_, half = W / 2;
    auto circle = [&](glm::vec2 p, float r, const char* col) {
        g.setFill(col);
        g.beginPath();
        g.arc(p.x, p.y, r, 0, PI * 2);
        g.fill();
    };
    auto paths = [&](const char* col) {
        g.setStroke(col);
        g.setLineWidth(4);
        g.setLineCap(true);
        for (auto& path : d.paths) {
            g.beginPath();
            for (size_t i = 0; i < path.size(); ++i) {
                glm::vec2 p = toPx(path[i].x, path[i].y);
                if (i) g.lineTo(p.x, p.y);
                else g.moveTo(p.x, p.y);
            }
            g.stroke();
        }
    };
    g.clear();
    g.setFill("#151124");
    g.fillRect(0, 0, W, W);

    if (d.type == "wilds") {
        // the valley between the towns (new map): snow fading into grass, road, river, two lakes
        g.beginPath();
        g.rect(0, 0, W, W);
        glm::vec2 top = toPx(0, d.center.y - d.radius), bottom = toPx(0, d.center.y + d.radius);
        g.fillLinearGradient(0, top.y, 0, bottom.y, {{0, cssColor("#d9dcef")}, {0.45f, cssColor("#c9d8c0")}, {0.75f, cssColor("#4c8c4a")}, {1, cssColor("#4c8c4a")}});
        // valley walls
        g.setFill("rgba(21,17,36,0.55)");
        glm::vec2 wl = toPx(-34, 0), wr = toPx(34, 0);
        g.fillRect(0, 0, wl.x, W);
        g.fillRect(wr.x, 0, W - wr.x, W);
        // river + lakes
        g.setStroke("#6fb7e6");
        g.setLineWidth((7 / R_) * half);
        g.setLineCap(true);
        g.beginPath();
        for (size_t i = 0; i < d.river.size(); ++i) {
            glm::vec2 p = toPx(d.river[i].x, d.river[i].y);
            if (i) g.lineTo(p.x, p.y);
            else g.moveTo(p.x, p.y);
        }
        g.stroke();
        circle(toPx(d.pond.x, d.pond.y), (d.pond.z / R_) * half, "#6fb7e6");
        circle(toPx(d.lake.x, d.lake.y), (d.lake.z / R_) * half, "#6fb7e6");
        paths("#a99c86");
        if (d.trees)
            for (auto& t : *d.trees) {
                if (t.removed) continue;
                circle(toPx(t.x, t.z), std::max(1.5f, (t.r / R_) * half * 0.7f), "#2f6a5a");
            }
        // the two town gates
        circle(toPx(0, 80), 6, "#e9c98b");
        circle(toPx(0, 176), 6, "#e9c98b");
        // campfire
        circle(toPx(-9, 128), 3.5f, "#ff9f3b");
    } else if (d.type == "medieval") {
        // kingdom grass
        circle({half, half}, (d.radius / R_) * half, "#4c8c4a");
        // outer fortress wall
        g.setStroke("#5a586a");
        g.setLineWidth(5);
        g.beginPath();
        g.arc(half, half, (64 / R_) * half, 0, PI * 2);
        g.stroke();
        paths("#8c8072");
        // moat water
        {
            glm::vec2 m = toPx(0, -21);
            g.setStroke("#2d6596");
            g.setLineWidth((8 / R_) * half);
            g.beginPath();
            g.arc(m.x, m.y, (14 / R_) * half, 0, PI);
            g.stroke();
        }
        // drawbridge
        {
            glm::vec2 p = toPx(d.drawbridge.x, d.drawbridge.y);
            g.setFill("#6e4a32");
            g.fillRect(p.x - 5, p.y - 10, 10, 20);
        }
        // castle + four corner towers
        {
            glm::vec2 c = toPx(d.castle.x, d.castle.y);
            float pw = (d.castle.z / R_) * half, pd = (d.castle.w / R_) * half;
            g.setFill("#555264");
            g.fillRect(c.x - pw / 2, c.y - pd / 2, pw, pd);
            for (float sx : {-pw / 2, pw / 2})
                for (float sy : {-pd / 2, pd / 2}) circle({c.x + sx, c.y + sy}, 5.5f, "#393c4e");
        }
        // royal fountain
        {
            glm::vec2 f = toPx(d.fountain.x, d.fountain.y);
            circle(f, 5, "#7fe8ff");
            circle(f, 2.5f, "#3877ae");
        }
        // tavern
        {
            glm::vec2 t = toPx(d.tavern.x, d.tavern.y);
            g.setFill("#8b4038");
            g.fillRect(t.x - 7, t.y - 5, 14, 10);
        }
        // training ground
        {
            glm::vec2 a = toPx(d.archery.x, d.archery.y);
            circle(a, (d.archery.z / R_) * half, "#8a6e4b");
            circle({a.x + 4, a.y}, 2, "#d62246");
        }
        // rose garden
        {
            glm::vec2 p = toPx(d.garden.x, d.garden.y);
            g.setFill("#38733f");
            g.fillRect(p.x - 9, p.y - 8, 18, 16);
            circle({p.x - 3, p.y - 2}, 2, "#ff5c8a");
            circle({p.x + 3, p.y + 2}, 2, "#ff5c8a");
        }
        // bell tower
        {
            glm::vec2 b = toPx(d.bellTower.x, d.bellTower.y);
            g.setFill("#464456");
            g.fillRect(b.x - 4, b.y - 4, 8, 8);
            circle(b, 2, "#ffd76a");
        }
        // time portal
        {
            glm::vec2 p = toPx(d.portal.x, d.portal.y);
            circle(p, 6, "#7fe8ff");
            circle(p, 3, "#151124");
        }
        // oaks
        if (d.trees)
            for (auto& t : *d.trees) {
                if (t.removed) continue;
                circle(toPx(t.x, t.z), std::max(1.8f, (t.r / R_) * half * 0.75f), "#2d6836");
            }
    } else {
        // snow
        circle({half, half}, (d.radius / R_) * half, "#d9dcef");
        // hill
        {
            glm::vec2 p = toPx(d.hill.x, d.hill.y);
            float r = (d.hill.z * 1.6f / R_) * W / 2;
            g.beginPath();
            g.rect(0, 0, W, W);
            g.fillRadialGradient(p.x, p.y, 0, p.x, p.y, r, {{0, cssColor("rgba(160,165,205,0.7)")}, {1, cssColor("rgba(160,165,205,0)")}});
        }
        // lake
        circle(toPx(d.pond.x, d.pond.y), (d.pond.z / R_) * half, "#6fb7e6");
        paths("#a99c86");
        // mountain terraces + cave mouth
        {
            glm::vec2 m = toPx(d.mountain.x, d.mountain.y);
            for (int i = 6; i >= 1; --i) {
                char col[64];
                std::snprintf(col, sizeof col, "rgb(%d,%d,%d)", 150 + i * 10, 148 + i * 10, 170 + i * 9);
                circle(m, (d.mountain.z * i / 6 / R_) * half, col);
            }
            glm::vec2 c = toPx(d.caveMouth.x, d.caveMouth.y);
            circle(c, 5, "#7fe8ff");
            circle(c, 2.5f, "#0c0a1c");
        }
        // gate to the medieval realm
        {
            glm::vec2 p = toPx(d.portal.x, d.portal.y);
            circle(p, 6, "#7fe8ff");
            circle(p, 3, "#151124");
        }
        // trees
        if (d.trees)
            for (auto& t : *d.trees) {
                if (t.removed) continue;
                circle(toPx(t.x, t.z), std::max(1.5f, (t.r / R_) * half * 0.7f), "#2f6a5a");
            }
        // cabins
        g.setFill("#8c3b4a");
        for (auto c : d.cabins) {
            glm::vec2 p = toPx(c.x, c.y);
            g.fillRect(p.x - 6, p.y - 6, 12, 12);
        }
        // home
        {
            glm::vec2 p = toPx(d.home.x, d.home.y);
            g.setFill("#e9c98b");
            g.beginPath();
            g.moveTo(p.x, p.y - 11);
            g.lineTo(p.x + 11, p.y);
            g.lineTo(p.x + 8, p.y);
            g.lineTo(p.x + 8, p.y + 9);
            g.lineTo(p.x - 8, p.y + 9);
            g.lineTo(p.x - 8, p.y);
            g.lineTo(p.x - 11, p.y);
            g.closePath();
            g.fill();
        }
        // Christmas tree
        {
            glm::vec2 p = toPx(0, 0);
            circle(p, 7, "#1d6b4f");
            circle(p, 2.5f, "#ffd76a");
        }
    }
}

void Minimap::draw(glm::vec3 player, float yaw, float camYaw, bool inCave, const std::vector<glm::vec3>& targets, float time) {
    Canvas2D& c = fg_;
    float W = (float)W_;
    auto arrow = [&](float x, float y) {
        c.save();
        c.translate(x, y);
        c.rotate(-yaw + PI);
        c.setFill("#ff4f9a");
        c.setStroke("#fff");
        c.setLineWidth(2);
        c.setLineCap(false);
        c.beginPath();
        c.moveTo(0, -9);
        c.lineTo(7, 7);
        c.lineTo(0, 3);
        c.lineTo(-7, 7);
        c.closePath();
        c.fill();
        c.beginPath();
        c.moveTo(0, -9);
        c.lineTo(7, 7);
        c.lineTo(0, 3);
        c.lineTo(-7, 7);
        c.closePath();
        c.stroke();
        c.restore();
    };
    if (inCave) {
        // inside the cave: no map, only our dot and the crystal glow around it
        c.setFill("#0c0a1c");
        c.fillRect(0, 0, W, W);
        float pulse = 0.5f + 0.5f * std::sin(time * 1000 / 400);
        c.setFill(RGBA{127 / 255.f, 232 / 255.f, 1, 0.15f + pulse * 0.15f});
        c.beginPath();
        c.arc(W / 2, W / 2, 60, 0, PI * 2);
        c.fill();
        arrow(W / 2, W / 2);
        c.upload(*tex_, false, false);
        return;
    }
    c.copyFrom(bg_);
    // companions still lost — blinking dots
    float pulse = 0.5f + 0.5f * std::sin(time * 1000 / 250);
    for (auto& tp : targets) {
        glm::vec2 p = toPx(tp.x, tp.z);
        c.setFill(RGBA{1, 143 / 255.f, 199 / 255.f, 0.3f + pulse * 0.4f});
        c.beginPath();
        c.arc(p.x, p.y, 9 + pulse * 3, 0, PI * 2);
        c.fill();
        c.setFill("#ff4f9a");
        c.beginPath();
        c.arc(p.x, p.y, 4, 0, PI * 2);
        c.fill();
    }
    // camera view cone
    glm::vec2 p = toPx(player.x, player.z);
    float look = std::atan2(-std::cos(camYaw), -std::sin(camYaw));
    c.setFill("rgba(255,255,255,0.18)");
    c.beginPath();
    c.moveTo(p.x, p.y);
    c.arc(p.x, p.y, 34, look - 0.5f, look + 0.5f);
    c.closePath();
    c.fill();
    arrow(p.x, p.y);
    c.upload(*tex_, false, false);
}
