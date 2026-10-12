// Game loop — started as a port of main.js (patty-project/xmas-app/web/js/main.js), now one open world:
// the snow village, the medieval citadel and the wilds valley between them (world/overworld.h).
// The player, the companions, cherries and boats live in global coordinates; each town's own systems
// (parkour, berry bushes, Cube World houses / animals) keep their town-local coordinates.
#include "game/game.h"
#include <CommonCrypto/CommonDigest.h>
#include <GLFW/glfw3.h>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <functional>
#include <map>
#include <optional>
#include "art/glow.h"
#include "art/sprites.h"
#include "audio/audio.h"
#include "core/math.h"
#include "game/berries.h"
#include "game/combat.h"
#include "game/config.h"
#include "game/effects.h"
#include "game/home.h"
#include "game/parkour.h"
#include "game/pets.h"
#include "game/player.h"
#include "game/save.h"
#include "game/vehicles.h"
#include "gfx/gl.h"
#include "gfx/shader.h"
#include "gfx/texture.h"
#include "net/net.h"
#include "ui/canvas2d.h"
#include "ui/hud.h"
#include "ui/minimap.h"
#include "world/cubeworld.h"
#include "world/layout.h"
#include "world/medieval.h"
#include "world/overworld.h"
#include "world/winter.h"

namespace {

// only SHA-256 hashes of the secret codes are kept — never write the words in code or docs
const std::map<std::string, std::string> CHAR_CODES = {
    {"2669dd54b1ab9d3ffa70f29d18b00433a46891f57f677b56ec9f4fae49f07eee", "ter"},
    {"d47c6f591a677ba80bd2f4e884f3a7f15fd9b4aba36c68df161517f594f8e942", "patty"},
    {"b624e6f735cd0a9e8e27833dc6e8c458620884956891a6de611470a3a0cf2314", "momonga"},
};

std::string sha256Hex(const std::string& s) {
    unsigned char h[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(s.data(), (CC_LONG)s.size(), h);
    char out[65];
    for (int i = 0; i < 32; ++i) std::snprintf(out + i * 2, 3, "%02x", h[i]);
    return out;
}

std::string pick(const std::vector<std::string>& v) { return v[(size_t)(frand() * v.size()) % v.size()]; }

bool contains(const std::vector<std::string>& v, const std::string& s) { return std::find(v.begin(), v.end(), s) != v.end(); }

// ---------- input: keyboard + drag-to-look (createInput in input.js) ----------
struct Input {
    GLFWwindow* win = nullptr;
    bool enabled = false;
    std::map<int, int> prev;
    float lookDx = 0, lookDy = 0, zoom = 0;
    // first connected gamepad (GLFW's standard Xbox-style mapping)
    GLFWgamepadstate pad{}, padPrev{};
    bool hasPad = false;
    void pollPad() {
        padPrev = pad;
        hasPad = false;
        for (int j = GLFW_JOYSTICK_1; j <= GLFW_JOYSTICK_LAST && !hasPad; ++j)
            if (glfwJoystickIsGamepad(j) && glfwGetGamepadState(j, &pad)) hasPad = true;
        if (!hasPad) pad = GLFWgamepadstate{};
    }
    bool padPressed(int b) const { return hasPad && pad.buttons[b] && !padPrev.buttons[b]; }
    static float dead(float v) { return std::abs(v) < 0.18f ? 0.0f : v; }
    bool down(int k) const { return glfwGetKey(win, k) == GLFW_PRESS; }
    bool pressed(int k) {
        int now = glfwGetKey(win, k);
        bool edge = now == GLFW_PRESS && prev[k] != GLFW_PRESS;
        prev[k] = now;
        return edge;
    }
    glm::vec2 move() const {
        if (!enabled) return glm::vec2(0);
        float x = 0, z = 0;
        if (down(GLFW_KEY_A) || down(GLFW_KEY_LEFT)) x -= 1;
        if (down(GLFW_KEY_D) || down(GLFW_KEY_RIGHT)) x += 1;
        if (down(GLFW_KEY_W) || down(GLFW_KEY_UP)) z -= 1;
        if (down(GLFW_KEY_S) || down(GLFW_KEY_DOWN)) z += 1;
        float l = std::hypot(x, z);
        if (l > 0) {
            x /= l;
            z /= l;
        }
        // left stick (like the touch joystick on the web)
        if (hasPad) {
            x += dead(pad.axes[GLFW_GAMEPAD_AXIS_LEFT_X]);
            z += dead(pad.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]);
        }
        return {x, z};
    }
    bool padSprint() const {
        if (!hasPad) return false;
        float sx = pad.axes[GLFW_GAMEPAD_AXIS_LEFT_X], sy = pad.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];
        return pad.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER] || pad.buttons[GLFW_GAMEPAD_BUTTON_LEFT_THUMB] || std::hypot(sx, sy) > 0.97f;
    }
    bool sprint() const { return down(GLFW_KEY_LEFT_SHIFT) || down(GLFW_KEY_RIGHT_SHIFT) || padSprint(); }
    // Space / A held (jetpack)
    bool up() const { return enabled && (down(GLFW_KEY_SPACE) || (hasPad && pad.buttons[GLFW_GAMEPAD_BUTTON_A])); }
};

float g_scroll = 0;
std::string g_typed;
int g_keyEvent = 0;

}  // namespace

struct Game::Impl {
    LaunchOptions opt;
    GLFWwindow* win = nullptr;
    Renderer renderer;
    Camera camera;
    Input input;
    Hud hud;
    HudState hs;
    Audio audio;
    ObjPtr scene = makeGroup("scene");
    std::string CHAR;
    Settings settings;

    std::unique_ptr<WinterWorld> winter;
    std::unique_ptr<MedievalWorld> medieval;
    std::unique_ptr<Overworld> world;
    // area the player is in: winter / medieval / wilds
    std::string area = "winter";
    std::unique_ptr<Player> player;
    std::unique_ptr<Snowfall> snow;
    std::unique_ptr<Fireworks> fireworks;
    std::unique_ptr<Footprints> footprints;
    std::unique_ptr<HomeDiorama> home;
    ObjPtr hearth;
    std::vector<std::unique_ptr<Pet>> pets;
    std::vector<Pet*> parrots;
    std::vector<glm::vec2> bushSpots;
    std::unique_ptr<BerryBushes> winterBushes, medievalBushes, wildsBushes;
    std::unique_ptr<CherryThrower> thrower;
    std::unique_ptr<Parkour> parkourWinter, parkourMedieval;
    glm::vec3 gliderSpot{0};
    std::unique_ptr<GliderPickup> gliderPickup;
    // the swan-lake boat in the citadel and the river boat of the wilds (both in global coordinates)
    std::vector<std::unique_ptr<Boat>> boats;
    Boat* boat = nullptr;
    bool boating = false;
    std::unique_ptr<SpriteActor> snowmanActor;
    ObjPtr snowman;
    ObjPtr zzz;
    std::unique_ptr<Minimap> minimap;
    std::unique_ptr<Multiplayer> net;
    // monsters and the sword (the wilds only)
    std::unique_ptr<Combat> combat;

    // riding: which area's herd the animal belongs to (its coordinates are that area's)
    std::string rideArea;
    // sleeping in a Cube World bed: bed = middle of the bed at floor level (global), back = where we stood
    bool sleeping = false;
    glm::vec3 sleepBed{0}, sleepBack{0};
    std::string sleepArea;
    static constexpr int COOK_COST = 3;

    std::string mode = "play";
    bool started = false;
    std::string zoneNow;
    float warpCooldown = 0;
    bool homeCelebrated = false;
    float nextFirework = 0;
    bool wantShot = false;
    bool reload = false;

    // camera (updateCamera in main.js)
    float DEFAULT_DIST = 14;
    float camYaw = 0, camPitch = 0.72f, camDist = 14, camClear = 1, camLift = 0, lastDrag = -10;
    glm::vec3 camTarget{0}, camPos{0};
    bool dragging = false, dragOnUi = false;
    double lastMx = 0, lastMy = 0;
    // a short click (not a drag) on the 3D view = attack; where / when the press started
    double pressMx = 0, pressMy = 0, pressAt = 0;
    bool rightWasDown = false, padRT = false, padLT = false;

    float elapsed = 0;
    // setTimeout replacement
    struct Timer {
        float at;
        std::function<void()> fn;
    };
    std::vector<Timer> timers;
    void later(float seconds, std::function<void()> fn) { timers.push_back({elapsed + seconds, std::move(fn)}); }

    int fpsFrames = 0;
    float fpsTime = 0;

    bool openWindow();
    void build();
    // --shot test runs never touch the real save
    void persist() {
        if (opt.shotPath.empty()) save::store(CHAR, settings);
    }
    Pet* pet(const std::string& key) {
        for (auto& p : pets)
            if (p->key == key) return p.get();
        return nullptr;
    }
    World* areaWorld(const std::string& a) {
        if (a == "winter") return winter.get();
        if (a == "medieval") return medieval.get();
        return world.get();
    }
    glm::vec3 local(const std::string& a, glm::vec3 p) const { return world->toLocal(a, p); }
    glm::vec3 global(const std::string& a, glm::vec3 p) const { return world->toGlobal(a, p); }
    // the herd ride of an area (animals walk in that area's coordinates)
    CubeWorld::Ride* rideOf(const std::string& a) { return cubeWorld().available() ? cubeWorld().ride(*areaWorld(a)) : nullptr; }
    CubeWorld::Ride* activeRide() {
        if (rideArea.empty()) return nullptr;
        auto* r = rideOf(rideArea);
        return r && r->active() ? r : nullptr;
    }
    bool isRiding() { return activeRide() != nullptr; }
    // a rideable animal near the player in this area (or in the wilds right next to it)
    void* nearestMount(std::string& outArea);
    const CubeWorld::Act* nearestAct(std::string& outArea);
    Boat* nearestBoat();

    void setMode(const std::string& m);
    void enterArea(const std::string& a);
    void travelTo(const std::string& town);
    void startGame();
    void setTime(const std::string& name);
    void applySettings();
    void updateZone();
    void warpTo(glm::vec3 pos, float facing);
    void warpPets();
    void checkFindPets(float t);
    void updateBerryHUD() { hs.berries = settings.berries; }
    void throwCherry();
    void buildCombat();
    void dodge(glm::vec2 move);
    // the player can swing and be hit: on foot, awake, not in a fade
    bool canFight() { return started && mode == "play" && !boating && !sleeping && !isRiding(); }
    void onCourseComplete(const std::string& id, glm::vec3 pos);
    void birdSay(const std::string& key, const std::string& text, float delay = 0);
    void companionSay(const std::string& text);
    void gotGlider();
    void stopRiding();
    void interact();
    glm::vec3 bedSeat(glm::vec3 bed) const;
    void wakeUp();
    void doAct(const CubeWorld::Act& act, const std::string& actArea);
    void submitCode();
    void handleUi(const std::string& id);
    void takeShot(int fw, int fh);
    float cameraClearance(glm::vec3 from, glm::vec3 to);
    void placeCam(float pitch) {
        camPos = {camTarget.x + std::sin(camYaw) * std::cos(pitch) * camDist, camTarget.y + std::sin(pitch) * camDist,
                  camTarget.z + std::cos(camYaw) * std::cos(pitch) * camDist};
    }
    void updateCamera(float dt, float t);
    void tick(float dt, float t);
};

bool Game::Impl::openWindow() {
    if (!glfwInit()) return false;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    // the 3D view has its own depth buffer (renderer.cpp); the window only gets the finished image + HUD
    glfwWindowHint(GLFW_DEPTH_BITS, 0);
    glfwWindowHint(GLFW_STENCIL_BITS, 0);
    win = glfwCreateWindow(1280, 800, "Ter's Realms", nullptr, nullptr);
    if (!win) return false;
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);
    std::printf("OpenGL %s · %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));
    glfwSetScrollCallback(win, [](GLFWwindow*, double, double y) { g_scroll += (float)y; });
    glfwSetCharCallback(win, [](GLFWwindow*, unsigned int cp) {
        // UTF-8 encode the typed character for the secret-code box
        if (cp < 0x80) {
            g_typed += (char)cp;
        } else if (cp < 0x800) {
            g_typed += (char)(0xc0 | (cp >> 6));
            g_typed += (char)(0x80 | (cp & 0x3f));
        } else {
            g_typed += (char)(0xe0 | (cp >> 12));
            g_typed += (char)(0x80 | ((cp >> 6) & 0x3f));
            g_typed += (char)(0x80 | (cp & 0x3f));
        }
    });
    glfwSetKeyCallback(win, [](GLFWwindow*, int key, int, int action, int) {
        if (action == GLFW_PRESS || action == GLFW_REPEAT) g_keyEvent = key;
    });
    input.win = win;
    return true;
}

// ---------------------------------------------------------------------------
// building everything
// ---------------------------------------------------------------------------
void Game::Impl::build() {
    double t0 = glfwGetTime();
    // character: Patty by default; the secret code (Y) switches to Ter / Momonga and is remembered
    CHAR = opt.character.empty() ? save::loadCharacter() : opt.character;
    settings = save::load(CHAR);
    if (!opt.time.empty()) settings.time = opt.time;
    if (!opt.realm.empty()) settings.realm = opt.realm;

    winter = std::make_unique<WinterWorld>();
    winter->setTreeProgress(1);
    winter->setHomeReady(true);
    medieval = std::make_unique<MedievalWorld>();
    world = std::make_unique<Overworld>(*winter, *medieval);
    scene->add(world->group);
    std::printf("open world built in %.0f ms\n", (glfwGetTime() - t0) * 1000);

    player = std::make_unique<Player>(world.get(), CHAR);
    scene->add(player->object);
    snow = std::make_unique<Snowfall>(2600);
    fireworks = std::make_unique<Fireworks>();
    footprints = std::make_unique<Footprints>();
    scene->add(snow->object, fireworks->object, footprints->object);

    // the old Christmas room (last year's gift) stands in the snow village
    home = std::make_unique<HomeDiorama>(true, false);
    home->group->position = winter->homeAnchorPos + glm::vec3(0, 2.5f, 0);
    home->group->rotation.y = winter->homeAnchorRotY;
    scene->add(home->group);
    hearth = makePointLight(hexc(0xffa860), 1.2f, 7, 2);
    hearth->position = winter->homeAnchorPos + glm::vec3(-1, 2, -1);
    scene->add(hearth);

    // ---------- companions ----------
    // the puppy is there from the start; Cham (on a stump in the woods) and Juice (asleep on the lake) must be found
    struct PetDef {
        std::string key, mode, idle;
        bool lost, canFly;
    };
    std::vector<PetDef> defs;
    if (CHAR == "ter") {
        // Ter's tabby cat curls up beside a cabin
        defs = {{"slid", "walk", "sleep", true, false}};
    } else if (CHAR != "momonga") {
        defs = {{"cham", "fly", "perch", true, true}, {"juice", "shoulder", "sleep", true, false}, {"puppy", "walk", "walk", false, true}};
    }
    for (auto& d : defs) {
        // winter coordinates are global coordinates
        glm::vec3 spot = d.lost ? winter->petSpots[d.key] : winter::SPAWN;
        PetOptions o;
        o.idle = d.idle;
        o.canFly = d.canFly;
        const config::Lines* talk = d.key == "slid" ? &config::cat : config::birds().count(d.key) ? &config::birds().at(d.key) : nullptr;
        if (talk) {
            o.lines = talk->talk;
            o.lostLines = talk->lost;
            if (!talk->eat.empty()) o.eatLines = talk->eat;
        }
        auto p = std::make_unique<Pet>(std::make_unique<CritterRig>(d.key), spot, d.mode, o);
        p->key = d.key;
        if (!d.lost || contains(settings.found, d.key)) p->find(-10);
        scene->add(p->object);
        pets.push_back(std::move(p));
    }

    // ---------- berries: pick from bushes, throw them for Cham to fetch ----------
    bushSpots = {{6, 8}, {-5, 10}, {9, -4}, {-8, -8}, {14, 4}, {-14, 2}, {3, -14}, {-3, -22}, {20, 8}, {-24, 12},
                 {24, -8}, {-26, -14}, {10, 26}, {-6, 30}, {34, 8}, {-38, -2}, {6, -26}, {-12, 36}, {28, 20}};
    // on the mountain terraces (a reward for climbers)
    const auto& MT = winter::MOUNTAIN;
    for (int i = 0; i < 4; ++i) {
        float k = 0.2f * (i + 1);
        for (float a0 : {0.0f, 2.1f, 4.2f}) {
            float ang = a0 + i * 0.9f;
            bushSpots.push_back({MT.x + std::cos(ang) * MT.r * (1 - k) * 0.95f, MT.z + std::sin(ang) * MT.r * (1 - k) * 0.95f});
        }
    }
    // in the cave
    const auto& CV = winter::CAVE;
    for (glm::vec2 o : {glm::vec2(1.2f, -6), glm::vec2(9, -18.5f), glm::vec2(24, -4), glm::vec2(17, 5)}) bushSpots.push_back(CV + o);
    winterBushes = std::make_unique<BerryBushes>(*scene, *winter, bushSpots);
    // the citadel's bushes live in its own (turned) coordinates; no bushes in the moat
    std::vector<glm::vec2> medSpots;
    for (auto s : medieval->berrySpots)
        if (medieval->groundAt(s.x, s.y, -99) > -0.3f) medSpots.push_back(s);
    medievalBushes = std::make_unique<BerryBushes>(*medieval->group, *medieval, medSpots);
    // the valley's bushes are already in global coordinates
    wildsBushes = std::make_unique<BerryBushes>(*world->group, *world, world->berrySpots);
    thrower = std::make_unique<CherryThrower>(*scene, world.get());
    // birds that can fly to fetch cherries (Juice cannot fly)
    for (auto& p : pets)
        if (p->key != "puppy" && p->canFly()) parrots.push_back(p.get());
    updateBerryHUD();

    // ---------- parkour (ice tower / crystal pillars in the cave / crates in the town) ----------
    auto done = [this](const std::string& id, glm::vec3 pos) { onCourseComplete(id, pos); };
    parkourWinter = std::make_unique<Parkour>(*winter, "winter", settings.stars, done);
    parkourMedieval = std::make_unique<Parkour>(*medieval, "medieval", settings.stars, [this](const std::string& id, glm::vec3 pos) {
        onCourseComplete(id, global("medieval", pos));
    });

    // ---------- glider stand on top of Mount Aurora ----------
    player->hasGlider = settings.glider;
    gliderSpot = {MT.x + 2.4f, 0, MT.z - 1.6f};
    gliderSpot.y = winter->groundAt(gliderSpot.x, gliderSpot.z, 99);
    gliderPickup = std::make_unique<GliderPickup>(*winter, gliderSpot, settings.glider);
    player->onGlide = [this](bool on) {
        audio.jump();
        if (on && frand() < 0.4f) birdSay("cham", pick({"ว้าว บินได้!", "บินด้วยกัน~", "สูงจัง!"}));
    };

    // ---------- boats: the swan lake in the citadel + the river boat at the village jetty ----------
    {
        glm::vec3 lake = global("medieval", {medieval::BOAT_SPOT.x, 0, medieval::BOAT_SPOT.z});
        float heading = world->yawToGlobal("medieval", medieval::BOAT_SPOT.heading);
        boats.push_back(std::make_unique<Boat>(*world, Boat::Spot{lake.x, lake.z, heading}, layout::WATER_Y));
        boats.push_back(std::make_unique<Boat>(*world, Boat::Spot{world->springBoat.x, world->springBoat.y, world->springBoatHeading}, layout::WATER_Y));
    }

    // ---------- snowman (decoration in the snow map) ----------
    snowmanActor = std::make_unique<SpriteActor>("snowman", 0.075f);
    snowman = makeGroup("snowman");
    snowman->add(snowmanActor->object);
    snowman->position = winter->snowmanSpot;
    scene->add(snowman);

    // ---------- Zzz above the bed ----------
    {
        Canvas2D c(128, 64);
        c.setFont("Mali", 40, 700);
        c.setFill("#fff6ea");
        c.setStroke("rgba(42,29,74,0.7)");
        c.setLineWidth(6);
        c.strokeText("Zzz", 12, 46);
        c.fillText("Zzz", 12, 46);
        auto tex = std::make_shared<Texture>();
        c.upload(*tex);
        auto m = spriteMaterial(tex);
        m->depthTest = false;
        zzz = makeSprite(m);
        zzz->scale = {0.8f, 0.4f, 1};
        zzz->renderOrder = 18;
        zzz->visible = false;
        scene->add(zzz);
    }

    // ---------- Cube World voxel models (swap tagged models + decorations + animals; skipped when missing) ----------
    t0 = glfwGetTime();
    if (cubeWorld().init()) {
        cubeWorld().applyTags(*scene);
        // buildings must not cover: berry bushes / lost companions / the glider stand / the jetties
        std::vector<glm::vec3> avoidW, avoidM, avoidX;
        for (auto s : bushSpots) avoidW.push_back({s.x, s.y, 1.5f});
        for (auto& [k, v] : winter->petSpots) avoidW.push_back({v.x, v.z, 3});
        avoidW.push_back({gliderSpot.x, gliderSpot.z, 4});
        for (auto s : medieval->berrySpots) avoidM.push_back({s.x, s.y, 1.5f});
        avoidX = {{world->springDock.x, world->springDock.z, 8}, {world->harbourDock.x, world->harbourDock.z, 8}, {-9, 128, 7}};
        for (auto s : world->berrySpots) avoidX.push_back({s.x, s.y, 1.5f});
        cubeWorld().populate(*winter, layout::WATER_Y, avoidW);
        cubeWorld().populate(*medieval, layout::WATER_Y, avoidM);
        cubeWorld().populate(*world, layout::WATER_Y, avoidX);
        std::printf("Cube World placed in %.0f ms\n", (glfwGetTime() - t0) * 1000);
    }
    // after Cube World so the monsters can use its voxel models
    buildCombat();
    if (cubeWorld().available()) cubeWorld().releaseSources();

    player->onStep = [this](glm::vec3 p, float yaw) {
        // footprints in the snow (the village and the snowy end of the valley)
        if (area == "winter" || (area == "wilds" && world->southness(p.z) < 0.4f)) footprints->add(p, yaw, elapsed);
        audio.step();
    };
    player->onJump = [this] {
        audio.jump();
        if (!pets.empty()) pets[0]->cheer();
    };
    player->onLand = [this] { audio.land(); };

    // ---------- multiplayer: see the other player (Patty / Ter) on the same server ----------
    NetMakers mk;
    mk.glider = [] {
        CwTag tag;
        tag.name = "glider";
        tag.w = 2.4f;
        tag.rotY = PI;
        tag.offset = {0, -1.15f, -0.55f};
        return cubeWorld().available() ? cubeWorld().mesh("glider", tag) : nullptr;
    };
    mk.mount = [](const std::string& kind) { return cubeWorld().available() ? cubeWorld().animalModel(kind) : nullptr; };
    mk.boat = [this] { return boats[0]->object->clone(); };
    net = std::make_unique<Multiplayer>(*scene, CHAR, opt.server, mk);
    // positions go out in realm coordinates like the web build (realm = the area the player is in)
    net->getState = [this](NetState& s) {
        if (mode != "play") return false;
        auto* ride = activeRide();
        s.realm = area;
        s.p = local(area, player->position);
        s.yaw = world->yawToLocal(area, player->yaw);
        s.grounded = player->grounded;
        s.jp = player->jetpack() ? (player->thrusting ? 2.0f : player->grounded ? 1.0f : 1.5f) : 0.0f;
        s.v = sleeping ? "sleep" : player->gliding ? "glide" : boating ? "boat" : ride ? ride->kind() : "";
        if (sleeping) {
            s.hasM = true;
            s.m = {local(area, sleepBed), 0};
        } else if (boating) {
            s.hasM = true;
            s.m = {local(area, boat->object->position), world->yawToLocal(area, boat->heading())};
        } else if (ride) {
            glm::vec4 p = ride->pose();
            glm::vec3 gp = global(rideArea, glm::vec3(p));
            s.hasM = true;
            s.m = {local(area, gp), world->yawToLocal(area, world->yawToGlobal(rideArea, p.w))};
        }
        for (auto& p : pets)
            if (p->found()) s.pets.push_back(p->key);
        return true;
    };
    net->onJoin = [this](const std::string& name) { companionSay(name + " มาแล้ว~ ♥"); };
    net->onLeave = [this](const std::string& name) { companionSay(name + " ไปแล้ว…"); };

    minimap = std::make_unique<Minimap>(300);
    hs.settings = &settings;
    setTime(settings.time);
    applySettings();
}

// ---------------------------------------------------------------------------
// areas / travel
// ---------------------------------------------------------------------------
void Game::Impl::setMode(const std::string& m) {
    mode = m;
    input.enabled = m == "play";
    hs.mode = m;
    if (m != "play") hs.settingsOpen = false;
}

// HUD labels / minimap follow the area you walk into
void Game::Impl::enterArea(const std::string& a) {
    area = a;
    if (a == "winter") {
        hs.worldTag = "THE WINTER REALM";
        hs.zoneRegion = "THE SNOWLANDS";
        hs.mmLabel = "⌂ WINTER VILLAGE";
        minimap->setMapData(winter->mapData);
    } else if (a == "medieval") {
        hs.worldTag = "MEDIEVAL CITADEL";
        hs.zoneRegion = "MEDIEVAL REALM";
        hs.mmLabel = "🏰 THE CITADEL";
        minimap->setMapData(medieval->mapData);
    } else {
        hs.worldTag = "THE WILDS";
        hs.zoneRegion = "THE OLD ROAD";
        hs.mmLabel = "🌲 THE WILDS";
        minimap->setMapData(world->wildsMap);
    }
    // the top-bar button is a quick trip to the other town
    bool south = a == "medieval" || (a == "wilds" && world->southness(player->position.z) > 0.5f);
    hs.realmIcon = south ? "❄️" : "🏰";
    hs.realmText = south ? "สู่ดินแดนหิมะ" : "สู่ยุคกลาง";
    if (a != "wilds" && settings.realm != a) {
        settings.realm = a;
        persist();
    }
    zoneNow = "";
    updateZone();
}

// fast travel (the top-bar button): fade, then appear at the other town's square
void Game::Impl::travelTo(const std::string& town) {
    setMode("fade");
    hud.setFade(true);
    audio.chime();
    later(0.6f, [this, town] {
        if (boating) {
            boating = false;
            boat->leave();
        }
        stopRiding();
        if (sleeping) wakeUp();
        glm::vec3 dest = town == "medieval" ? global("medieval", medieval::SPAWN) : winter::SPAWN;
        player->teleport(dest, town == "medieval" ? world->yawToGlobal("medieval", PI) : PI);
        warpPets();
        camYaw = player->yaw + PI;
        camTarget = player->position;
        warpCooldown = 2.5f;
        later(0.2f, [this] {
            hud.setFade(false);
            setMode("play");
        });
    });
}

void Game::Impl::startGame() {
    audio.unlock();
    // start in the town you were last in
    bool med = settings.realm == "medieval";
    glm::vec3 spawn = med ? global("medieval", medieval::SPAWN) : winter::SPAWN;
    player->teleport(spawn, med ? world->yawToGlobal("medieval", PI) : PI);
    warpPets();
    camYaw = player->yaw + PI;
    camTarget = player->position;
    started = true;
    setMode("play");
    enterArea(world->areaAt(player->position.x, player->position.z));
}

void Game::Impl::setTime(const std::string& name) {
    world->setTimeOfDay(name);
    settings.time = name;
    persist();
    hs.time = name;
}

void Game::Impl::applySettings() {
    renderer.tiltShift = settings.tilt;
    renderer.shadows = settings.shadows;
}

void Game::Impl::updateZone() {
    std::string z = world->zoneAt(player->position.x, player->position.z);
    if (z == zoneNow) return;
    zoneNow = z;
    auto it = config::zones().find(z);
    hs.zoneName = it != config::zones().end() ? it->second.first : "The Realm";
    hs.zoneSub = it != config::zones().end() ? it->second.second : "◆ ดินแดน";
    hud.zoneSwap(elapsed);
}

// ---------- in / out of the cave (screen fades, then warp) ----------
void Game::Impl::warpTo(glm::vec3 pos, float facing) {
    setMode("fade");
    hud.setFade(true);
    later(0.7f, [this, pos, facing] {
        player->teleport(pos, facing);
        warpPets();
        camYaw = facing + PI;
        camTarget = player->position;
        warpCooldown = 2;
        later(0.15f, [this] {
            hud.setFade(false);
            setMode("play");
        });
    });
}

void Game::Impl::warpPets() {
    for (auto& p : pets)
        if (p->found()) p->object->position = player->position + glm::vec3(0, 0.5f, 0.8f);
}

void Game::Impl::checkFindPets(float t) {
    glm::vec3 p = player->position;
    for (auto& pet : pets) {
        if (pet->found()) continue;
        if (std::hypot(p.x - pet->position.x, p.z - pet->position.z) < 1.6f && std::abs(p.y - pet->position.y) < 1.8f) {
            pet->find(t);
            pet->say("♥♥", t);
            settings.found.push_back(pet->key);
            persist();
            audio.chime();
            hud.floatHearts(12, t);
        }
    }
}

void Game::Impl::throwCherry() {
    if (mode != "play") return;
    if (settings.berries <= 0) {
        hud.shakeBerry(elapsed);
        return;
    }
    settings.berries--;
    persist();
    updateBerryHUD();
    thrower->throwCherry(player->position + glm::vec3(0, 1.1f, 0), player->yaw);
    audio.jump();
}

void Game::Impl::onCourseComplete(const std::string& id, glm::vec3 pos) {
    if (!contains(settings.stars, id)) settings.stars.push_back(id);
    persist();
    audio.fanfare();
    hud.floatHearts(16, elapsed);
    fireworks->burst(pos + glm::vec3(0, 2, 0), hexc(0xffd76a), 160);
    for (int i = 0; i < 4; ++i)
        later(0.3f + i * 0.35f, [this, pos] { fireworks->launch(pos + glm::vec3((frand() - 0.5f) * 8, 0, (frand() - 0.5f) * 8), 8 + frand() * 5); });
    int i = 0;
    for (auto& b : pets) {
        if (b->key == "puppy") continue;
        if (b->found()) {
            Pet* p = b.get();
            later(i * 0.5f, [this, p] { p->say(pick(config::cheer), elapsed); });
        }
        ++i;
    }
}

void Game::Impl::birdSay(const std::string& key, const std::string& text, float delay) {
    Pet* b = pet(key);
    if (b && b->found()) later(delay, [this, b, text] { b->say(text, elapsed, 3); });
}

// ---------------------------------------------------------------------------
// combat: monsters of the wilds (game/combat.cpp)
// ---------------------------------------------------------------------------
void Game::Impl::buildCombat() {
    combat = std::make_unique<Combat>(*world, *player, settings.level, settings.xp);
    combat->onSwing = [this] { audio.swing(); };
    combat->onHit = [this](glm::vec3, bool crit) { audio.hit(crit); };
    combat->onHurt = [this](float) {
        audio.hurt();
        hud.hurt(elapsed);
    };
    combat->onKill = [this](const std::string&, glm::vec3 p, glm::vec3 color) {
        audio.slay();
        fireworks->burst(p, color, 45);
        // monsters sometimes drop berries
        if (frand() < 0.4f) {
            settings.berries += 1 + (frand() < 0.3f);
            persist();
            updateBerryHUD();
            hud.bumpBerry(elapsed);
        }
    };
    combat->onProgress = [this] {
        settings.level = combat->level;
        settings.xp = combat->xp;
        persist();
    };
    combat->onLevelUp = [this](int lv) {
        audio.levelUp();
        hud.floatHearts(8, elapsed);
        fireworks->burst(player->position + glm::vec3(0, 1.2f, 0), glm::vec3(1, 0.85f, 0.4f), 80);
        companionSay("เลเวลอัป! Lv " + std::to_string(lv) + " ✨");
    };
    combat->onDeath = [this] {
        // wake up in whichever town is closer, at full health
        bool south = player->position.z > 128;
        glm::vec3 spawn = south ? global("medieval", medieval::SPAWN) : winter::SPAWN;
        float facing = south ? world->yawToGlobal("medieval", PI) : PI;
        if (boating) boating = false;
        warpTo(spawn, facing);
        later(0.75f, [this] {
            combat->revive();
            companionSay(pick({"ไม่เป็นไรนะ พักก่อน~", "เจ็บมั้ย? กลับมาตั้งหลักก่อน", "เกือบไปแล้ว!"}));
        });
    };
}

void Game::Impl::dodge(glm::vec2 move) {
    if (!canFight()) return;
    // camera-relative like walking
    float s = std::sin(camYaw), c = std::cos(camYaw);
    combat->roll({move.x * c + move.y * s, -move.x * s + move.y * c});
}

void Game::Impl::companionSay(const std::string& text) {
    for (auto& p : pets)
        if (p->found() && p->key != "puppy") {
            p->say(text, elapsed, 3);
            return;
        }
}

void Game::Impl::gotGlider() {
    settings.glider = true;
    persist();
    player->hasGlider = true;
    audio.fanfare();
    hud.floatHearts(14, elapsed);
    birdSay("cham", "ได้เครื่องร่อนแล้ว!");
    birdSay("cham", "กระโดดกลางอากาศ = ร่อน", 2.6f);
    birdSay("juice", "ลองกระโดดลงเขาสิ~", 1.3f);
}

// ---------- riding (Cube World animals: alpaca / pony in the towns, ponies on the old road) ----------
void* Game::Impl::nearestMount(std::string& outArea) {
    // the wilds herd can walk anywhere, so check it everywhere; town herds only in their town
    for (const std::string& a : {area, std::string("wilds")}) {
        auto* r = rideOf(a);
        if (!r) continue;
        if (void* m = r->nearest(local(a, player->position))) {
            outArea = a;
            return m;
        }
    }
    return nullptr;
}

void Game::Impl::stopRiding() {
    auto* ride = activeRide();
    if (!ride) return;
    player->ride(global(rideArea, ride->dismount()), player->yaw);
    rideArea.clear();
}

const CubeWorld::Act* Game::Impl::nearestAct(std::string& outArea) {
    if (!cubeWorld().available()) return nullptr;
    outArea = area;
    return cubeWorld().nearestAct(*areaWorld(area), local(area, player->position));
}

Boat* Game::Impl::nearestBoat() {
    for (auto& b : boats)
        if (glm::length(player->position - b->position()) < 2.6f) return b.get();
    return nullptr;
}

void Game::Impl::interact() {
    if (mode != "play") return;
    if (sleeping) {
        wakeUp();
        return;
    }
    if (isRiding()) {
        stopRiding();
        return;
    }
    std::string actArea;
    const CubeWorld::Act* act = !boating ? nearestAct(actArea) : nullptr;
    if (act) {
        doAct(*act, actArea);
        return;
    }
    std::string mountArea;
    void* mount = !boating ? nearestMount(mountArea) : nullptr;
    if (mount) {
        rideOf(mountArea)->mount(mount);
        rideArea = mountArea;
        birdSay("cham", pick({"ไปกันเลย~", "ขี่เก่งจัง!", "ฮี้~"}), 0.3f);
        return;
    }
    if (boating) {
        auto shore = boat->findShore(*world);
        if (!shore) {
            birdSay("cham", "เข้าใกล้ฝั่งก่อนนะ~");
            return;
        }
        boating = false;
        boat->leave();
        player->teleport(*shore, boat->heading());
        warpPets();
    } else if (Boat* b = nearestBoat()) {
        boat = b;
        boating = true;
        boat->board();
        player->ride(boat->seat(), boat->heading());
        birdSay("juice", "พายเรือกัน~", 0.3f);
    }
}

// ---------- inside the house (Cube World): sleep in the bed, cook with 3 berries ----------
// the sprite lies along the camera's left -> shift the feet right so the body is centred on the bed
glm::vec3 Game::Impl::bedSeat(glm::vec3 bed) const {
    glm::vec3 right(std::cos(g_spriteCameraYaw), 0, -std::sin(g_spriteCameraYaw));
    glm::vec3 p = bed + right * 0.72f;
    p.y = bed.y + 0.55f;
    return p;
}

void Game::Impl::wakeUp() {
    if (!sleeping) return;
    player->setLying(false);
    player->ride(sleepBack, player->yaw);
    sleeping = false;
    zzz->visible = false;
    companionSay(pick({"ตื่นแล้ว~", "หลับสบายไหม?", "อรุณสวัสดิ์~ ☀"}));
}

void Game::Impl::doAct(const CubeWorld::Act& act, const std::string& actArea) {
    if (act.kind == "bed") {
        sleeping = true;
        sleepBed = global(actArea, act.pos);
        sleepArea = actArea;
        sleepBack = player->position;
        player->setLying(true);
        player->ride(bedSeat(sleepBed), player->yaw);
        zzz->visible = true;
        companionSay(pick({"ฝันดีนะ~ ♥", "นอนด้วยๆ", "ง่วงจัง~"}));
        return;
    }
    if (settings.berries < COOK_COST) {
        companionSay("ขอเบอร์รี่ " + std::to_string(COOK_COST) + " ลูกนะ~");
        return;
    }
    if (cubeWorld().cook(act).empty()) return;
    settings.berries -= COOK_COST;
    persist();
    updateBerryHUD();
    audio.chime();
    hud.floatHearts(10, elapsed);
    companionSay(pick({"หอมจัง~", "อร่อย! ♥", "ทำเก่งจัง!", "น่ากินมาก~"}));
}

// ---------- secret code that switches the character (Y) ----------
void Game::Impl::submitCode() {
    std::string s = hs.codeText;
    while (!s.empty() && s.back() == ' ') s.pop_back();
    while (!s.empty() && s.front() == ' ') s.erase(s.begin());
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    auto it = CHAR_CODES.find(sha256Hex(s));
    if (it == CHAR_CODES.end()) {
        hud.shakeCode(elapsed);
        hs.codeText.clear();
        return;
    }
    hs.codeOpen = false;
    hs.codeText.clear();
    if (it->second == CHAR) return;
    persist();
    save::saveCharacter(it->second);
    // location.reload(): restart the game as the new character
    reload = true;
}

void Game::Impl::takeShot(int fw, int fh) {
    wantShot = false;
    std::vector<uint8_t> rgba((size_t)fw * fh * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, fw, fh, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    std::vector<uint8_t> flipped(rgba.size());
    for (int y = 0; y < fh; ++y)
        std::copy(rgba.begin() + (size_t)y * fw * 4, rgba.begin() + (size_t)(y + 1) * fw * 4, flipped.begin() + (size_t)(fh - 1 - y) * fw * 4);
    for (size_t i = 3; i < flipped.size(); i += 4) flipped[i] = 255;
    const char* homeDir = std::getenv("HOME");
    std::string path = std::string(homeDir ? homeDir : ".") + "/Downloads/patty-world-" + std::to_string((long long)std::time(nullptr)) + ".png";
    if (savePNG(path, fw, fh, flipped)) std::printf("photo saved: %s\n", path.c_str());
    hud.flash(elapsed);
}

void Game::Impl::handleUi(const std::string& id) {
    if (id.empty()) return;
    if (id == "restart") {
        settings.found.clear();
        settings.stars.clear();
        settings.berries = 0;
        settings.level = 1;
        settings.xp = 0;
        persist();
        reload = true;
    } else if (id == "realm") {
        travelTo(hs.realmText == "สู่ยุคกลาง" ? "medieval" : "winter");
    } else if (id == "music") {
        hs.muted = audio.toggleMute();
    } else if (id == "shot") {
        wantShot = true;
    } else if (id == "full") {
        if (glfwGetWindowMonitor(win)) {
            glfwSetWindowMonitor(win, nullptr, 100, 100, 1280, 800, GLFW_DONT_CARE);
        } else {
            GLFWmonitor* pm = glfwGetPrimaryMonitor();
            const GLFWvidmode* vm = glfwGetVideoMode(pm);
            glfwSetWindowMonitor(win, pm, 0, 0, vm->width, vm->height, vm->refreshRate);
        }
    } else if (id == "settings") {
        hs.settingsOpen = !hs.settingsOpen;
    } else if (id.rfind("time:", 0) == 0) {
        setTime(id.substr(5));
    } else if (id.rfind("pixel:", 0) == 0) {
        settings.pixel = std::stoi(id.substr(6));
        persist();
    } else if (id == "opt:tilt") {
        settings.tilt = !settings.tilt;
    } else if (id == "opt:shadows") {
        settings.shadows = !settings.shadows;
    } else if (id == "opt:snow") {
        settings.snow = !settings.snow;
    } else if (id == "opt:camAvoid") {
        settings.camAvoid = !settings.camAvoid;
    } else if (id == "camReset") {
        camYaw = player->yaw + PI;
        camPitch = 0.72f;
        camDist = DEFAULT_DIST;
    }
    if (id.rfind("opt:", 0) == 0) {
        applySettings();
        persist();
    }
}

// ---------------------------------------------------------------------------
// camera
// ---------------------------------------------------------------------------
// how far the camera can back away without entering a tree / house (asked of the area it looks at)
float Game::Impl::cameraClearance(glm::vec3 from, glm::vec3 to) {
    std::string a = world->areaAt(from.x, from.z);
    return areaWorld(a)->cameraClearance(local(a, from), local(a, to));
}

void Game::Impl::updateCamera(float dt, float t) {
    if (mode == "play") {
        if (input.lookDx != 0 || input.lookDy != 0) lastDrag = t;
        camYaw -= input.lookDx * 0.006f;
        camPitch = std::clamp(camPitch + input.lookDy * 0.004f, 0.2f, 1.25f);
        camDist = std::clamp(camDist + input.zoom, 5.0f, 26.0f);
    }
    input.lookDx = input.lookDy = input.zoom = 0;
    // swing round behind the walking direction
    float speed = std::hypot(player->velocity.x, player->velocity.z);
    if (settings.camTrack > 0 && speed > 0.5f && t - lastDrag > 1.5f) {
        float d = wrapAngle(player->yaw + PI - camYaw);
        camYaw += d * std::min(1.0f, dt * settings.camTrack * 2.5f * std::min(1.0f, speed / 4));
    }
    // follow the character
    float follow = 20 - settings.camSmooth * 17;
    glm::vec3 want(player->position.x, player->position.y + 0.9f, player->position.z);
    if (glm::length(camTarget - want) > 8) camTarget = want;
    else camTarget += (want - camTarget) * std::min(1.0f, dt * follow);

    bool inCaveNow = winter::isInCave(player->position.x);
    float pitch = camPitch + (inCaveNow ? 0.2f : 0);
    if (settings.camAvoid) {
        placeCam(pitch);
        bool blocked = cameraClearance(camTarget, camPos) < 1;
        camLift += ((blocked ? 0.5f : 0) - camLift) * std::min(1.0f, dt * 1.5f);
        placeCam(std::min(1.4f, pitch + camLift));
        float clear = cameraClearance(camTarget, camPos);
        camClear += (clear - camClear) * std::min(1.0f, dt * 1.5f);
        camPos = glm::mix(camTarget, camPos, std::max(0.55f, camClear));
    } else {
        camLift = 0;
        camClear = 1;
        placeCam(pitch);
    }
    float minY = world->groundAt(camPos.x, camPos.z) + 0.6f;
    if (camPos.y < minY) camPos.y = minY;
    camera.position = camPos;
    camera.lookAt(camTarget);
}

// ---------------------------------------------------------------------------
// one frame of the game (tick() in main.js)
// ---------------------------------------------------------------------------
void Game::Impl::tick(float dt, float t) {
    // timers (setTimeout)
    for (size_t i = 0; i < timers.size();) {
        if (timers[i].at <= t) {
            auto fn = std::move(timers[i].fn);
            timers.erase(timers.begin() + (long)i);
            fn();
        } else {
            ++i;
        }
    }
    glm::vec2 move = input.move();
    if (opt.autoKeys.find('w') != std::string::npos) move = {0, -1};
    bool sprintKey = input.sprint() || opt.autoKeys.find('r') != std::string::npos;
    bool upKey = input.up() || (opt.autoKeys.find('u') != std::string::npos && t > 0.3f);
    auto* ride = activeRide();
    bool riding = ride != nullptr;
    // moving gets you out of bed
    if (sleeping && glm::length(move) > 0.5f) wakeUp();
    if (sleeping) {
        player->ride(bedSeat(sleepBed), player->yaw);
        zzz->position = sleepBed + glm::vec3(0, 1.2f + std::sin(t * 2) * 0.12f, 0);
        zzz->material()->opacity = 0.6f + std::sin(t * 2) * 0.4f;
    }
    if (started && !boating && !riding && !sleeping) player->update(dt, move, camYaw, sprintKey, upKey);
    if (riding) {
        // the animal walks in its own area's coordinates
        glm::vec3 seat;
        float yaw = ride->update(t, dt, move, world->yawToLocal(rideArea, camYaw), sprintKey, seat);
        player->ride(global(rideArea, seat), world->yawToGlobal(rideArea, yaw));
    }
    for (auto& b : boats) b->update(t, dt, boating && b.get() == boat ? move : glm::vec2(0), camYaw, *world);
    if (boating) player->ride(boat->seat(), boat->heading());
    if (started && gliderPickup->update(t, *player)) gotGlider();
    combat->update(t, dt, canFight(), settings.time == "night");
    hs.hp = combat->hp;
    hs.maxHp = combat->maxHp;
    hs.level = combat->level;
    hs.xpK = (float)combat->xp / combat->xpNext();
    hs.specialK = combat->specialReady();
    hs.combat = area == "wilds" || combat->hp < combat->maxHp - 0.5f;

    // which area are we in now
    std::string a = world->areaAt(player->position.x, player->position.z);
    if (a != area) enterArea(a);
    // the quick-trip button points at whichever town is farther
    if (area == "wilds") {
        bool south = world->southness(player->position.z) > 0.5f;
        hs.realmIcon = south ? "❄️" : "🏰";
        hs.realmText = south ? "สู่ดินแดนหิมะ" : "สู่ยุคกลาง";
    }

    // the "E · ..." prompt
    bool nearBoat = !boating && nearestBoat();
    std::string tmpArea;
    const CubeWorld::Act* nearAct = !riding && !boating && !sleeping && started ? nearestAct(tmpArea) : nullptr;
    bool nearMount = !riding && !boating && !nearAct && started && nearestMount(tmpArea);
    hs.prompt = sleeping    ? "E · ลุกจากเตียง"
                : riding    ? "E · ลงจากหลัง"
                : boating   ? "E · ขึ้นฝั่ง"
                : nearAct   ? (nearAct->kind == "bed" ? "E · นอน" : "E · ทำอาหาร (🍒" + std::to_string(COOK_COST) + ")")
                : nearMount ? "E · ขี่"
                : nearBoat  ? "E · พายเรือ"
                            : "";

    if (mode == "play") {
        glm::vec3 p = player->position;
        warpCooldown -= dt;
        // the crystal cave is still reached by warping
        if (warpCooldown <= 0 && area == "winter") {
            const glm::vec3 CAVE_EXIT(winter::CAVE.x, 0, winter::CAVE.y + 2.6f);
            if (!winter::isInCave(p.x) && std::hypot(p.x - winter::CAVE_ENTRY.x, p.z - winter::CAVE_ENTRY.z) < 1.4f) warpTo(winter::CAVE_SPAWN, PI);
            if (winter::isInCave(p.x) && std::hypot(p.x - CAVE_EXIT.x, p.z - CAVE_EXIT.z) < 1.2f)
                warpTo(winter::CAVE_ENTRY + glm::vec3(-2.5f, 0, 0.5f), -PI / 2);
        }
        // first visit to the home in the snow -> fireworks
        if (area == "winter" && !homeCelebrated && glm::length(p - winter::HOME_DOOR) < 3) {
            homeCelebrated = true;
            hud.floatHearts(20, t);
            audio.fanfare();
            for (int i = 0; i < 6; ++i)
                later(i * 0.45f, [this] {
                    fireworks->launch({winter::HOME_DOOR.x + (frand() - 0.5f) * 12, 2, winter::HOME_DOOR.z - 6 + (frand() - 0.5f) * 6}, 12 + frand() * 6);
                });
        }
        checkFindPets(t);
        updateZone();
    }

    // berries in both towns + send the parrots to the cherries on the ground
    if (started) {
        int got = winterBushes->update(t, player->position) + medievalBushes->update(t, local("medieval", player->position)) +
                  wildsBushes->update(t, player->position);
        if (got) {
            settings.berries += got;
            persist();
            updateBerryHUD();
            audio.pop();
            hud.bumpBerry(t);
        }
    }
    thrower->update(dt);
    for (auto c : std::vector<std::shared_ptr<CherryThrower::Cherry>>(thrower->cherries)) {
        if (!c->landed || c->claimed) continue;
        std::vector<Pet*> free;
        for (Pet* b : parrots)
            if (b->found() && !b->busy()) free.push_back(b);
        if (free.empty()) break;
        glm::vec3 cp = c->obj->position;
        std::sort(free.begin(), free.end(), [&](Pet* a1, Pet* b) { return glm::length(a1->object->position - cp) < glm::length(b->object->position - cp); });
        c->claimed = true;
        free[0]->fetch(cp, [this, c] {
            thrower->remove(c);
            audio.pop();
            // Juice on the shoulder gets a little jealous
            Pet* juice = pet("juice");
            if (juice && juice->found() && frand() < 0.6f)
                later(0.7f, [this, juice] { juice->say(pick({"แบ่งจุ๋ยด้วย~", "จุ๋ยก็หิว!", "อยากกินบ้าง"}), elapsed); });
        });
    }

    if (started) {
        parkourWinter->update(t, player->position);
        parkourMedieval->update(t, local("medieval", player->position));
    }
    if (cubeWorld().available()) {
        cubeWorld().update(t, dt, *winter, player->position);
        cubeWorld().update(t, dt, *medieval, local("medieval", player->position));
        cubeWorld().update(t, dt, *world, player->position);
    }
    net->update(t, dt, *world, [this](const std::string& realm, glm::vec3& p, float& yaw) {
        if (realm != "winter" && realm != "medieval" && realm != "wilds") return false;
        p = global(realm, p);
        yaw = world->yawToGlobal(realm, yaw);
        return true;
    });

    int slot = 0;
    for (auto& p : pets) {
        if (boating && p->key == "puppy") {
            p->object->position = boat->bow();
            p->object->rotation.y = boat->heading();
            continue;
        }
        p->update(t, dt, *player, slot++, *world);
    }
    if (snowman->visible) {
        snowmanActor->billboard();
        snowmanActor->plane->rotation.z = std::sin(t * 1.5f) * 0.04f;
    }
    home->update(dt);
    hearth->intensity = 1.0f + std::sin(t * 9) * 0.12f + std::sin(t * 23) * 0.08f;

    // night in the snow map: fireworks
    if (settings.time == "night" && started && area == "winter" && !winter::isInCave(player->position.x) && t > nextFirework) {
        nextFirework = t + 6 + frand() * 10;
        fireworks->launch({(frand() - 0.5f) * 30, 2, -10 + (frand() - 0.5f) * 30}, 16 + frand() * 8);
    }
    fireworks->update(dt);
    footprints->update(t);
    world->update(t, dt, started ? player->position : glm::vec3(0));
    updateCamera(dt, t);
    g_spriteCameraYaw = camYaw;

    // snow falls over the village and the snowy half of the valley
    bool snowy = (area == "winter" && !winter::isInCave(camera.position.x)) || (area == "wilds" && world->southness(player->position.z) < 0.55f);
    snow->object->visible = settings.snow && snowy;
    snow->update(t, camera.position);

    if (started) {
        bool inCave = area == "winter" && winter::isInCave(player->position.x);
        std::vector<glm::vec3> lost;
        if (area == "winter")
            for (auto& p : pets)
                if (!p->found()) lost.push_back(p->position);
        // the map is drawn in the area's own coordinates
        float rot = world->area(area).rot;
        minimap->draw(local(area, player->position), player->yaw - rot, camYaw - rot, inCave, lost, t);
    }
}

// ---------------------------------------------------------------------------

Game::Game(const LaunchOptions& opt) : d(std::make_unique<Impl>()) { d->opt = opt; }

Game::~Game() {
    if (d->win) {
        glfwDestroyWindow(d->win);
        glfwTerminate();
    }
}

int Game::run() {
    Impl& g = *d;
    if (!g.openWindow()) {
        std::fprintf(stderr, "cannot create an OpenGL 4.1 window\n");
        return 1;
    }
    if (!g.renderer.init() || !g.hud.init()) return 1;
    g.build();

    // straight into the game (no title screen)
    g.startGame();
    if (g.opt.hasPos) {
        g.player->teleport({g.opt.pos.x, 0, g.opt.pos.y}, PI);
        g.enterArea(g.world->areaAt(g.opt.pos.x, g.opt.pos.y));
    }
    g.camYaw = 0;
    if (g.opt.hasCam) {
        g.camYaw = g.opt.cam.x;
        g.camPitch = g.opt.cam.y;
        g.camDist = g.opt.cam.z;
    }
    g.camTarget = g.player->position + glm::vec3(0, 0.9f, 0);

    const bool scripted = !g.opt.shotPath.empty();
    auto autoKey = [&](char k) { return g.opt.autoKeys.find(k) != std::string::npos; };
    double last = glfwGetTime(), startTime = last, autoJumpAt = 0, autoAttackAt = 0;
    bool autoGlided = false, autoJet = false, autoRide = false, autoSpin = false;
    int shotFrames = 0;

    while (!glfwWindowShouldClose(g.win) && !g.reload) {
        double now = glfwGetTime();
        float dt = std::min((float)(now - last), 0.05f);
        last = now;
        g.elapsed += dt;
        float t = g.elapsed;
        g_typed.clear();
        g_keyEvent = 0;
        glfwPollEvents();

        // ---------- keyboard ----------
        if (g.hs.codeOpen) {
            // typing into the secret-code box
            g.hs.codeText += g_typed;
            if (g_keyEvent == GLFW_KEY_BACKSPACE && !g.hs.codeText.empty()) {
                g.hs.codeText.pop_back();
                while (!g.hs.codeText.empty() && ((unsigned char)g.hs.codeText.back() & 0xc0) == 0x80) g.hs.codeText.pop_back();
            }
            if (g_keyEvent == GLFW_KEY_ESCAPE) {
                g.hs.codeOpen = false;
                g.hs.codeText.clear();
            }
            if (g_keyEvent == GLFW_KEY_ENTER || g_keyEvent == GLFW_KEY_KP_ENTER) g.submitCode();
            // swallow the key edges so they do not trigger game actions
            for (int k : {GLFW_KEY_ESCAPE, GLFW_KEY_E, GLFW_KEY_F, GLFW_KEY_U, GLFW_KEY_Y, GLFW_KEY_SPACE, GLFW_KEY_T, GLFW_KEY_R, GLFW_KEY_J, GLFW_KEY_K,
                          GLFW_KEY_Q, GLFW_KEY_LEFT_CONTROL})
                g.input.pressed(k);
        } else {
            // Esc closes the settings panel
            if (g.input.pressed(GLFW_KEY_ESCAPE)) g.hs.settingsOpen = false;
            bool en = g.input.enabled;
            if (g.input.pressed(GLFW_KEY_Y) && en) {
                g.hs.codeOpen = true;
                g.hs.codeText.clear();
            }
            if (g.input.pressed(GLFW_KEY_U) && en && g.CHAR != "patty" && !g.boating && !g.sleeping && !g.isRiding()) {
                g.player->setJetpack(!g.player->jetpack());
                g.audio.chime();
            }
            if (g.input.pressed(GLFW_KEY_SPACE) && en) {
                if (g.isRiding()) g.activeRide()->jump();
                else if (!g.boating) g.player->jump();
            }
            if (g.input.pressed(GLFW_KEY_E) && en) g.interact();
            if (g.input.pressed(GLFW_KEY_F) && en) g.throwCherry();
            // combat: J attack · K spin · Q / Ctrl dodge roll
            if (g.input.pressed(GLFW_KEY_J) && en && g.canFight()) g.combat->attack();
            if (g.input.pressed(GLFW_KEY_K) && en && g.canFight()) g.combat->special();
            bool ctrl = g.input.pressed(GLFW_KEY_LEFT_CONTROL);
            if ((g.input.pressed(GLFW_KEY_Q) || ctrl) && en) g.dodge(g.input.move());
            // desktop extras: T cycles the time of day, R reloads shaders
            if (g.input.pressed(GLFW_KEY_T) && en) g.setTime(g.settings.time == "day" ? "dusk" : g.settings.time == "dusk" ? "night" : "day");
            if (g.input.pressed(GLFW_KEY_R)) {
                g.renderer.init();
                reloadSharedShaders();
                std::printf("shaders reloaded\n");
            }
        }
        // ---------- gamepad: A jump · X interact · B throw · Y jetpack · right stick camera · d-pad zoom · Start settings ----------
        g.input.pollPad();
        if (g.input.hasPad && !g.hs.codeOpen) {
            bool en = g.input.enabled;
            if (g.input.padPressed(GLFW_GAMEPAD_BUTTON_A) && en) {
                if (g.isRiding()) g.activeRide()->jump();
                else if (!g.boating) g.player->jump();
            }
            if (g.input.padPressed(GLFW_GAMEPAD_BUTTON_X) && en) g.interact();
            if (g.input.padPressed(GLFW_GAMEPAD_BUTTON_B) && en) g.throwCherry();
            if (g.input.padPressed(GLFW_GAMEPAD_BUTTON_Y) && en && g.CHAR != "patty" && !g.boating && !g.sleeping && !g.isRiding()) {
                g.player->setJetpack(!g.player->jetpack());
                g.audio.chime();
            }
            if (g.input.padPressed(GLFW_GAMEPAD_BUTTON_START)) g.hs.settingsOpen = !g.hs.settingsOpen;
            // combat: RT attack · LT spin · LB dodge roll
            bool rt = g.input.pad.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] > 0.4f, lt = g.input.pad.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] > 0.4f;
            if (rt && !g.padRT && en && g.canFight()) g.combat->attack();
            if (lt && !g.padLT && en && g.canFight()) g.combat->special();
            g.padRT = rt;
            g.padLT = lt;
            if (g.input.padPressed(GLFW_GAMEPAD_BUTTON_LEFT_BUMPER) && en) g.dodge(g.input.move());
            // the right stick turns the camera like dragging the mouse
            float rx = Input::dead(g.input.pad.axes[GLFW_GAMEPAD_AXIS_RIGHT_X]), ry = Input::dead(g.input.pad.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]);
            if (en) {
                g.input.lookDx += rx * 520 * dt;
                g.input.lookDy += ry * 380 * dt;
            }
            if (g.input.pad.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP]) g.input.zoom -= 10 * dt;
            if (g.input.pad.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN]) g.input.zoom += 10 * dt;
        }

        // scripted keys for --shot runs (e = press E once after 0.3 s: board the boat / mount the pony)
        if (autoKey('u') && !autoJet && g.CHAR != "patty") {
            g.player->setJetpack(true);
            autoJet = true;
        }
        if (autoKey('e') && !autoRide && now - startTime > 0.3) {
            g.interact();
            autoRide = true;
        }
        // a = keep swinging, k = one spin attack
        if (autoKey('a') && now >= autoAttackAt && g.canFight()) {
            g.combat->attack();
            autoAttackAt = now + 0.2;
        }
        if (autoKey('k') && !autoSpin && now - startTime > 0.5) {
            g.combat->special();
            autoSpin = true;
        }
        if (autoKey('j') && now >= autoJumpAt) {
            g.player->jump();
            autoJumpAt = now + 0.6;
        }
        if (autoKey('g') && now - startTime < 0.05) {
            g.player->hasGlider = true;
            g.player->jump();
        }
        if (autoKey('g') && !autoGlided && now - startTime > 0.35) {
            g.player->jump();
            autoGlided = true;
        }

        // ---------- mouse: HUD clicks / drag to turn the camera / scroll to zoom ----------
        double mx, my;
        glfwGetCursorPos(g.win, &mx, &my);
        bool down = glfwGetMouseButton(g.win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        if (down && !g.dragging) {
            g.dragOnUi = g.hud.overUi((float)mx, (float)my);
            if (g.dragOnUi) g.handleUi(g.hud.click((float)mx, (float)my));
            g.pressMx = mx;
            g.pressMy = my;
            g.pressAt = now;
        }
        // released without dragging the camera: swing the sword
        if (!down && g.dragging && !g.dragOnUi && g.input.enabled && g.canFight() && std::hypot(mx - g.pressMx, my - g.pressMy) < 6 &&
            now - g.pressAt < 0.35)
            g.combat->attack();
        // right click: spin attack
        bool right = glfwGetMouseButton(g.win, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        if (right && !g.rightWasDown && g.input.enabled && g.canFight() && !g.hud.overUi((float)mx, (float)my)) g.combat->special();
        g.rightWasDown = right;
        std::string dragId = g.hud.drag((float)mx, (float)my, down);
        if (!dragId.empty()) {
            float v = std::stof(dragId.substr(dragId.find('=') + 1));
            if (dragId.rfind("camTrack", 0) == 0) g.settings.camTrack = v;
            else g.settings.camSmooth = v;
            g.persist();
        }
        if (down && g.dragging && !g.dragOnUi && g.mode == "play") {
            g.input.lookDx += (float)(mx - g.lastMx);
            g.input.lookDy += (float)(my - g.lastMy);
        }
        g.dragging = down;
        g.lastMx = mx;
        g.lastMy = my;
        g.input.zoom += -g_scroll * 0.4f;
        g_scroll = 0;

        g.tick(dt, t);

        int fw, fh, ww, wh;
        glfwGetFramebufferSize(g.win, &fw, &fh);
        glfwGetWindowSize(g.win, &ww, &wh);
        if (fw == 0 || fh == 0) continue;
        float scale = ww > 0 ? (float)fw / ww : 1;
        g.renderer.resize(fw, fh, g.settings.pixel);
        g.renderer.time = t;
        g.camera.aspect = (float)fw / fh;
        g.camera.updateProjection();
        g.renderer.render(*g.scene, g.camera, g.world->env);
        g.renderer.present();
        g.hud.draw(g.hs, fw, fh, scale, t, g.minimap.get());
        if (g.wantShot) g.takeShot(fw, fh);

        g.fpsFrames++;
        g.fpsTime += dt;
        if (g.fpsTime > 1) {
            g.hs.fps = (int)std::lround(g.fpsFrames / g.fpsTime);
            g.fpsFrames = 0;
            g.fpsTime = 0;
        }

        if (scripted && now - startTime > 0.5 + g.opt.shotDelay && ++shotFrames == 3) {
            std::vector<uint8_t> px((size_t)fw * fh * 3);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadBuffer(GL_BACK);
            glReadPixels(0, 0, fw, fh, GL_RGB, GL_UNSIGNED_BYTE, px.data());
            if (FILE* f = std::fopen(g.opt.shotPath.c_str(), "wb")) {
                std::fprintf(f, "P6\n%d %d\n255\n", fw, fh);
                for (int y = fh - 1; y >= 0; --y) std::fwrite(&px[(size_t)y * fw * 3], 1, (size_t)fw * 3, f);
                std::fclose(f);
                std::printf("player at (%.2f, %.2f, %.2f) %s%s%s · area %s · hp %.0f/%.0f lv %d xp %d kills %d · saved %s\n",
                            g.player->position.x, g.player->position.y, g.player->position.z, g.player->grounded ? "grounded" : "airborne",
                            g.boating ? " · boating" : "", g.isRiding() ? " · riding" : "", g.area.c_str(), g.combat->hp, g.combat->maxHp,
                            g.combat->level, g.combat->xp, g.combat->kills, g.opt.shotPath.c_str());
            }
            glfwSetWindowShouldClose(g.win, 1);
        }
        glfwSwapBuffers(g.win);
    }
    return g.reload ? 100 : 0;
}
