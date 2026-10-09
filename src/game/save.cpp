#include "game/save.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

using nlohmann::json;
namespace fs = std::filesystem;

namespace {

fs::path dir() {
    const char* home = std::getenv("HOME");
    fs::path d = fs::path(home ? home : ".") / "Library" / "Application Support" / "terPiece";
    std::error_code ec;
    fs::create_directories(d, ec);
    return d;
}

// same keys as the web build
std::string saveKey(const std::string& c) { return c == "patty" ? "patty-xmas-v2" : "patty-xmas-" + c; }

}  // namespace

namespace save {

std::string loadCharacter() {
    std::ifstream f(dir() / "patty-xmas-char.txt");
    std::string c;
    if (f) std::getline(f, c);
    return c == "ter" || c == "momonga" ? c : "patty";
}

void saveCharacter(const std::string& c) {
    std::ofstream f(dir() / "patty-xmas-char.txt");
    f << c;
}

Settings load(const std::string& character) {
    Settings s;
    std::ifstream f(dir() / (saveKey(character) + ".json"));
    if (!f) return s;
    json j = json::parse(f, nullptr, false);
    if (!j.is_object()) return s;
    s.realm = j.value("realm", s.realm);
    s.time = j.value("time", s.time);
    s.pixel = j.value("pixel", s.pixel);
    s.tilt = j.value("tilt", s.tilt);
    s.shadows = j.value("shadows", s.shadows);
    s.snow = j.value("snow", s.snow);
    s.camAvoid = j.value("camAvoid", s.camAvoid);
    s.camTrack = j.value("camTrack", s.camTrack);
    s.camSmooth = j.value("camSmooth", s.camSmooth);
    s.berries = j.value("berries", s.berries);
    s.found = j.value("found", s.found);
    s.stars = j.value("stars", s.stars);
    s.glider = j.value("glider", s.glider);
    return s;
}

void store(const std::string& character, const Settings& s) {
    json j = {{"realm", s.realm},       {"time", s.time},         {"pixel", s.pixel},         {"tilt", s.tilt},
              {"shadows", s.shadows},   {"snow", s.snow},         {"camAvoid", s.camAvoid},   {"camTrack", s.camTrack},
              {"camSmooth", s.camSmooth}, {"berries", s.berries}, {"found", s.found},         {"stars", s.stars},
              {"glider", s.glider}};
    std::ofstream f(dir() / (saveKey(character) + ".json"));
    f << j.dump(2);
}

}  // namespace save
