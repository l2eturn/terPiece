// Port of home.js (most of it originally came from the old style/module.js)
#include "game/home.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include "core/math.h"
#include "gfx/shader.h"
#include "gfx/texture.h"
#include "scene/gltf.h"

namespace {

std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

std::string modelPath(const char* f) { return std::string(ASSET_DIR) + "/models/" + f; }

// piecewise-linear spline over (t, value) points (getLinearSpline)
template <class T>
struct Spline {
    std::vector<std::pair<float, T>> pts;
    T at(float t) const {
        size_t p1 = 0;
        for (size_t i = 0; i < pts.size(); ++i) {
            if (pts[i].first >= t) break;
            p1 = i;
        }
        size_t p2 = std::min(pts.size() - 1, p1 + 1);
        if (p1 == p2) return pts[p1].second;
        float k = (t - pts[p1].first) / (pts[p2].first - pts[p1].first);
        return pts[p1].second + (pts[p2].second - pts[p1].second) * k;
    }
};

}  // namespace

// ---------- fireplace particles (getParticleSystem) ----------
struct HomeDiorama::Fire {
    struct P {
        glm::vec3 pos, vel;
        float size, currentSize = 0, alpha = 1, life, maxLife, rotation, rotationRate;
        glm::vec3 colour{1};
    };
    std::vector<P> particles;
    // particles are rewritten every frame
    GeoPtr geo = [] {
        auto g = std::make_shared<Geometry>();
        g->dynamic = true;
        return g;
    }();
    ObjPtr points;
    glm::vec3 emitter{0.1f, -2.2f, -1.6f};
    float accum = 0;
    const float rate = 200, radius = 0.5f, maxLife = 1.5f, maxSize = 3.0f;
    Spline<float> alphaSpline{{{0.0f, 0.0f}, {0.6f, 1.0f}, {1.0f, 0.0f}}};
    Spline<glm::vec3> colorSpline{{{0.0f, hexc(0xffffff)}, {1.0f, hexc(0xff8080)}}};
    Spline<float> sizeSpline{{{0.0f, 0.0f}, {1.0f, 1.0f}}};

    Fire() {
        auto tex = std::make_shared<Texture>();
        // TextureLoader defaults: linear encoding, linear filtering
        TextureOptions o;
        o.srgb = false;
        o.nearest = false;
        o.repeat = false;
        tex->load(modelPath("fire.png"), o);
        auto m = std::make_shared<Material>();
        m->type = MaterialType::Custom;
        m->shader = sharedShader("fireparticles");
        m->map = tex;
        m->transparent = true;
        m->depthWrite = false;
        m->blending = Blending::Additive;
        geo->extra = {{"size", 1, {}}, {"angle", 1, {}}, {"aColor", 4, {}}};
        points = makePoints(geo, m);
    }

    void update(float dt) {
        // spawn
        accum += dt;
        int n = (int)std::floor(accum * rate);
        accum -= n / rate;
        for (int i = 0; i < n; ++i) {
            float life = (frand() * 0.75f + 0.25f) * maxLife;
            P p;
            p.pos = glm::vec3((frand() * 1.5f - 1) * radius, (frand() * 0.125f - 1) * radius, (frand() * 1.5f - 1) * radius) + emitter;
            p.size = (frand() * 0.5f + 0.5f) * maxSize;
            p.life = p.maxLife = life;
            p.rotation = frand() * 2 * PI;
            p.rotationRate = frand() * 0.01f - 0.005f;
            p.vel = {0, 1.5f, 0};
            particles.push_back(p);
        }
        // age
        for (auto& p : particles) p.life -= dt;
        particles.erase(std::remove_if(particles.begin(), particles.end(), [](const P& p) { return p.life <= 0; }), particles.end());
        for (auto& p : particles) {
            float t = 1 - p.life / p.maxLife;
            p.rotation += p.rotationRate;
            p.alpha = alphaSpline.at(t);
            p.currentSize = p.size * sizeSpline.at(t);
            p.colour = colorSpline.at(t);
            p.pos += p.vel * dt;
            glm::vec3 drag = p.vel * (dt * 0.1f);
            for (int k = 0; k < 3; ++k) drag[k] = (p.vel[k] > 0 ? 1.0f : p.vel[k] < 0 ? -1.0f : 0.0f) * std::min(std::abs(drag[k]), std::abs(p.vel[k]));
            p.vel -= drag;
        }
        // geometry
        geo->position.clear();
        for (auto& e : geo->extra) e.data.clear();
        for (auto& p : particles) {
            geo->position.push_back(p.pos);
            geo->extra[0].data.push_back(p.currentSize);
            geo->extra[1].data.push_back(p.rotation);
            geo->extra[2].data.insert(geo->extra[2].data.end(), {p.colour.r, p.colour.g, p.colour.b, p.alpha});
        }
        geo->markDirty();
    }
};

HomeDiorama::HomeDiorama(bool characters, bool darken) : fire_(std::make_unique<Fire>()) {
    group = makeGroup("home");

    auto baked = std::make_shared<Texture>();
    TextureOptions bo;
    bo.srgb = false;
    bo.nearest = false;
    bo.flipY = false;
    // the bake is 4096²: the room never fills the screen, 1024 looks the same at a sixteenth of the memory
    bo.maxSize = 1024;
    baked->load(modelPath("baked.jpg"), bo);
    auto bakedMaterial = basic(glm::vec3(1), baked);
    auto snowMaterial = standard(hexc(0xeeeeee), 0.6f, 0.1f);
    auto windowMaterial = basic(hexc(0xffdcc2));
    windowMaterial->side = Side::Double;
    auto neonBase = standard(glm::vec3(1), 1, 0);
    neonBase->emissive = glm::vec3(1);
    neonBase->side = Side::Double;
    auto neon = [](uint32_t base, uint32_t fin, float* time) {
        auto m = std::make_shared<Material>();
        m->type = MaterialType::Custom;
        m->shader = sharedShader("neon");
        glm::vec3 b = hexc(base), f = hexc(fin);
        m->uniforms = [b, f, time](const Shader& s) {
            s.set("time", *time);
            s.set("delay", 1.0f);
            s.set("colorSpeed", 5.0f);
            s.set("baseColor", b);
            s.set("finalColor", f);
        };
        return m;
    };
    neon1_ = neon(0xaa00ff, 0xffccff, &neon1Time_);
    neon2_ = neon(0xe39f9f, 0xffffff, &neon2Time_);
    auto fireMaterial = lambertColor(hexc(0xffdab9));
    fireMaterial->side = Side::Double;

    // the room (bear / hats hidden like the original)
    if (auto room = loadGltf(modelPath("model.glb"))) {
        room->root->traverse([&](Object3D& c) {
            if (c.kind == ObjectKind::Mesh) {
                for (auto& m : c.materials) m = bakedMaterial;
                c.receiveShadow = true;
            }
            std::string n = lower(c.name);
            bool hide = false;
            for (const char* k : {"bear", "sphere", "cylinder", "hat", "cap", "santa"})
                if (n.find(k) != std::string::npos) hide = true;
            if (hide && n.find("lamp") == std::string::npos) c.visible = false;
        });
        group->add(room->root);
    }

    if (auto deco = loadGltf(modelPath("model2.glb"))) {
        auto& names = gltfMaterialNames();
        deco->root->traverse([&](Object3D& c) {
            if (c.kind != ObjectKind::Mesh) return;
            for (auto& m : c.materials) {
                std::string mn = names.count(m.get()) ? names[m.get()] : "";
                if (mn == "GiftBox" || lower(c.name).find("gift") != std::string::npos) {
                    m = standard(hexc(0xffff00), 0.5f, 0.3f);
                    m->side = Side::Double;
                }
                if (mn == "SnowSimple") m = snowMaterial;
                if (mn == "Window") m = windowMaterial;
                if (mn == "NeonBase") m = neonBase;
                if (mn == "Neon.001") m = neon2_;
                if (mn == "Neon") m = neon1_;
                if (mn == "Fire") m = fireMaterial;
            }
        });
        if (!deco->animations.empty()) mixer_ = std::make_unique<AnimationMixer>(deco->animations);
        group->add(deco->root);
    }

    if (characters) {
        auto place = [&](const char* file, float s, glm::vec3 p, float ry, bool dim) {
            auto sc = loadGltf(modelPath(file));
            if (!sc) return;
            ObjPtr o = sc->root;
            if (dim && darken)
                o->traverse([](Object3D& c) {
                    for (auto& m : c.materials) {
                        m = m->clone();
                        m->color = glm::vec3(0.15f);
                        m->roughness = 1;
                        m->metalness = 0;
                    }
                });
            o->scale = glm::vec3(s);
            o->position = p;
            o->rotation.y = ry;
            group->add(o);
        };
        place("cham2.glb", 0.6f, {0.3f, -1.2f, -0.01f}, -PI / 5, false);
        place("juice.glb", 1, {-0.5f, -0.93f, 1}, -PI / 5, false);
        place("snowman.glb", 0.75f, {0.2f, -2, 0.7f}, PI / 4, true);
        place("patt.glb", 1.25f, {-0.6f, -0.8f, 0.5f}, PI / 4, true);
    }

    group->add(fire_->points);
}

HomeDiorama::~HomeDiorama() = default;

void HomeDiorama::update(float dt) {
    if (mixer_) mixer_->update(dt);
    neon1Time_ += 0.075f;
    neon2Time_ += 0.09f;
    fire_->update(0.016f);
}
