#pragma once
// Multiplayer — port of net.js: see the other player in the same world through the /ws relay of server.py
// (same protocol as the web build, so web and desktop players can meet). State is sent ~12 times a second;
// others are drawn with a name tag, glider / mount / boat, jetpack and the companions they have found.
// Offline when no server is given (like opening the web page as a file).
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "scene/object.h"
#include "game/player.h"

class World;

struct NetState {
    std::string realm;
    glm::vec3 p{0};
    float yaw = 0;
    bool grounded = true;
    // 0 off, 1 worn on the ground, 1.5 drifting down, 2 thrusting
    float jp = 0;
    // "", "sleep", "glide", "boat", or the ridden animal kind
    std::string v;
    bool hasM = false;
    // x, y, z, heading of the boat / mount / bed
    glm::vec4 m{0};
    std::vector<std::string> pets;
};

struct NetMakers {
    std::function<ObjPtr()> glider;
    std::function<ObjPtr(const std::string&)> mount;
    std::function<ObjPtr()> boat;
};

class Multiplayer {
public:
    // server = "host:port" ("" = offline)
    Multiplayer(Object3D& scene, const std::string& character, const std::string& server, NetMakers make);
    ~Multiplayer();
    // getState returns false while there is nothing to send (title screen / changing realm)
    std::function<bool(NetState&)> getState;
    std::function<void(const std::string&)> onJoin, onLeave;
    // toGlobal: turns a peer's (realm, local position, local yaw) into this world's global frame; false = not shown
    using Placer = std::function<bool(const std::string& realm, glm::vec3& p, float& yaw)>;
    void update(float t, float dt, const World& world, const Placer& toGlobal);
    int count() const;
    bool connected() const;

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
