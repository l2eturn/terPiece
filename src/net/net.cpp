// Port of net.js — with a tiny WebSocket client (RFC 6455 text frames) on POSIX sockets.
#include "net/net.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <deque>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <thread>
#include "core/math.h"
#include "game/pets.h"
#include "gfx/texture.h"
#include "ui/canvas2d.h"
#include "world/world.h"

using nlohmann::json;

namespace {

const std::map<std::string, std::string> NAMES = {{"patty", "Patty"}, {"ter", "Ter"}, {"momonga", "Momonga"}};
constexpr float SEND_EVERY = 1.0f / 12;

std::string base64(const uint8_t* p, size_t n) {
    static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (size_t i = 0; i < n; i += 3) {
        uint32_t v = p[i] << 16 | (i + 1 < n ? p[i + 1] << 8 : 0) | (i + 2 < n ? p[i + 2] : 0);
        out += T[v >> 18 & 63];
        out += T[v >> 12 & 63];
        out += i + 1 < n ? T[v >> 6 & 63] : '=';
        out += i + 2 < n ? T[v & 63] : '=';
    }
    return out;
}

// name tag sprite above other players
ObjPtr nameTag(const std::string& text) {
    Canvas2D g(256, 64);
    g.setFont("Mali", 36, 600);
    float w = std::min(250.0f, g.measureText(text) + 32);
    g.setFill("rgba(42,29,74,0.72)");
    g.beginPath();
    g.roundRect((256 - w) / 2, 8, w, 48, 24);
    g.fill();
    g.setFill("#fff6ea");
    g.setTextAlign(TextAlign::Center);
    g.setTextBaseline(TextBaseline::Middle);
    g.fillText(text, 128, 33);
    auto tex = std::make_shared<Texture>();
    g.upload(*tex);
    auto m = spriteMaterial(tex);
    m->depthTest = false;
    auto s = makeSprite(m);
    s->scale = {1.2f, 0.3f, 1};
    s->renderOrder = 19;
    return s;
}

// blocking WebSocket connection driven by a background thread
class Socket {
public:
    bool open(const std::string& host, const std::string& port) {
        addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0) return false;
        for (addrinfo* a = res; a; a = a->ai_next) {
            fd_ = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
            if (fd_ < 0) continue;
            if (connect(fd_, a->ai_addr, a->ai_addrlen) == 0) break;
            ::close(fd_);
            fd_ = -1;
        }
        freeaddrinfo(res);
        if (fd_ < 0) return false;
        int one = 1;
        setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
        setsockopt(fd_, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
        // handshake
        uint8_t key[16];
        for (auto& k : key) k = (uint8_t)(frand() * 256);
        std::string req = "GET /ws HTTP/1.1\r\nHost: " + host + ":" + port +
                          "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: " + base64(key, 16) +
                          "\r\nSec-WebSocket-Version: 13\r\n\r\n";
        if (!sendAll(req.data(), req.size())) return fail();
        std::string resp;
        char c;
        while (resp.find("\r\n\r\n") == std::string::npos) {
            if (::recv(fd_, &c, 1, 0) != 1) return fail();
            resp += c;
            if (resp.size() > 8192) return fail();
        }
        if (resp.find(" 101 ") == std::string::npos) return fail();
        return true;
    }
    void close() {
        if (fd_ >= 0) {
            ::shutdown(fd_, SHUT_RDWR);
            ::close(fd_);
        }
        fd_ = -1;
    }
    bool isOpen() const { return fd_ >= 0; }
    // client frames must be masked
    bool sendText(const std::string& s) {
        std::lock_guard<std::mutex> g(sendLock_);
        if (fd_ < 0) return false;
        std::vector<uint8_t> f;
        f.push_back(0x81);
        size_t n = s.size();
        if (n < 126) {
            f.push_back((uint8_t)(0x80 | n));
        } else if (n < 65536) {
            f.push_back(0x80 | 126);
            f.push_back((uint8_t)(n >> 8));
            f.push_back((uint8_t)n);
        } else {
            f.push_back(0x80 | 127);
            for (int i = 7; i >= 0; --i) f.push_back((uint8_t)((uint64_t)n >> (i * 8)));
        }
        uint8_t mask[4];
        for (auto& m : mask) m = (uint8_t)(frand() * 256);
        f.insert(f.end(), mask, mask + 4);
        for (size_t i = 0; i < n; ++i) f.push_back((uint8_t)s[i] ^ mask[i % 4]);
        return sendAll(f.data(), f.size());
    }
    // reads one message; false when the connection dropped
    bool recvText(std::string& out) {
        while (true) {
            uint8_t h[2];
            if (!recvAll(h, 2)) return false;
            int op = h[0] & 0x0f;
            uint64_t n = h[1] & 0x7f;
            if (n == 126) {
                uint8_t b[2];
                if (!recvAll(b, 2)) return false;
                n = b[0] << 8 | b[1];
            } else if (n == 127) {
                uint8_t b[8];
                if (!recvAll(b, 8)) return false;
                n = 0;
                for (int i = 0; i < 8; ++i) n = n << 8 | b[i];
            }
            uint8_t mask[4] = {0, 0, 0, 0};
            if (h[1] & 0x80 && !recvAll(mask, 4)) return false;
            std::string data(n, '\0');
            if (n && !recvAll((uint8_t*)data.data(), n)) return false;
            for (size_t i = 0; i < n; ++i) data[i] ^= mask[i % 4];
            if (op == 0x8) return false;
            if (op == 0x1) {
                out = std::move(data);
                return true;
            }
        }
    }

private:
    bool fail() {
        close();
        return false;
    }
    bool sendAll(const void* p, size_t n) {
        const char* c = (const char*)p;
        while (n) {
            ssize_t k = ::send(fd_, c, n, 0);
            if (k <= 0) return false;
            c += k;
            n -= (size_t)k;
        }
        return true;
    }
    bool recvAll(uint8_t* p, size_t n) {
        while (n) {
            ssize_t k = ::recv(fd_, p, n, 0);
            if (k <= 0) return false;
            p += k;
            n -= (size_t)k;
        }
        return true;
    }
    int fd_ = -1;
    std::mutex sendLock_;
};

}  // namespace

struct Multiplayer::Impl {
    Object3D& scene;
    std::string character, host, port;
    NetMakers make;
    Socket sock;
    std::thread thread;
    std::atomic<bool> running{true}, open{false};
    std::mutex inLock;
    std::deque<std::string> inbox;
    int myId = -1;
    float sendTimer = 0;

    struct PetView {
        std::unique_ptr<CritterRig> rig;
        std::string key;
    };
    struct Peer {
        int id;
        std::string kind;
        ObjPtr root;
        std::unique_ptr<SpriteActor> actor;
        ObjPtr glider, tag, mount, boat;
        std::string mountKind;
        Jetpack jet;
        bool hasJet = false;
        // position as sent (sender's realm coordinates) and its global version
        glm::vec3 local{0}, target{0};
        float yaw = 0, phase = 0, speed = 0;
        json state;
        std::map<std::string, PetView> pets;
        bool fresh = true;
    };
    std::map<int, std::unique_ptr<Peer>> peers;

    Impl(Object3D& s) : scene(s) {}

    void loop() {
        float retry = 2;
        while (running) {
            if (sock.open(host, port)) {
                open = true;
                retry = 2;
                std::string msg;
                while (running && sock.recvText(msg)) {
                    std::lock_guard<std::mutex> g(inLock);
                    inbox.push_back(std::move(msg));
                }
                open = false;
                sock.close();
                std::lock_guard<std::mutex> g(inLock);
                inbox.push_back("{\"t\":\"__closed\"}");
            }
            // reconnect with back-off
            for (int i = 0; i < (int)(retry * 10) && running; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
            retry = std::min(15.0f, retry * 1.5f);
        }
    }

    Peer& addPeer(const json& msg, Multiplayer& self) {
        auto p = std::make_unique<Peer>();
        std::string c = msg.value("char", "patty");
        p->kind = NAMES.count(c) ? c : "patty";
        p->id = msg.value("id", -1);
        p->root = makeGroup("peer");
        p->actor = std::make_unique<SpriteActor>(p->kind, 0.06f);
        p->root->add(p->actor->object);
        p->tag = nameTag(NAMES.at(p->kind));
        p->tag->position.y = 1.85f;
        p->root->add(p->tag);
        // Momonga glides with its own membrane
        if (p->kind != "momonga" && make.glider) p->glider = make.glider();
        if (p->glider) {
            p->glider->position.y = 2.15f;
            p->glider->visible = false;
            p->root->add(p->glider);
        }
        scene.add(p->root);
        p->jet = buildJetpack();
        p->hasJet = true;
        p->jet.group->visible = false;
        p->root->add(p->jet.group);
        if (msg.contains("p")) p->local = p->target = {msg["p"][0], msg["p"][1], msg["p"][2]};
        p->yaw = msg.value("yaw", 0.0f);
        p->state = msg;
        p->root->position = p->target;
        Peer& ref = *p;
        peers[p->id] = std::move(p);
        if (self.onJoin) self.onJoin(NAMES.at(ref.kind));
        return ref;
    }

    void removePeer(int id, bool silent, Multiplayer& self) {
        auto it = peers.find(id);
        if (it == peers.end()) return;
        Peer& p = *it->second;
        scene.remove(p.root.get());
        if (p.mount) scene.remove(p.mount.get());
        if (p.boat) scene.remove(p.boat.get());
        for (auto& [k, pet] : p.pets) scene.remove(pet.rig->object().get());
        std::string name = NAMES.at(p.kind);
        peers.erase(it);
        if (!silent && self.onLeave) self.onLeave(name);
    }

    void receive(const json& msg, Multiplayer& self) {
        int id = msg.value("id", -1);
        auto it = peers.find(id);
        Peer& p = it != peers.end() ? *it->second : addPeer(msg, self);
        p.state = msg;
        if (msg.contains("p")) p.local = {msg["p"][0], msg["p"][1], msg["p"][2]};
        // companions they have found
        if (msg.contains("pets"))
            for (auto& k : msg["pets"]) {
                std::string key = k;
                if (p.pets.count(key)) continue;
                PetView pv{std::make_unique<CritterRig>(key), key};
                pv.rig->object()->position = p.target;
                scene.add(pv.rig->object());
                p.pets[key] = std::move(pv);
            }
    }

    void updatePets(Peer& p, float t, float dt, const World& world, bool visible) {
        float yaw = p.yaw, fx = std::sin(yaw), fz = std::cos(yaw);
        glm::vec3 base = p.root->position;
        int slot = 0;
        for (auto& [key, pet] : p.pets) {
            auto o = pet.rig->object();
            o->visible = visible;
            if (!visible) continue;
            std::string state = "perch";
            if (key == "juice") {
                // on the shoulder
                o->position = {base.x - fz * 0.42f, base.y + 0.92f, base.z + fx * 0.42f};
                o->rotation.y = yaw;
            } else if (key == "cham") {
                // circling around
                float a = t * 1.4f;
                glm::vec3 tmp(base.x - fx * 1.1f + std::cos(a) * 0.9f, base.y + 1.7f + std::sin(t * 3) * 0.2f, base.z - fz * 1.1f + std::sin(a) * 0.9f);
                float px = o->position.x, pz = o->position.z;
                o->position = glm::mix(o->position, tmp, std::min(1.0f, dt * 3));
                float vx = o->position.x - px, vz = o->position.z - pz;
                if (vx * vx + vz * vz > 1e-7f) o->rotation.y = std::atan2(vx, vz);
                state = "fly";
            } else {
                // dog / cat trotting behind
                glm::vec3 tmp(base.x - fx * (1.2f + slot * 0.6f) + fz * 0.5f, 0, base.z - fz * (1.2f + slot * 0.6f) - fx * 0.5f);
                float d = std::hypot(tmp.x - o->position.x, tmp.z - o->position.z);
                if (d > 10) {
                    o->position = {tmp.x, base.y, tmp.z};
                } else if (d > 0.15f) {
                    float sp = std::min(d * 3, 9.0f) * dt;
                    o->position.x += (tmp.x - o->position.x) / d * sp;
                    o->position.z += (tmp.z - o->position.z) / d * sp;
                    o->rotation.y = std::atan2(tmp.x - o->position.x, tmp.z - o->position.z);
                }
                float g = world.groundAt(o->position.x, o->position.z, base.y + 0.5f);
                o->position.y += (g - o->position.y) * std::min(1.0f, dt * 12);
                state = d > 0.3f ? "walk-moving" : "walk";
                ++slot;
            }
            pet.rig->animate(t, dt, state, 0);
        }
    }

    // vehicle pieces: m = [x, y, z, heading] of the ridden animal / the boat
    Placer place;
    void updateVehicle(Peer& p, bool visible) {
        const json& s = p.state;
        std::string v = s.value("v", json()).is_string() ? s["v"].get<std::string>() : "";
        std::string want = visible && !v.empty() && v != "glide" && v != "sleep" ? v : "";
        if (want != "boat" && p.boat) p.boat->visible = false;
        if (p.mount && want != p.mountKind) p.mount->visible = false;
        if (want.empty() || !s.contains("m") || !s["m"].is_array()) return;
        ObjPtr obj;
        if (want == "boat") {
            if (!p.boat && make.boat) p.boat = make.boat();
            obj = p.boat;
        } else {
            if (p.mountKind != want) {
                if (p.mount) scene.remove(p.mount.get());
                p.mount = make.mount ? make.mount(want) : nullptr;
                p.mountKind = want;
            }
            obj = p.mount;
        }
        if (!obj) return;
        if (!obj->parent) scene.add(obj);
        obj->visible = true;
        glm::vec3 mp(s["m"][0], s["m"][1], s["m"][2]);
        float my = s["m"][3];
        place(s.value("realm", ""), mp, my);
        obj->position = mp;
        obj->rotation.y = my;
    }
};

Multiplayer::Multiplayer(Object3D& scene, const std::string& character, const std::string& server, NetMakers make)
    : d(std::make_unique<Impl>(scene)) {
    d->character = character;
    d->make = std::move(make);
    size_t c = server.rfind(':');
    if (server.empty() || c == std::string::npos) {
        d->running = false;
        return;
    }
    d->host = server.substr(0, c);
    d->port = server.substr(c + 1);
    std::printf("multiplayer: connecting to ws://%s/ws\n", server.c_str());
    d->thread = std::thread([this] { d->loop(); });
}

Multiplayer::~Multiplayer() {
    d->running = false;
    d->sock.close();
    if (d->thread.joinable()) d->thread.join();
}

int Multiplayer::count() const { return (int)d->peers.size(); }
bool Multiplayer::connected() const { return d->open; }

void Multiplayer::update(float t, float dt, const World& world, const Placer& toGlobal) {
    Impl& I = *d;
    // incoming messages
    std::deque<std::string> msgs;
    {
        std::lock_guard<std::mutex> g(I.inLock);
        msgs.swap(I.inbox);
    }
    for (auto& text : msgs) {
        json msg = json::parse(text, nullptr, false);
        if (!msg.is_object()) continue;
        std::string tt = msg.value("t", "");
        if (tt == "hello") {
            I.myId = msg.value("id", -1);
        } else if (tt == "state" && msg.value("id", -1) != I.myId) {
            I.receive(msg, *this);
        } else if (tt == "leave") {
            I.removePeer(msg.value("id", -1), false, *this);
        } else if (tt == "__closed") {
            std::vector<int> ids;
            for (auto& [id, p] : I.peers) ids.push_back(id);
            for (int id : ids) I.removePeer(id, true, *this);
        }
    }
    // our own state ~12 Hz
    I.sendTimer += dt;
    if (I.open && I.sendTimer >= SEND_EVERY && getState) {
        I.sendTimer = 0;
        NetState s;
        if (getState(s)) {
            auto r2 = [](float v) { return std::round(v * 100) / 100; };
            json j = {{"t", "state"}, {"char", I.character}, {"realm", s.realm}, {"p", {r2(s.p.x), r2(s.p.y), r2(s.p.z)}},
                      {"yaw", r2(s.yaw)}, {"gr", s.grounded ? 1 : 0}, {"pets", s.pets}};
            if (s.jp == 1 || s.jp == 2 || s.jp == 0) j["jp"] = (int)s.jp;
            else j["jp"] = s.jp;
            j["v"] = s.v.empty() ? json() : json(s.v);
            j["m"] = s.hasM ? json({s.m.x, s.m.y, s.m.z, s.m.w}) : json();
            I.sock.sendText(j.dump());
        }
    }
    // draw the others
    I.place = toGlobal;
    for (auto& [id, pp] : I.peers) {
        Impl::Peer& p = *pp;
        const json& s = p.state;
        // positions arrive in the sender's realm coordinates (same as the web build)
        glm::vec3 gp = p.local;
        float gy = s.value("yaw", 0.0f);
        bool visible = toGlobal(s.value("realm", ""), gp, gy);
        p.target = gp;
        p.root->visible = visible;
        I.updateVehicle(p, visible);
        I.updatePets(p, t, dt, world, visible);
        if (!visible) continue;
        std::string v = s.value("v", json()).is_string() ? s["v"].get<std::string>() : "";
        glm::vec3& pos = p.root->position;
        float px = pos.x, pz = pos.z;
        if (v == "sleep" && s.contains("m") && s["m"].is_array()) {
            // lying in bed: put the body in the middle of the bed for this machine's camera angle
            float yaw = g_spriteCameraYaw, by = 0;
            glm::vec3 bed(s["m"][0].get<float>(), s["m"][1].get<float>(), s["m"][2].get<float>());
            toGlobal(s.value("realm", ""), bed, by);
            p.target = {bed.x + std::cos(yaw) * 0.72f, bed.y + 0.55f, bed.z - std::sin(yaw) * 0.72f};
        }
        if (p.fresh || glm::length(pos - p.target) > 8) {
            pos = p.target;
            p.fresh = false;
        } else {
            pos = glm::mix(pos, p.target, 1 - std::exp(-dt * 12));
        }
        p.yaw += wrapAngle(gy - p.yaw) * std::min(1.0f, dt * 12);
        p.speed += (std::hypot(pos.x - px, pos.z - pz) / std::max(dt, 1e-3f) - p.speed) * std::min(1.0f, dt * 8);
        p.phase += dt * (5 + p.speed * 1.6f);
        // pose: glide = arms up, airborne = jump frame, walking = alternate legs
        SpriteActor& a = *p.actor;
        a.object->rotation.y = p.yaw;
        a.billboard();
        std::string dir = a.viewDir(p.yaw), step;
        bool gr = s.value("gr", 1) != 0;
        if (v == "glide") step = "G";
        else if (!gr && v.empty()) step = "1";
        else if (p.speed > 0.9f && v.empty()) {
            int k = (int)std::floor(p.phase / (PI / 2)) % 4;
            step = k == 1 ? "1" : k == 3 ? "3" : "";
        }
        a.setFrame(v == "sleep" ? dir : dir + step);
        a.plane->rotation.z = v == "sleep" ? PI / 2 : 0;
        // jetpack: 2 = thrusting, 1.5 = drifting down, 1 = worn on the ground
        float jp = s.contains("jp") && s["jp"].is_number() ? s["jp"].get<float>() : 0;
        p.jet.group->visible = jp != 0;
        p.jet.group->rotation.y = p.yaw;
        setFlames(p.jet.flames, jp == 2 ? 1 : jp == 1.5f ? 0.45f : 0);
        if (p.glider) {
            p.glider->visible = v == "glide";
            p.glider->rotation = {-0.12f, p.yaw, 0};
        }
    }
}
