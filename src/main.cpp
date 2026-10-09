// terPiece — Patty's Realms (patty-project, three.js) ported to C++20 + OpenGL 4.1.
//
//   ./build/terpiece [--char patty|ter|momonga] [--time day|dusk|night] [--realm winter|medieval]
//                    [--pos x z] [--cam yaw pitch dist] [--play] [--title] [--server host:port]
//   ./build/terpiece --shot out.ppm [--shot-delay s] [--auto wrjgu]   save a frame and quit (for checking)
//
// Keys: WASD / arrows walk · Shift run · Space jump, in mid-air = glide · U jetpack (Ter / Momonga)
//       E interact · F throw a cherry · Y secret code · T time of day · Esc menu
//       drag = turn the camera · scroll = zoom · R reload shaders
#include <mach-o/dyld.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "game/game.h"

int main(int argc, char** argv) {
    LaunchOptions o;
    for (int i = 1; i < argc; ++i) {
        std::string s = argv[i];
        auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : "0"; };
        if (s == "--char") o.character = next();
        else if (s == "--time") o.time = next();
        else if (s == "--realm") o.realm = next();
        else if (s == "--shot") o.shotPath = next();
        else if (s == "--shot-delay") o.shotDelay = (float)std::atof(next());
        else if (s == "--auto") o.autoKeys = next();
        else if (s == "--play") o.skipTitle = true;
        else if (s == "--title") o.forceTitle = true;
        else if (s == "--server") o.server = next();
        else if (s == "--pos") {
            o.hasPos = true;
            o.pos.x = (float)std::atof(next());
            o.pos.y = (float)std::atof(next());
        } else if (s == "--cam") {
            o.hasCam = true;
            o.cam.x = (float)std::atof(next());
            o.cam.y = (float)std::atof(next());
            o.cam.z = (float)std::atof(next());
        } else {
            std::fprintf(stderr, "unknown argument: %s\n", s.c_str());
            return 2;
        }
    }
    if (!o.character.empty() && o.character != "patty" && o.character != "ter" && o.character != "momonga") {
        std::fprintf(stderr, "--char must be patty / ter / momonga\n");
        return 2;
    }
    // --shot runs skip the title screen
    if (!o.shotPath.empty() && !o.forceTitle) o.skipTitle = true;
    if (o.server.empty())
        if (const char* env = std::getenv("TERPIECE_SERVER")) o.server = env;
    int rc;
    {
        Game game(o);
        rc = game.run();
    }
    // 100 = the secret code switched character / progress was reset: restart like location.reload()
    if (rc == 100) {
        char path[4096];
        uint32_t size = sizeof path;
        if (_NSGetExecutablePath(path, &size) == 0) execv(path, argv);
    }
    return rc;
}
