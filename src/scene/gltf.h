#pragma once
// glTF 2.0 / .glb loading (cgltf + meshoptimizer decoder + ImageIO for textures), producing the same
// scene graph and materials GLTFLoader builds in three.js (MeshStandardMaterial, vertex colours, flipY = false).
#include <map>
#include <optional>
#include <string>
#include <vector>
#include "scene/object.h"

struct GltfAnimation {
    struct Channel {
        Object3D* node;
        // 0 = translation, 1 = rotation, 2 = scale
        int path;
        // 0 = linear, 1 = step, 2 = cubic spline
        int interp;
        std::vector<float> times;
        std::vector<glm::vec4> values;
    };
    std::string name;
    float duration = 0;
    std::vector<Channel> channels;
};

struct GltfScene {
    ObjPtr root;
    std::vector<GltfAnimation> animations;
};

std::optional<GltfScene> loadGltf(const std::string& path);
// glTF material name of a loaded material (home.cpp swaps materials by name)
std::map<const Material*, std::string>& gltfMaterialNames();

// Minimal AnimationMixer: plays every clip looping
class AnimationMixer {
public:
    explicit AnimationMixer(std::vector<GltfAnimation> clips) : clips_(std::move(clips)) {}
    void update(float dt);

private:
    std::vector<GltfAnimation> clips_;
    float time_ = 0;
};
