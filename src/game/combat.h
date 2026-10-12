#pragma once
// Fighting in the wilds, after Cube World (new — not in the web build): monsters with levels and camps,
// a three-hit sword combo, a spin attack, a dodge roll, floating damage numbers, XP and levels.
// The towns stay peaceful: monsters never step out of the wilds and give up when you walk into a town.
// Uses the Cube World voxel monsters when the assets are there, simple low-poly stand-ins otherwise.
#include <functional>
#include <memory>
#include <string>
#include <glm/glm.hpp>

class Overworld;
class Player;

class Combat {
public:
    Combat(Overworld& world, Player& player, int level, int xp);
    ~Combat();

    // ---------- the player's side ----------
    int level, xp;
    float hp, maxHp;
    int kills = 0;
    int xpNext() const { return 50 * level; }
    static float maxHpAt(int level) { return 100 + 15.0f * (level - 1); }
    bool dead() const { return hp <= 0; }
    // a hit given or taken in the last few seconds (no health regeneration while true)
    bool inCombat() const;
    // 0..1, 1 = the spin attack is ready
    float specialReady() const;

    // ---------- actions ----------
    // sword combo: each press inside the window continues the chain (1 -> 2 -> 3)
    void attack();
    // spin attack: hits everything around you, long cooldown
    void special();
    // dodge roll towards dir (world x, z; zero = forward), invulnerable while rolling
    void roll(glm::vec2 dir);
    // back to full health (after respawning); every monster forgets you
    void revive();

    // canFight = the player can swing and be hit (on foot or riding, alive, not asleep)
    // night = night monsters (skeletons, bats) are out
    void update(float t, float dt, bool canFight, bool night);

    // ---------- events: sounds, HUD and rewards live in the game ----------
    std::function<void()> onSwing;
    std::function<void(glm::vec3 pos, bool crit)> onHit;
    std::function<void(float damage)> onHurt;
    std::function<void()> onDeath;
    std::function<void(const std::string& kind, glm::vec3 pos, glm::vec3 color)> onKill;
    std::function<void(int level)> onLevelUp;
    // xp / level changed (save it)
    std::function<void()> onProgress;

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
