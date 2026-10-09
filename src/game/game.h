#pragma once
// Game orchestrator — port of main.js: realms, player, companions, camera, interactions, HUD, saving.
#include <memory>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "scene/renderer.h"

struct GLFWwindow;
class World;
class WinterWorld;
class Player;

struct LaunchOptions {
    // empty = the character chosen with the secret code (saved)
    std::string character;
    std::string time;
    std::string shotPath;
    float shotDelay = 0;
    // scripted keys for --shot runs: w walk, r run, j jump, g glide, u jetpack
    std::string autoKeys;
    bool hasPos = false;
    glm::vec2 pos{0};
    bool hasCam = false;
    glm::vec3 cam{0};
    bool skipTitle = false;
    // keep the title screen in --shot runs
    bool forceTitle = false;
    std::string realm;
    // multiplayer relay "host:port" (server.py); empty = offline
    std::string server;
};

class Game {
public:
    explicit Game(const LaunchOptions& opt);
    ~Game();
    int run();

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
