#pragma once
// Small helpers shared by every module (ports of the little utility functions the web code repeats).
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <glm/glm.hpp>

constexpr float PI = 3.14159265358979f;

// THREE.Color(0xrrggbb) in r151 legacy color mode: hex / 255 used directly as a linear value.
inline glm::vec3 hexc(uint32_t c) {
    return glm::vec3((c >> 16 & 255) / 255.f, (c >> 8 & 255) / 255.f, (c & 255) / 255.f);
}

inline float smoothstep(float a, float b, float x) {
    float t = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3 - 2 * t);
}

// Wrap an angle into (-PI, PI] the same way the JS does: atan2(sin, cos).
inline float wrapAngle(float a) { return std::atan2(std::sin(a), std::cos(a)); }

// Park-Miller generator, identical to rng(seed) in the JS files so seeded layouts match number for number.
struct Rng {
    int64_t s;
    explicit Rng(int64_t seed) : s(seed) {}
    double operator()() {
        s = (s * 16807) % 2147483647;
        return (double)(s - 1) / 2147483646.0;
    }
    float f() { return (float)(*this)(); }
};

// Math.random() replacement (unseeded on purpose, like the web).
inline float frand() {
    static std::mt19937 gen{std::random_device{}()};
    return std::uniform_real_distribution<float>(0.0f, 1.0f)(gen);
}

// THREE.Vector3().randomDirection()
inline glm::vec3 randomDirection() {
    float u = (frand() - 0.5f) * 2, t = frand() * PI * 2, f = std::sqrt(1 - u * u);
    return glm::vec3(f * std::cos(t), u, f * std::sin(t));
}
