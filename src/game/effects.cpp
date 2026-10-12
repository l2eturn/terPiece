// Port of effects.js
#include "game/effects.h"
#include "art/glow.h"
#include "core/math.h"
#include "gfx/shader.h"

// ---------- snowfall (computed in the shader, no CPU cost) ----------
Snowfall::Snowfall(int count) {
    const glm::vec3 BOX(70, 32, 70);
    auto g = std::make_shared<Geometry>();
    ExtraAttribute rnd{"rnd", 1, {}};
    for (int i = 0; i < count; ++i) {
        g->position.push_back({frand() * BOX.x, frand() * BOX.y, frand() * BOX.z});
        rnd.data.push_back(frand());
    }
    g->extra.push_back(rnd);
    auto m = std::make_shared<Material>();
    m->type = MaterialType::Custom;
    m->shader = sharedShader("snow");
    m->map = glowTexture();
    m->transparent = true;
    m->depthWrite = false;
    m->uniforms = [this, BOX](const Shader& s) {
        s.set("uTime", time_);
        s.set("uCenter", center_);
        s.set("uBox", BOX);
    };
    object = makePoints(g, m);
}

void Snowfall::update(float t, glm::vec3 center) {
    time_ = t;
    center_ = center;
}

// ---------- fireworks ----------
Fireworks::Fireworks(int capacity) : capacity_(capacity) {
    geo_ = std::make_shared<Geometry>();
    // rewritten every frame
    geo_->dynamic = true;
    geo_->position.assign(capacity, glm::vec3(0));
    geo_->extra.push_back({"aColor", 4, std::vector<float>((size_t)capacity * 4, 0)});
    vel_.assign(capacity, glm::vec3(0));
    life_.assign(capacity, 0);
    auto m = std::make_shared<Material>();
    m->type = MaterialType::Custom;
    m->shader = sharedShader("fireworks");
    m->map = glowTexture();
    m->transparent = true;
    m->depthWrite = false;
    m->blending = Blending::Additive;
    object = makePoints(geo_, m);
}

void Fireworks::burst(glm::vec3 p, glm::vec3 color, int n) {
    static const uint32_t palette[6] = {0xff4f9a, 0xffd76a, 0x7ad7ff, 0xb98bff, 0x8affc1, 0xffffff};
    glm::vec3 c = color.x < 0 ? hexc(palette[(int)(frand() * 6) % 6]) : color;
    auto& col = geo_->extra[0].data;
    for (int i = 0; i < n; ++i) {
        int k = cursor_;
        cursor_ = (cursor_ + 1) % capacity_;
        geo_->position[k] = p;
        vel_[k] = randomDirection() * (5 + frand() * 3);
        col[k * 4] = c.r;
        col[k * 4 + 1] = c.g;
        col[k * 4 + 2] = c.b;
        col[k * 4 + 3] = 1;
        life_[k] = 1.4f + frand() * 0.8f;
    }
}

void Fireworks::launch(glm::vec3 from, float height) { rockets_.push_back({from, height * 1.1f, 0, from.y + height}); }

void Fireworks::update(float dt) {
    auto& col = geo_->extra[0].data;
    for (int i = (int)rockets_.size() - 1; i >= 0; --i) {
        Rocket& r = rockets_[i];
        r.p.y += r.vy * dt;
        r.p.x += std::sin(r.t * 20) * 0.02f;
        r.t += dt;
        // rocket trail
        int k = cursor_;
        cursor_ = (cursor_ + 1) % capacity_;
        geo_->position[k] = r.p;
        vel_[k] = {0, -1, 0};
        col[k * 4] = 1;
        col[k * 4 + 1] = 0.85f;
        col[k * 4 + 2] = 0.6f;
        col[k * 4 + 3] = 0.8f;
        life_[k] = 0.4f;
        if (r.p.y >= r.target) {
            burst(r.p);
            rockets_.erase(rockets_.begin() + i);
        }
    }
    for (int i = 0; i < capacity_; ++i) {
        if (life_[i] <= 0) continue;
        life_[i] -= dt;
        vel_[i].y -= 4.5f * dt;
        vel_[i] *= 1 - 1.6f * dt;
        geo_->position[i] += vel_[i] * dt;
        col[i * 4 + 3] = std::clamp(life_[i], 0.0f, 1.0f);
    }
    geo_->markDirty();
}

// ---------- footprints in the snow ----------
Footprints::Footprints(int count) {
    auto g = geo::circle(0.07f, 10);
    g->scale(1, 1.6f, 1);
    g->rotateX(-PI / 2);
    auto m = basic(hexc(0x8f97c9));
    m->transparent = true;
    m->opacity = 0.55f;
    m->depthWrite = false;
    m->polygonOffset = true;
    m->polygonOffsetFactor = -2;
    object = makeInstanced(g, {m}, count);
    data_.resize(count);
    born_.assign(count, -999);
    for (auto& mtx : object->instances) mtx = glm::mat4(0);
}

void Footprints::add(glm::vec3 p, float yaw, float t) {
    data_[cursor_] = {p, yaw};
    born_[cursor_] = t;
    cursor_ = (cursor_ + 1) % (int)data_.size();
}

void Footprints::update(float t) {
    for (size_t i = 0; i < data_.size(); ++i) {
        float age = t - born_[i];
        float k = age < 0 || age > 12 ? 0 : 1 - std::max(0.0f, age - 6) / 6;
        object->instances[i] = composeMatrix(data_[i].p, glm::angleAxis(data_[i].yaw, glm::vec3(0, 1, 0)), glm::vec3(k));
    }
    object->instancesDirty = true;
}
