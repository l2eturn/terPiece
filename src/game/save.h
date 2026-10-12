#pragma once
// Settings / progress kept between runs (localStorage 'patty-xmas-v2' / 'patty-xmas-<char>' on the web)
// -> JSON files in ~/Library/Application Support/terPiece/
#include <string>
#include <vector>

struct Settings {
    // winter | medieval
    std::string realm = "winter";
    std::string time = "dusk";
    int pixel = 2;
    bool tilt = false;
    bool shadows = true;
    bool snow = true;
    // camera dodges trees / houses automatically
    bool camAvoid = false;
    // 0 = fixed camera angle, 1 = swing fully behind the walking direction
    float camTrack = 0.25f;
    // 0 = follows instantly, 1 = very smooth
    float camSmooth = 0.5f;
    int berries = 0;
    // companions already found
    std::vector<std::string> found;
    // parkour courses finished
    std::vector<std::string> stars;
    bool glider = false;
    // combat progress (desktop only — the web build has no fighting)
    int level = 1;
    int xp = 0;
};

namespace save {
// "patty" / "ter" / "momonga" chosen with the secret code (patty-xmas-char)
std::string loadCharacter();
void saveCharacter(const std::string& c);
Settings load(const std::string& character);
void store(const std::string& character, const Settings& s);
}  // namespace save
