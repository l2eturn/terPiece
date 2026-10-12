// Fighting in the wilds (new — not in the web build). See combat.h.
#include "game/combat.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>
#include "art/textures.h"
#include "core/math.h"
#include "game/player.h"
#include "gfx/texture.h"
#include "scene/geometry.h"
#include "scene/material.h"
#include "scene/object.h"
#include "ui/canvas2d.h"
#include "world/cubeworld.h"
#include "world/layout.h"
#include "world/overworld.h"

namespace {

// ---------- monster kinds ----------
struct Spec {
    std::string kind, name;
    // Cube World animal kind (combat parts in cubeworld.cpp), or a single mesh for slimes
    std::string model;
    bool slime = false, biped = false, flying = false, night = false;
    int level;
    float hp, dmg, speed, radius, reach, aggro, windup, cooldown;
    int xp;
    // knockback taken (0 = never staggers: the boss)
    float stagger;
    // stand-in model colour, death sparks
    glm::vec3 color;
    // stand-in model height (m)
    float size;
};

const std::vector<Spec>& specs() {
    static const std::vector<Spec> S = {
        // kind, name, model, slime, biped, flying, night, level, hp, dmg, speed, radius, reach, aggro, windup, cooldown, xp, stagger, color, size
        {"frost-slime", "Frost Slime", "slime-blue", true, false, false, false, 1, 30, 5, 2.6f, 0.45f, 1.1f, 8, 0.5f, 1.4f, 8, 1, {0.45f, 0.7f, 1}, 0.6f},
        {"slime", "Slime", "slime-green", true, false, false, false, 2, 40, 6, 2.6f, 0.45f, 1.1f, 8, 0.5f, 1.4f, 10, 1, {0.5f, 0.9f, 0.4f}, 0.6f},
        {"wolf", "Wolf", "wolf", false, false, false, false, 3, 60, 9, 5.2f, 0.5f, 1.5f, 11, 0.4f, 1.2f, 18, 0.8f, {0.6f, 0.6f, 0.65f}, 0.9f},
        {"gnoll", "Gnoll", "gnoll", false, true, false, false, 4, 80, 11, 3.8f, 0.45f, 1.6f, 10, 0.55f, 1.5f, 25, 0.7f, {0.8f, 0.7f, 0.5f}, 1.4f},
        {"skeleton", "Skeleton", "skeleton", false, true, false, true, 5, 90, 13, 3.4f, 0.45f, 1.6f, 12, 0.6f, 1.6f, 30, 0.7f, {0.92f, 0.9f, 0.85f}, 1.5f},
        {"bat", "Bat", "bat", false, true, true, true, 3, 35, 6, 5, 0.4f, 1.3f, 12, 0.35f, 1.1f, 12, 1, {0.45f, 0.3f, 0.55f}, 0.6f},
        {"ogre", "Ogre", "ogre", false, true, false, false, 8, 420, 26, 3, 1.0f, 2.4f, 8, 0.9f, 2.2f, 160, 0, {0.55f, 0.7f, 0.4f}, 2.6f},
    };
    return S;
}
const Spec& spec(const std::string& kind) {
    for (auto& s : specs())
        if (s.kind == kind) return s;
    return specs()[0];
}

// where the monsters live (global coordinates; away from the road, the camp and the jetties)
struct Camp {
    std::string kind;
    glm::vec2 at;
    float r;
    int n;
};
const std::vector<Camp>& camps() {
    static const std::vector<Camp> C = {
        // snowy end: frost slimes; grassy end: slimes and gnolls
        {"frost-slime", {-16, 95}, 6, 3},
        {"slime", {-20, 150}, 7, 3},
        {"gnoll", {-25, 138}, 5, 2},
        // across the river: wolves, and the ogre near the spring
        {"wolf", {27, 120}, 5, 2},
        {"wolf", {28, 156}, 5, 2},
        {"ogre", {27, 100}, 3, 1},
        // the night shift
        {"skeleton", {-20, 114}, 6, 3},
        {"skeleton", {-12, 166}, 5, 2},
        {"bat", {8, 140}, 6, 3},
        {"bat", {-6, 104}, 6, 2},
    };
    return C;
}

// damage scales with the level difference (attacker - defender)
float levelMod(int diff) { return std::clamp(1 + 0.1f * diff, 0.4f, 1.8f); }

// heading that makes a model (facing -Z) look from `from` towards `to`
float headingTo(glm::vec2 from, glm::vec2 to) { return std::atan2(-(to.x - from.x), -(to.y - from.y)); }

constexpr float LEASH = 24;
constexpr float REGEN_DELAY = 4;

}  // namespace

// ---------------------------------------------------------------------------

struct Combat::Impl {
    Overworld& world;
    Player& player;
    Combat* self = nullptr;
    float now = 0;
    float lastCombat = -100, lastHurt = -100;

    // ---------- monsters ----------
    enum class State { Idle, Chase, Windup, Recover, Return, Dying, Gone };
    struct Enemy {
        const Spec* spec;
        glm::vec2 home;
        ObjPtr g, rig, head;
        std::vector<ObjPtr> legs;
        float height = 1;
        // own copies of the materials (for the hit flash) and their normal emissive
        std::vector<std::pair<Material*, glm::vec3>> mats;
        ObjPtr bar;
        std::shared_ptr<Texture> barTex;
        int barShown = -1;
        float hp, heading = 0, phase = 0;
        State state = State::Idle;
        float timer = 0, cool = 0, flash = 0, wait = 0, respawn = 0;
        glm::vec2 knock{0}, wander{0};
        bool hasWander = false;
        glm::vec2 pos() const { return {g->position.x, g->position.z}; }
    };
    std::vector<std::unique_ptr<Enemy>> enemies;

    // ---------- the sword ----------
    ObjPtr pivot, sword, arc;
    struct Swing {
        bool on = false, hitDone = false, queued = false, spin = false;
        int step = 0;
        float t = 0, dur = 0;
    } swing;
    float comboUntil = -1, specialAt = -100, rollAt = -100, rollUntil = -100;
    int lastStep = 0;
    float arcT = -1, arcYaw = 0;
    bool arcFull = false;

    // ---------- floating damage numbers ----------
    struct Number {
        ObjPtr s;
        glm::vec3 p;
        float born;
    };
    std::vector<Number> numbers;
    std::map<std::string, std::shared_ptr<Texture>> numberTex;

    Impl(Overworld& w, Player& p) : world(w), player(p) {}

    // ----------------------------------------------------------------------
    // building
    // ----------------------------------------------------------------------
    void buildModel(Enemy& e) {
        const Spec& s = *e.spec;
        auto& cw = cubeWorld();
        if (cw.available() && s.slime && cw.has(s.model)) {
            CwTag tag;
            tag.name = s.model;
            tag.h = s.size;
            e.g = makeGroup();
            e.rig = makeGroup();
            e.rig->add(cw.mesh(s.model, tag));
            e.g->add(e.rig);
            e.head = e.rig;
            e.height = s.size;
        } else if (cw.available() && !s.slime && cw.hasAnimal(s.model)) {
            auto m = cw.buildAnimal(s.model);
            e.g = m.g;
            e.rig = m.rig;
            e.head = m.head;
            e.legs = m.legs;
            e.height = m.height;
        } else {
            buildStandIn(e);
        }
        e.g->name = "monster:" + s.kind;
        // own materials so one monster can flash without the others
        e.g->traverse([&](Object3D& o) {
            if (o.kind != ObjectKind::Mesh) return;
            o.castShadow = true;
            for (auto& m : o.materials) {
                m = m->clone();
                e.mats.push_back({m.get(), m->emissive});
            }
        });
        // health bar + name above the head
        Canvas2D c(256, 64);
        e.barTex = std::make_shared<Texture>();
        c.upload(*e.barTex);
        auto bm = spriteMaterial(e.barTex);
        bm->depthTest = false;
        bm->transparent = true;
        e.bar = makeSprite(bm);
        e.bar->scale = {1.1f, 0.275f, 1};
        e.bar->renderOrder = 16;
        e.bar->visible = false;
        world.addObject(e.bar);
    }

    // low-poly stand-ins when the Cube World assets are missing
    void buildStandIn(Enemy& e) {
        const Spec& s = *e.spec;
        auto mat = lambertColor(s.color);
        auto dark = lambertColor(s.color * 0.55f);
        auto eye = basic(hexc(0x1d1428));
        float k = s.size;
        e.g = makeGroup();
        e.rig = makeGroup();
        e.g->add(e.rig);
        auto addEyes = [&](Object3D& head, float hw, float y, float z) {
            for (float sx : {-1.0f, 1.0f}) {
                auto ey = makeMesh(geo::box(0.06f * k, 0.08f * k, 0.02f), eye);
                ey->position = {sx * hw * 0.4f, y, z};
                head.add(ey);
            }
        };
        if (s.slime) {
            auto body = makeMesh(geo::sphere(k * 0.5f, 12, 8), mat);
            body->scale = {1, 0.7f, 1};
            body->position.y = k * 0.35f;
            e.rig->add(body);
            addEyes(*body, k, k * 0.1f, -k * 0.48f);
            e.head = e.rig;
            e.height = k * 0.7f;
            return;
        }
        if (!s.biped) {
            // four legs (wolf)
            auto body = makeMesh(geo::box(0.45f * k, 0.4f * k, 0.9f * k), mat);
            body->position.y = 0.55f * k;
            auto head = makeMesh(geo::box(0.38f * k, 0.36f * k, 0.4f * k), mat);
            head->position = {0, 0.75f * k, -0.58f * k};
            addEyes(*head, 0.38f * k, 0.05f * k, -0.2f * k - 0.01f);
            e.rig->add(body, head);
            for (glm::vec2 l : {glm::vec2(-1, -1), glm::vec2(1, -1), glm::vec2(-1, 1), glm::vec2(1, 1)}) {
                auto g = geo::box(0.12f * k, 0.4f * k, 0.12f * k);
                g->translate(0, 0.2f * k, 0);
                auto leg = makeMesh(g, dark);
                leg->position = {l.x * 0.15f * k, 0, l.y * 0.32f * k};
                e.g->add(leg);
                e.legs.push_back(leg);
            }
            e.head = head;
            e.height = 0.95f * k;
            return;
        }
        // two legs
        auto body = makeMesh(geo::box(0.42f * k, 0.38f * k, 0.3f * k), mat);
        body->position.y = 0.48f * k;
        auto head = makeMesh(geo::box(0.34f * k, 0.3f * k, 0.3f * k), mat);
        head->position.y = 0.84f * k;
        addEyes(*head, 0.34f * k, 0.02f * k, -0.15f * k - 0.01f);
        e.rig->add(body, head);
        if (s.flying) {
            // wings instead of arms
            for (float sx : {-1.0f, 1.0f}) {
                auto w = makeMesh(geo::box(0.5f * k, 0.03f, 0.3f * k), dark);
                w->position = {sx * 0.45f * k, 0.55f * k, 0};
                e.rig->add(w);
            }
        }
        for (float sx : {-1.0f, 1.0f}) {
            auto g = geo::box(0.13f * k, 0.3f * k, 0.15f * k);
            g->translate(0, 0.15f * k, 0);
            auto leg = makeMesh(g, dark);
            leg->position = {sx * 0.11f * k, 0, 0};
            e.g->add(leg);
            e.legs.push_back(leg);
        }
        e.head = head;
        e.height = k;
    }

    void buildSword() {
        pivot = makeGroup("sword");
        pivot->position = {0, 0.75f, 0};
        // the blade points forward (+Z, the facing direction) once the arm is tipped over
        auto arm = makeGroup();
        arm->rotation.x = PI / 2;
        pivot->add(arm);
        auto& cw = cubeWorld();
        if (cw.available() && cw.has("iron-sword")) {
            CwTag tag;
            tag.name = "iron-sword";
            tag.h = 0.95f;
            sword = cw.mesh("iron-sword", tag);
        } else {
            sword = makeGroup();
            auto steel = standard(hexc(0xd8dde6), 0.35f, 0.8f);
            auto wood = lambertColor(hexc(0x6e4a32));
            auto blade = makeMesh(geo::box(0.07f, 0.7f, 0.02f), steel);
            blade->position.y = 0.55f;
            auto guard = makeMesh(geo::box(0.24f, 0.05f, 0.06f), wood);
            guard->position.y = 0.2f;
            auto grip = makeMesh(geo::box(0.05f, 0.2f, 0.05f), wood);
            grip->position.y = 0.1f;
            sword->add(blade, guard, grip);
        }
        sword->position.y = 0.2f;
        sword->traverse([](Object3D& o) {
            if (o.kind == ObjectKind::Mesh) o.castShadow = true;
        });
        arm->add(sword);
        pivot->visible = false;
        player.body().add(pivot);

        // the swoosh: a thin ring arc lying flat around the player
        auto ag = geo::torus(1.45f, 0.05f, 3, 24, 2.3f);
        ag->rotateX(PI / 2);
        auto am = basic(hexc(0xfff3d6));
        am->transparent = true;
        am->blending = Blending::Additive;
        am->depthWrite = false;
        am->opacity = 0;
        arc = makeMesh(ag, am);
        arc->position.y = 0.75f;
        arc->visible = false;
        world.addObject(arc);
    }

    void spawnAll() {
        Rng rand(31337);
        for (auto& camp : camps()) {
            const Spec& s = spec(camp.kind);
            for (int i = 0; i < camp.n; ++i) {
                auto e = std::make_unique<Enemy>();
                e->spec = &s;
                // a dry spot inside the camp
                glm::vec2 at = camp.at;
                for (int tries = 0; tries < 40; ++tries) {
                    float a = rand.f() * PI * 2, d = rand.f() * camp.r;
                    glm::vec2 p = camp.at + glm::vec2(std::cos(a), std::sin(a)) * d;
                    if (world.areaAt(p.x, p.y) == "wilds" && !world.isWater(p.x, p.y)) {
                        at = p;
                        break;
                    }
                }
                e->home = at;
                buildModel(*e);
                e->hp = s.hp;
                e->heading = rand.f() * PI * 2;
                e->phase = rand.f() * 10;
                e->wait = rand.f() * 3;
                e->g->position = {at.x, groundY(at.x, at.y, 99), at.y};
                world.addObject(e->g);
                enemies.push_back(std::move(e));
            }
        }
    }

    // ----------------------------------------------------------------------
    // helpers
    // ----------------------------------------------------------------------
    float groundY(float x, float z, float y) const {
        float g = world.groundAt(x, z, y);
        // monsters float in deep water instead of sinking (they never walk into it on purpose)
        return std::max(g, layout::WATER_Y - 0.3f);
    }

    bool alive(const Enemy& e) const { return e.state != State::Dying && e.state != State::Gone && e.g->visible; }

    // move by (dx, dz) with collisions; false when blocked by a ledge / water / the edge of the wilds
    bool step(Enemy& e, float dx, float dz) {
        glm::vec3& p = e.g->position;
        glm::vec3 tmp(p.x + dx, p.y, p.z + dz);
        world.resolveCollisions(tmp, e.spec->radius * 0.6f);
        if (world.areaAt(tmp.x, tmp.z) != "wilds") return false;
        if (!e.spec->flying) {
            if (world.isWater(tmp.x, tmp.z)) return false;
            if (world.groundAt(tmp.x, tmp.z, p.y) > p.y + 0.6f) return false;
        }
        float moved = std::hypot(tmp.x - p.x, tmp.z - p.z);
        p.x = tmp.x;
        p.z = tmp.z;
        return moved > std::hypot(dx, dz) * 0.3f;
    }

    bool walkTowards(Enemy& e, glm::vec2 target, float speed, float dt) {
        glm::vec2 d = target - e.pos();
        float l = glm::length(d);
        if (l < 0.05f) return true;
        float want = headingTo(e.pos(), target);
        e.heading += wrapAngle(want - e.heading) * std::min(1.0f, dt * 8);
        // slimes only move while in the air
        if (e.spec->slime && std::sin(e.phase) < 0) return false;
        float s = std::min(speed * dt, l);
        return step(e, -std::sin(e.heading) * s, -std::cos(e.heading) * s);
    }

    void animate(Enemy& e, float t, float dt, float speed) {
        e.g->rotation.y = e.heading;
        if (e.spec->slime) {
            // hop, squash on landing
            e.phase += dt * (speed > 0.05f ? 6 : 2.5f);
            float hop = std::max(0.0f, std::sin(e.phase));
            e.rig->position.y = hop * (speed > 0.05f ? 0.45f : 0.12f);
            float sq = 1 + (hop < 0.15f ? 0.15f : -0.05f);
            e.rig->scale = {sq, 2 - sq, sq};
            return;
        }
        e.phase += dt * (speed > 0.05f ? 6 + speed * 2.2f : 0);
        float sw = speed > 0.05f ? std::sin(e.phase) : 0;
        float k = std::min(1.0f, 0.4f + speed * 0.12f);
        e.rig->position.y = std::abs(sw) * 0.05f * k;
        for (size_t i = 0; i < e.legs.size(); ++i) e.legs[i]->position.y = std::max(0.0f, ((i == 0 || i == 3) ? sw : -sw) * 0.08f * k);
        if (e.head != e.rig) e.head->rotation.x = speed > 0.05f ? sw * 0.04f : std::sin(t * 0.7f + e.phase) * 0.12f;
    }

    void setGlow(Enemy& e, glm::vec3 c) {
        for (auto& [m, base] : e.mats) m->emissive = base + c;
    }

    void drawBar(Enemy& e) {
        int shown = (int)std::ceil(std::max(0.0f, e.hp));
        if (shown == e.barShown) return;
        e.barShown = shown;
        const Spec& s = *e.spec;
        Canvas2D c(256, 64);
        c.setFont("Inter", 22, 700);
        c.setTextAlign(TextAlign::Center);
        c.setTextBaseline(TextBaseline::Middle);
        c.setLineWidth(5);
        c.setStroke("rgba(20,14,36,0.85)");
        std::string label = "Lv " + std::to_string(s.level) + "  " + s.name;
        c.strokeText(label, 128, 18);
        c.setFill(s.stagger == 0 ? "#ffd76a" : "#fff6ea");
        c.fillText(label, 128, 18);
        // bar
        float k = std::clamp(e.hp / s.hp, 0.0f, 1.0f);
        c.setFill("rgba(20,14,36,0.85)");
        c.beginPath();
        c.roundRect(28, 38, 200, 16, 8);
        c.fill();
        if (k > 0) {
            c.beginPath();
            c.roundRect(31, 41, 194 * k, 10, 5);
            c.fillLinearGradient(0, 41, 0, 51, {{0, cssColor("#ff8a8a")}, {1, cssColor("#c8203a")}});
        }
        c.upload(*e.barTex);
    }

    // ----------------------------------------------------------------------
    // damage numbers
    // ----------------------------------------------------------------------
    void number(glm::vec3 p, const std::string& text, const std::string& color) {
        std::string key = text + "|" + color;
        auto& tex = numberTex[key];
        if (!tex) {
            Canvas2D c(160, 64);
            c.setFont("Mali", 44, 700);
            c.setTextAlign(TextAlign::Center);
            c.setTextBaseline(TextBaseline::Middle);
            c.setLineWidth(8);
            c.setStroke("rgba(20,14,36,0.9)");
            c.strokeText(text, 80, 34);
            c.setFill(color);
            c.fillText(text, 80, 34);
            tex = std::make_shared<Texture>();
            c.upload(*tex);
        }
        auto m = spriteMaterial(tex);
        m->depthTest = false;
        m->transparent = true;
        auto s = makeSprite(m);
        s->scale = {0.75f, 0.3f, 1};
        s->renderOrder = 19;
        p.x += (frand() - 0.5f) * 0.4f;
        s->position = p;
        world.addObject(s);
        numbers.push_back({s, p, now});
    }

    void updateNumbers() {
        for (size_t i = 0; i < numbers.size();) {
            Number& n = numbers[i];
            float k = (now - n.born) / 0.9f;
            if (k >= 1) {
                world.group->remove(n.s.get());
                numbers.erase(numbers.begin() + (long)i);
                continue;
            }
            n.s->position = n.p + glm::vec3(0, 0.8f * (1 - (1 - k) * (1 - k)), 0);
            // pop in, then fade
            float pop = k < 0.12f ? 0.6f + k / 0.12f * 0.5f : 1.1f - std::min(0.1f, (k - 0.12f));
            n.s->scale = glm::vec3(0.75f * pop, 0.3f * pop, 1);
            n.s->material()->opacity = k < 0.6f ? 1 : 1 - (k - 0.6f) / 0.4f;
            ++i;
        }
    }

    // ----------------------------------------------------------------------
    // the player's attacks
    // ----------------------------------------------------------------------
    Enemy* nearestEnemy(float maxD) {
        Enemy* best = nullptr;
        float bd = maxD;
        glm::vec2 pp(player.position.x, player.position.z);
        for (auto& e : enemies) {
            if (!alive(*e)) continue;
            float d = glm::length(e->pos() - pp);
            if (d < bd) {
                bd = d;
                best = e.get();
            }
        }
        return best;
    }

    void startSwing(int stepN, bool spin) {
        swing = {};
        swing.on = true;
        swing.spin = spin;
        swing.step = stepN;
        swing.dur = spin ? 0.5f : stepN == 3 ? 0.42f : 0.3f;
        lastStep = stepN;
        // aim at the closest monster
        if (Enemy* e = nearestEnemy(4.5f)) {
            glm::vec2 d = e->pos() - glm::vec2(player.position.x, player.position.z);
            player.face(std::atan2(d.x, d.y));
        }
        pivot->visible = true;
        if (self->onSwing) self->onSwing();
    }

    void hitEnemies() {
        glm::vec2 pp(player.position.x, player.position.z);
        float base = 9 + 3.0f * (self->level - 1);
        float mult = swing.spin ? 1.3f : swing.step == 3 ? 1.8f : swing.step == 2 ? 1.1f : 1.0f;
        float reach = swing.spin ? 2.8f : swing.step == 3 ? 2.5f : 2.1f;
        bool any = false;
        for (auto& ep : enemies) {
            Enemy& e = *ep;
            if (!alive(e)) continue;
            glm::vec2 d = e.pos() - pp;
            float dist = glm::length(d);
            if (dist > reach + e.spec->radius) continue;
            float midY = e.g->position.y + e.height * 0.5f;
            if (std::abs(midY - (player.position.y + 0.8f)) > 2.2f) continue;
            if (!swing.spin && dist > 0.6f && std::abs(wrapAngle(std::atan2(d.x, d.y) - player.yaw)) > 1.25f) continue;
            bool crit = frand() < 0.12f;
            float dmg = base * mult * levelMod(self->level - e.spec->level) * (crit ? 1.6f : 1) * (0.9f + frand() * 0.2f);
            damageEnemy(e, dmg, crit, swing.spin || swing.step == 3 || crit, dist > 1e-3f ? d / dist : glm::vec2(0, 1));
            any = true;
        }
        if (any) lastCombat = now;
    }

    // heavy = combo finisher / spin / crit: the only hits that break a bigger monster's wind-up
    void damageEnemy(Enemy& e, float dmg, bool crit, bool heavy, glm::vec2 dir) {
        e.hp -= dmg;
        e.flash = 1;
        glm::vec3 head = e.g->position + glm::vec3(0, e.height + 0.3f, 0);
        number(head, std::to_string((int)std::round(dmg)), crit ? "#ffd76a" : "#ffffff");
        if (self->onHit) self->onHit(head, crit);
        if (e.hp <= 0) {
            kill(e);
            return;
        }
        // get knocked back and interrupted (the boss shrugs it off)
        if (e.spec->stagger > 0) {
            e.knock = dir * (heavy ? 6.0f : 2.5f) * e.spec->stagger;
            bool breaks = heavy || e.spec->stagger >= 1 || e.state != State::Windup;
            if (breaks && (e.state == State::Windup || e.state == State::Idle || e.state == State::Chase)) {
                e.state = State::Recover;
                e.timer = 0.3f;
            }
        } else if (e.state == State::Idle) {
            e.state = State::Chase;
        }
        if (e.state == State::Return) e.state = State::Chase;
    }

    void kill(Enemy& e) {
        e.state = State::Dying;
        e.timer = 0.7f;
        e.bar->visible = false;
        self->kills++;
        glm::vec3 p = e.g->position + glm::vec3(0, e.height * 0.5f, 0);
        if (self->onKill) self->onKill(e.spec->kind, p, e.spec->color);
        gainXp(e.spec->xp);
        number(p + glm::vec3(0, e.height * 0.5f + 0.6f, 0), "+" + std::to_string(e.spec->xp) + " XP", "#8affc1");
    }

    void gainXp(int n) {
        self->xp += n;
        while (self->xp >= self->xpNext()) {
            self->xp -= self->xpNext();
            self->level++;
            self->maxHp = maxHpAt(self->level);
            self->hp = self->maxHp;
            if (self->onLevelUp) self->onLevelUp(self->level);
        }
        if (self->onProgress) self->onProgress();
    }

    void updateSwing(float dt) {
        if (swing.on) {
            swing.t += dt;
            float k = std::min(1.0f, swing.t / swing.dur);
            // ease-out sweep
            float e = 1 - (1 - k) * (1 - k);
            if (swing.spin) {
                pivot->rotation = {0, -1.2f + e * PI * 2, 0};
            } else if (swing.step == 3) {
                // overhead chop
                pivot->rotation = {-1.4f + e * 1.9f, 0, 0};
            } else {
                float a = swing.step == 1 ? 1.6f - e * 3.0f : -1.4f + e * 3.0f;
                pivot->rotation = {0.15f, a, 0};
            }
            if (!swing.hitDone && swing.t >= swing.dur * 0.38f) {
                swing.hitDone = true;
                hitEnemies();
                arcT = now;
                arcFull = swing.spin;
                arcYaw = player.yaw;
            }
            if (swing.t >= swing.dur) {
                swing.on = false;
                comboUntil = now + 0.45f;
                if (swing.queued && !swing.spin && swing.step < 3) startSwing(swing.step + 1, false);
            }
            player.moveScale = swing.on ? (swing.spin ? 0.6f : 0.35f) : 1;
        } else {
            player.moveScale = 1;
            // put the sword away a moment after the last swing
            if (now > comboUntil + 0.8f) pivot->visible = false;
            pivot->rotation = {0.3f, 0.6f, 0};
        }
        // the swoosh
        float ak = (now - arcT) / 0.18f;
        arc->visible = ak >= 0 && ak < 1;
        if (arc->visible) {
            arc->position = player.position + glm::vec3(0, 0.75f, 0);
            arc->material()->opacity = 0.75f * (1 - ak);
            // the arc geometry spans 2.3 rad from +X; centre it on the facing direction
            arc->rotation.y = arcYaw - PI / 2 + 1.15f;
            arc->scale = arcFull ? glm::vec3(1.25f, 1, 1.25f) * (0.9f + ak * 0.2f) : glm::vec3(0.9f + ak * 0.2f);
            if (arcFull) arc->rotation.y += ak * 6;
        }
    }

    // ----------------------------------------------------------------------
    // monsters' turn
    // ----------------------------------------------------------------------
    void hurtPlayer(Enemy& e) {
        float dmg = e.spec->dmg * levelMod(e.spec->level - self->level) * (0.9f + frand() * 0.2f);
        self->hp = std::max(0.0f, self->hp - dmg);
        lastCombat = lastHurt = now;
        glm::vec2 d = glm::vec2(player.position.x, player.position.z) - e.pos();
        float l = glm::length(d);
        if (l > 1e-3f) d /= l;
        player.velocity.x += d.x * 5;
        player.velocity.z += d.y * 5;
        number(player.position + glm::vec3(0, 1.9f, 0), std::to_string((int)std::round(dmg)), "#ff6b6b");
        if (self->onHurt) self->onHurt(dmg);
        if (self->hp <= 0) {
            swing.on = false;
            for (auto& o : enemies)
                if (alive(*o)) o->state = State::Return;
            if (self->onDeath) self->onDeath();
        }
    }

    void updateEnemy(Enemy& e, float t, float dt, bool canFight, bool night) {
        const Spec& s = *e.spec;
        glm::vec2 pp(player.position.x, player.position.z);
        bool present = !s.night || night;

        if (e.state == State::Gone) {
            e.respawn -= dt;
            // come back out of sight, at night only for the night shift
            if (e.respawn <= 0 && present && glm::length(pp - e.home) > 30) {
                e.state = State::Idle;
                e.hp = s.hp;
                e.barShown = -1;
                e.g->scale = glm::vec3(1);
                e.g->position = {e.home.x, groundY(e.home.x, e.home.y, 99), e.home.y};
                e.g->visible = true;
            }
            return;
        }
        if (e.state == State::Dying) {
            e.timer -= dt;
            float k = 1 - std::max(0.0f, e.timer / 0.7f);
            e.g->scale = glm::vec3(1 - k * 0.9f);
            e.g->rotation.z = k * 1.2f;
            setGlow(e, glm::vec3(1) * (1 - k));
            if (e.timer <= 0) {
                e.state = State::Gone;
                e.g->visible = false;
                e.g->rotation.z = 0;
                e.respawn = 45;
            }
            return;
        }
        // the night shift goes home at dawn (when nobody is fighting it)
        if (!present && (e.state == State::Idle || e.state == State::Return)) {
            e.g->visible = false;
            e.bar->visible = false;
            return;
        }
        if (present && !e.g->visible) {
            e.g->visible = true;
            e.state = State::Idle;
            e.hp = s.hp;
        }
        float dist = glm::length(pp - e.pos());
        // skip the far ones (no one sees them)
        if (dist > 90 && e.state == State::Idle) return;

        bool targetable = canFight && !self->dead() && world.areaAt(pp.x, pp.y) == "wilds";
        float fromHome = glm::length(e.pos() - e.home);
        float speed = 0;
        e.cool -= dt;

        switch (e.state) {
        case State::Idle: {
            if (e.wait > 0) {
                e.wait -= dt;
            } else {
                if (!e.hasWander) {
                    float a = frand() * PI * 2, r = frand() * 4;
                    e.wander = e.home + glm::vec2(std::cos(a), std::sin(a)) * r;
                    e.hasWander = true;
                }
                speed = s.speed * 0.3f;
                bool stuck = !walkTowards(e, e.wander, speed, dt) && !s.slime;
                if (glm::length(e.wander - e.pos()) < 0.4f || stuck) {
                    e.hasWander = false;
                    e.wait = 1.5f + frand() * 3;
                }
            }
            float aggro = s.aggro * (night ? 1.2f : 1);
            if (targetable && dist < aggro && std::abs(player.position.y - e.g->position.y) < 4) {
                e.state = State::Chase;
                e.hasWander = false;
            }
            e.hp = std::min(s.hp, e.hp + s.hp * 0.1f * dt);
            break;
        }
        case State::Chase:
            if (!targetable || fromHome > LEASH) {
                e.state = State::Return;
                break;
            }
            if (dist <= s.reach + 0.3f && e.cool <= 0) {
                e.state = State::Windup;
                e.timer = s.windup;
                break;
            }
            if (dist > s.reach * 0.8f) {
                speed = s.speed;
                walkTowards(e, pp, speed, dt);
            } else {
                e.heading += wrapAngle(headingTo(e.pos(), pp) - e.heading) * std::min(1.0f, dt * 8);
            }
            break;
        case State::Windup: {
            e.heading += wrapAngle(headingTo(e.pos(), pp) - e.heading) * std::min(1.0f, dt * 6);
            e.timer -= dt;
            // red telegraph, rearing back
            float k = 1 - std::max(0.0f, e.timer / s.windup);
            setGlow(e, glm::vec3(0.9f, 0.1f, 0.1f) * (0.3f + 0.5f * std::abs(std::sin(t * 18))) * k);
            e.rig->rotation.x = -0.25f * k;
            if (e.timer <= 0) {
                e.rig->rotation.x = 0.3f;
                // a quick lunge
                step(e, -std::sin(e.heading) * 0.35f, -std::cos(e.heading) * 0.35f);
                float dy = std::abs(player.position.y + 0.8f - (e.g->position.y + e.height * 0.5f));
                bool dodged = t < rollUntil;
                if (targetable && dist <= s.reach + 0.7f && dy < 2.0f && !dodged) hurtPlayer(e);
                else if (targetable && dodged && dist <= s.reach + 0.7f) number(player.position + glm::vec3(0, 1.9f, 0), "MISS", "#b9c4ff");
                e.state = State::Recover;
                e.timer = 0.35f;
                e.cool = s.cooldown;
            }
            break;
        }
        case State::Recover:
            e.timer -= dt;
            e.rig->rotation.x *= std::max(0.0f, 1 - dt * 8);
            if (e.timer <= 0) e.state = targetable && fromHome < LEASH ? State::Chase : State::Return;
            break;
        case State::Return:
            speed = s.speed * 0.8f;
            if (walkTowards(e, e.home, speed, dt) || glm::length(e.home - e.pos()) < 1.2f) e.state = State::Idle;
            // stuck on the way home: just pop back
            if (fromHome > LEASH * 2) e.g->position = {e.home.x, groundY(e.home.x, e.home.y, 99), e.home.y};
            e.hp = std::min(s.hp, e.hp + s.hp * 0.25f * dt);
            if (targetable && dist < s.aggro * 0.5f && fromHome < LEASH * 0.5f) e.state = State::Chase;
            break;
        default:
            break;
        }
        if (e.state != State::Windup && e.state != State::Recover) e.rig->rotation.x *= std::max(0.0f, 1 - dt * 8);

        // knockback
        if (glm::length(e.knock) > 0.05f) {
            step(e, e.knock.x * dt, e.knock.y * dt);
            e.knock *= std::exp(-9 * dt);
        }
        // keep a little apart from the player
        if (dist < s.radius + 0.35f && dist > 1e-3f) {
            glm::vec2 push = (e.pos() - pp) / dist * (s.radius + 0.35f - dist);
            step(e, push.x, push.y);
        }

        // height: walk on the ground, flyers bob above it (and dive while striking)
        glm::vec3& p = e.g->position;
        float gy = groundY(p.x, p.z, p.y + 0.5f);
        if (s.flying) {
            float want = gy + 1.5f + std::sin(t * 3 + e.phase) * 0.2f;
            if (e.state == State::Windup || e.state == State::Recover) want = std::max(gy + 0.5f, player.position.y + 0.6f);
            p.y += (want - p.y) * std::min(1.0f, dt * 4);
        } else {
            p.y += (gy - p.y) * std::min(1.0f, dt * 14);
        }
        animate(e, t, dt, speed);

        // hit flash
        if (e.state != State::Windup) {
            e.flash = std::max(0.0f, e.flash - dt * 6);
            setGlow(e, glm::vec3(1) * e.flash);
        }
        // health bar while fighting or hurt
        bool showBar = e.state != State::Idle || e.hp < s.hp - 0.5f;
        e.bar->visible = showBar && dist < 40;
        if (e.bar->visible) {
            drawBar(e);
            e.bar->position = p + glm::vec3(0, e.height + 0.55f, 0);
        }
    }

    // monsters do not stand inside each other
    void separate() {
        for (size_t i = 0; i < enemies.size(); ++i)
            for (size_t j = i + 1; j < enemies.size(); ++j) {
                Enemy &a = *enemies[i], &b = *enemies[j];
                if (!alive(a) || !alive(b)) continue;
                glm::vec2 d = b.pos() - a.pos();
                float l = glm::length(d), m = a.spec->radius + b.spec->radius;
                if (l >= m || l < 1e-4f) continue;
                glm::vec2 push = d / l * (m - l) * 0.5f;
                step(a, -push.x, -push.y);
                step(b, push.x, push.y);
            }
    }
};

// ---------------------------------------------------------------------------

Combat::Combat(Overworld& world, Player& player, int lv, int x) : level(std::max(1, lv)), xp(std::max(0, x)), d(std::make_unique<Impl>(world, player)) {
    d->self = this;
    maxHp = maxHpAt(level);
    hp = maxHp;
    d->buildSword();
    d->spawnAll();
}

Combat::~Combat() = default;

bool Combat::inCombat() const { return d->now - d->lastCombat < REGEN_DELAY; }

float Combat::specialReady() const { return std::clamp((d->now - d->specialAt) / 4.0f, 0.0f, 1.0f); }

void Combat::attack() {
    if (dead() || d->player.dashing()) return;
    if (d->swing.on) {
        // buffer the next hit of the combo
        if (!d->swing.spin && d->swing.t > d->swing.dur * 0.35f) d->swing.queued = true;
        return;
    }
    int next = d->now < d->comboUntil && d->lastStep < 3 ? d->lastStep + 1 : 1;
    d->startSwing(next, false);
}

void Combat::special() {
    if (dead() || d->swing.on || specialReady() < 1) return;
    d->specialAt = d->now;
    d->startSwing(0, true);
}

void Combat::roll(glm::vec2 dir) {
    if (dead() || d->now - d->rollAt < 0.7f || !d->player.grounded) return;
    d->rollAt = d->now;
    d->rollUntil = d->now + 0.4f;
    d->swing.on = false;
    d->player.dash(dir, 9.5f, 0.32f);
}

void Combat::revive() {
    hp = maxHp;
    d->lastCombat = -100;
    for (auto& e : d->enemies)
        if (d->alive(*e)) e->state = Impl::State::Return;
}

void Combat::update(float t, float dt, bool canFight, bool night) {
    d->now = t;
    if (!canFight) {
        d->swing.on = false;
        d->player.moveScale = 1;
    }
    d->updateSwing(dt);
    for (auto& e : d->enemies) d->updateEnemy(*e, t, dt, canFight, night);
    d->separate();
    d->updateNumbers();
    // health comes back out of combat
    if (!dead() && !inCombat()) hp = std::min(maxHp, hp + maxHp * 0.08f * dt);
    // flash red when hurt
    d->player.tint(glm::vec3(1, 0.25f, 0.25f), std::max(0.0f, 1 - (t - d->lastHurt) / 0.25f));
}
