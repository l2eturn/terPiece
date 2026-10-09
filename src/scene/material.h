#pragma once
// Material equivalent of the three.js materials the web uses (Basic / Lambert / Standard / Sprite / ShaderMaterial).
#include <functional>
#include <memory>
#include <glm/glm.hpp>

class Texture;
class Shader;

enum class MaterialType {
    // MeshBasicMaterial: unlit
    Basic,
    // MeshLambertMaterial (also used for Phong in the old room)
    Lambert,
    // MeshStandardMaterial / MeshPhysicalMaterial (no environment map)
    Standard,
    // SpriteMaterial (only valid on Sprite objects)
    Sprite,
    // ShaderMaterial: `shader` + `uniforms` callback
    Custom,
};

enum class Side { Front, Back, Double };
enum class Blending { Normal, Additive };

struct Material {
    MaterialType type = MaterialType::Lambert;
    glm::vec3 color{1};
    std::shared_ptr<Texture> map;
    // texture offset / repeat (map.offset, map.repeat) — the sprite sheets animate this
    glm::vec2 mapOffset{0}, mapRepeat{1};
    // second colour map blended in by the per-vertex "blend" attribute (terrain: snow -> grass)
    std::shared_ptr<Texture> map2;
    glm::vec3 emissive{0};
    std::shared_ptr<Texture> emissiveMap;
    float emissiveIntensity = 1;
    float roughness = 1, metalness = 0;

    float opacity = 1;
    bool transparent = false;
    float alphaTest = 0;
    bool vertexColors = false;
    Blending blending = Blending::Normal;
    Side side = Side::Front;
    bool depthWrite = true, depthTest = true;
    bool fog = true;
    bool polygonOffset = false;
    float polygonOffsetFactor = 0, polygonOffsetUnits = 0;
    // optional clipping plane (normal.xyz, constant): keeps points where dot(n, p) + c >= 0
    bool clipping = false;
    glm::vec4 clipPlane{0, -1, 0, 0};

    // ShaderMaterial
    Shader* shader = nullptr;
    std::function<void(const Shader&)> uniforms;

    // unique id used for opaque sorting (like material.id)
    int id;
    Material();

    std::shared_ptr<Material> clone() const;
};
using MatPtr = std::shared_ptr<Material>;

// Shorthand constructors matching the JS one-liners
MatPtr basic(glm::vec3 color, std::shared_ptr<Texture> map = nullptr);
MatPtr lambert(std::shared_ptr<Texture> map, glm::vec3 color = glm::vec3(1));
MatPtr lambertColor(glm::vec3 color);
MatPtr standard(glm::vec3 color, float roughness, float metalness);
MatPtr spriteMaterial(std::shared_ptr<Texture> map, glm::vec3 color = glm::vec3(1), float opacity = 1);
