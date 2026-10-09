#pragma once
// Forward renderer that behaves like three.js r151 WebGLRenderer for what the game uses:
// Lambert/Standard/Basic/Sprite/Shader materials, hemisphere + directional (PCF soft shadow) + point lights,
// linear fog, opaque/transparent sorting, then the post chain of postfx.js
// (4x MSAA half-float target at 1/pixel resolution -> optional tilt-shift -> grade + sRGB -> screen, nearest upscale).
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "gfx/shader.h"
#include "scene/object.h"

struct Camera {
    float fov = 32, aspect = 1, near = 0.1f, far = 900;
    glm::vec3 position{0};
    glm::mat4 view{1}, proj{1};
    void lookAt(glm::vec3 target) { view = glm::lookAt(position, target, glm::vec3(0, 1, 0)); }
    void updateProjection() { proj = glm::perspective(glm::radians(fov), aspect, near, far); }
};

// Scene-wide lighting (three.js HemisphereLight + one DirectionalLight + Fog)
struct Environment {
    glm::vec3 hemiSky{1}, hemiGround{0};
    float hemiIntensity = 0;
    glm::vec3 sunColor{1};
    float sunIntensity = 0;
    // light position and target (direction = position - target)
    glm::vec3 sunPosition{0, 10, 0}, sunTarget{0};
    bool sunShadow = true;
    // orthographic shadow camera
    float shadowHalf = 14, shadowNear = 1, shadowFar = 80;
    float shadowBias = -0.0008f, shadowNormalBias = 0.03f;
    glm::vec3 fogColor{0};
    float fogNear = 22, fogFar = 90;
};

class Renderer {
public:
    bool init();
    // framebuffer size in device pixels; pixel = 1/2/3 (the "Pixel art" setting)
    void resize(int fbWidth, int fbHeight, int pixel);
    void render(Object3D& scene, const Camera& cam, const Environment& env);
    // draw the post chain to the default framebuffer
    void present();

    bool shadows = true;
    bool tiltShift = false;
    float tiltAmount = 0.35f;
    int shadowMapSize = 2048;

    // size of the scene target (device pixels / pixel)
    int width() const { return w_; }
    int height() const { return h_; }
    int fbWidth() const { return fbw_; }
    int fbHeight() const { return fbh_; }
    float time = 0;

    // shared unit quad used by sprites
    GeoPtr spriteQuad;

private:
    struct Item {
        Object3D* obj;
        Geometry* geo;
        Material* mat;
        uint32_t start, count;
        float z;
    };
    struct PointLightData {
        glm::vec3 pos, color;
        float distance, decay;
    };

    void collect(Object3D& o, const glm::mat4& viewProj);
    void drawItem(const Item& it, const Camera& cam, const Environment& env, bool depthOnly);
    void bindMaterialState(const Material& m, bool depthOnly);
    void setupShadow(const Environment& env);
    void bindInstances(Object3D& o);

    Shader mesh_, depth_, sprite_, post_, tilt_;
    std::vector<Item> opaque_, transparent_;
    std::vector<PointLightData> lights_;
    glm::mat4 shadowMatrix_{1}, shadowViewProj_{1};
    bool shadowActive_ = false;

    GLuint msFbo_ = 0, msColor_ = 0, msDepth_ = 0;
    GLuint resolveFbo_ = 0, resolveTex_ = 0;
    GLuint pingFbo_ = 0, pingTex_ = 0;
    GLuint shadowFbo_ = 0, shadowTex_ = 0;
    GLuint emptyVao_ = 0;
    std::shared_ptr<Texture> white_;
    int w_ = 0, h_ = 0, fbw_ = 0, fbh_ = 0, allocatedShadow_ = 0;
};
