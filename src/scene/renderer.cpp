#include "scene/renderer.h"
#include <algorithm>
#include <cstdio>
#include <glm/gtc/matrix_inverse.hpp>
#include "gfx/texture.h"

namespace {
constexpr int kMaxLights = 16;

// plane extraction for sphere-vs-frustum culling
struct Frustum {
    glm::vec4 p[6];
    explicit Frustum(const glm::mat4& m) {
        glm::mat4 t = glm::transpose(m);
        p[0] = t[3] + t[0];
        p[1] = t[3] - t[0];
        p[2] = t[3] + t[1];
        p[3] = t[3] - t[1];
        p[4] = t[3] + t[2];
        p[5] = t[3] - t[2];
        for (auto& q : p) q /= glm::length(glm::vec3(q));
    }
    bool sphere(glm::vec3 c, float r) const {
        for (auto& q : p)
            if (glm::dot(glm::vec3(q), c) + q.w < -r) return false;
        return true;
    }
};

float maxScale(const glm::mat4& m) {
    return std::max({glm::length(glm::vec3(m[0])), glm::length(glm::vec3(m[1])), glm::length(glm::vec3(m[2]))});
}
}  // namespace

bool Renderer::init() {
    if (!mesh_.load("mesh") || !depth_.load("depth") || !sprite_.load("sprite") || !post_.load("post") || !tilt_.load("tilt")) return false;
    glGenVertexArrays(1, &emptyVao_);
    white_ = std::make_shared<Texture>();
    white_->upload(1, 1, {255, 255, 255, 255});
    // THREE.Sprite geometry: unit quad centred on the origin
    spriteQuad = std::make_shared<Geometry>();
    spriteQuad->position = {{-0.5f, -0.5f, 0}, {0.5f, -0.5f, 0}, {0.5f, 0.5f, 0}, {-0.5f, 0.5f, 0}};
    spriteQuad->uv = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    spriteQuad->normal.assign(4, glm::vec3(0, 0, 1));
    spriteQuad->index = {0, 1, 2, 0, 2, 3};
    glEnable(GL_PROGRAM_POINT_SIZE);
    return true;
}

void Renderer::resize(int fbw, int fbh, int pixel) {
    int w = std::max(1, fbw / std::max(1, pixel)), h = std::max(1, fbh / std::max(1, pixel));
    fbw_ = fbw;
    fbh_ = fbh;
    if (w == w_ && h == h_ && msFbo_) return;
    w_ = w;
    h_ = h;
    if (msFbo_) {
        glDeleteFramebuffers(1, &msFbo_);
        glDeleteRenderbuffers(1, &msColor_);
        glDeleteRenderbuffers(1, &msDepth_);
        glDeleteFramebuffers(1, &resolveFbo_);
        glDeleteTextures(1, &resolveTex_);
        glDeleteFramebuffers(1, &pingFbo_);
        glDeleteTextures(1, &pingTex_);
    }
    // multisampled half-float scene target (WebGLRenderTarget samples: 4, HalfFloatType)
    glGenFramebuffers(1, &msFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    glGenRenderbuffers(1, &msColor_);
    glBindRenderbuffer(GL_RENDERBUFFER, msColor_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA16F, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msColor_);
    glGenRenderbuffers(1, &msDepth_);
    glBindRenderbuffer(GL_RENDERBUFFER, msDepth_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT24, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msDepth_);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) std::fprintf(stderr, "renderer: MSAA target incomplete\n");
    auto colorTarget = [&](GLuint& fbo, GLuint& tex) {
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    };
    colorTarget(resolveFbo_, resolveTex_);
    colorTarget(pingFbo_, pingTex_);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::setupShadow(const Environment& env) {
    shadowActive_ = shadows && env.sunShadow && env.sunIntensity > 0;
    if (!shadowActive_) return;
    if (allocatedShadow_ != shadowMapSize) {
        if (shadowFbo_) {
            glDeleteFramebuffers(1, &shadowFbo_);
            glDeleteTextures(1, &shadowTex_);
        }
        glGenFramebuffers(1, &shadowFbo_);
        glGenTextures(1, &shadowTex_);
        glBindTexture(GL_TEXTURE_2D, shadowTex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, shadowMapSize, shadowMapSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTex_, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        allocatedShadow_ = shadowMapSize;
    }
    float s = env.shadowHalf;
    glm::mat4 view = glm::lookAt(env.sunPosition, env.sunTarget, glm::vec3(0, 1, 0));
    glm::mat4 proj = glm::ortho(-s, s, -s, s, env.shadowNear, env.shadowFar);
    shadowViewProj_ = proj * view;
    glm::mat4 bias(0.5f);
    bias[3] = glm::vec4(0.5f, 0.5f, 0.5f, 1);
    shadowMatrix_ = bias * shadowViewProj_;
}

void Renderer::collect(Object3D& o, const glm::mat4& viewProj) {
    if (!o.visible) return;
    if (o.kind == ObjectKind::PointLight) {
        if (o.intensity > 0)
            lights_.push_back({o.worldPosition(), o.lightColor * o.intensity, o.distance, o.decay});
    } else if (o.kind != ObjectKind::Group) {
        if (o.onBeforeRender) o.onBeforeRender(o);
        Geometry* g = o.kind == ObjectKind::Sprite ? spriteQuad.get() : o.geometry.get();
        if (g && !o.materials.empty()) {
            if (g->sphereRadius == 0 && !g->position.empty()) g->computeBounds();
            glm::vec3 c = glm::vec3(o.matrixWorld * glm::vec4(g->sphereCenter, 1));
            glm::vec4 clip = viewProj * glm::vec4(c, 1);
            float z = clip.w;
            auto push = [&](Material* m, uint32_t start, uint32_t count) {
                if (!m) return;
                Item it{&o, g, m, start, count, z};
                (m->transparent ? transparent_ : opaque_).push_back(it);
            };
            if (o.multiMaterial && !g->groups.empty()) {
                for (auto& gr : g->groups) {
                    int mi = std::clamp(gr.materialIndex, 0, (int)o.materials.size() - 1);
                    push(o.materials[mi].get(), gr.start, gr.count);
                }
            } else {
                push(o.materials[0].get(), 0, g->drawCount());
            }
        }
    }
    for (auto& c : o.children) collect(*c, viewProj);
}

void Renderer::bindInstances(Object3D& o) {
    if (!o.instanced) {
        for (int i = 0; i < 4; ++i) {
            glDisableVertexAttribArray(4 + i);
            glm::vec4 col(0);
            col[i] = 1;
            glVertexAttrib4f(4 + i, col.x, col.y, col.z, col.w);
        }
        return;
    }
    if (!o.instanceVbo) glGenBuffers(1, &o.instanceVbo);
    glBindBuffer(GL_ARRAY_BUFFER, o.instanceVbo);
    if (o.instancesDirty) {
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(o.instances.size() * sizeof(glm::mat4)), o.instances.data(), GL_DYNAMIC_DRAW);
        o.instancesDirty = false;
    }
    for (int i = 0; i < 4; ++i) {
        glEnableVertexAttribArray(4 + i);
        glVertexAttribPointer(4 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(sizeof(glm::vec4) * i));
        glVertexAttribDivisor(4 + i, 1);
    }
}

void Renderer::bindMaterialState(const Material& m, bool depthOnly) {
    // cull mode; the shadow pass renders the opposite side (three.js shadowSide default)
    Side side = m.side;
    if (depthOnly) side = side == Side::Front ? Side::Back : side == Side::Back ? Side::Front : Side::Double;
    if (side == Side::Double) glDisable(GL_CULL_FACE);
    else {
        glEnable(GL_CULL_FACE);
        glCullFace(side == Side::Front ? GL_BACK : GL_FRONT);
    }
    if (depthOnly) return;
    if (m.depthTest) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);
    glDepthMask(m.depthWrite ? GL_TRUE : GL_FALSE);
    if (m.transparent || m.blending == Blending::Additive) {
        glEnable(GL_BLEND);
        if (m.blending == Blending::Additive) glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        else glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }
    if (m.polygonOffset) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(m.polygonOffsetFactor, m.polygonOffsetUnits);
    } else {
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
}

void Renderer::drawItem(const Item& it, const Camera& cam, const Environment& env, bool depthOnly) {
    Object3D& o = *it.obj;
    Material& m = *it.mat;
    const Shader* sh;
    if (depthOnly) sh = &depth_;
    else if (m.type == MaterialType::Custom && m.shader) sh = m.shader;
    else if (o.kind == ObjectKind::Sprite) sh = &sprite_;
    else sh = &mesh_;
    sh->use();
    bindMaterialState(m, depthOnly);

    glm::mat4 view = depthOnly ? glm::mat4(1) : cam.view;
    glm::mat4 proj = depthOnly ? shadowViewProj_ : cam.proj;
    sh->set("uModel", o.matrixWorld);
    sh->set("uView", view);
    sh->set("uProj", proj);
    sh->set("uInstanced", o.instanced ? 1 : 0);
    sh->set("uNormalMatrix", glm::inverseTranspose(glm::mat3(o.matrixWorld)));
    sh->set("uMapOffset", m.mapOffset);
    sh->set("uMapRepeat", m.mapRepeat);
    sh->set("uCamPos", cam.position);
    sh->set("uTime", time);
    sh->set("uResolution", glm::vec2(w_, h_));

    // textures: 0 = map, 1 = emissiveMap, 2 = shadow map
    (m.map ? m.map.get() : white_.get())->bind(0);
    sh->set("uMap", 0);
    sh->set("uHasMap", m.map ? 1 : 0);
    sh->set("uAlphaTest", m.alphaTest);
    sh->set("uOpacity", m.opacity);

    if (!depthOnly && sh != &depth_) {
        (m.map2 ? m.map2.get() : white_.get())->bind(3);
        sh->set("uMap2", 3);
        sh->set("uHasMap2", m.map2 ? 1 : 0);
        (m.emissiveMap ? m.emissiveMap.get() : white_.get())->bind(1);
        sh->set("uEmissiveMap", 1);
        sh->set("uHasEmissiveMap", m.emissiveMap ? 1 : 0);
        sh->set("uType", m.type == MaterialType::Basic ? 0 : m.type == MaterialType::Standard ? 2 : 1);
        sh->set("uColor", m.color);
        sh->set("uEmissive", m.emissive * m.emissiveIntensity);
        sh->set("uRoughness", m.roughness);
        sh->set("uMetalness", m.metalness);
        sh->set("uVertexColors", m.vertexColors ? 1 : 0);
        sh->set("uSide", m.side == Side::Front ? 0 : m.side == Side::Back ? 1 : 2);
        sh->set("uFog", m.fog ? 1 : 0);
        sh->set("uFogColor", env.fogColor);
        sh->set("uFogNear", env.fogNear);
        sh->set("uFogFar", env.fogFar);
        sh->set("uClip", m.clipping ? 1 : 0);
        sh->set("uClipPlane", m.clipPlane);
        sh->set("uHemiSky", env.hemiSky * env.hemiIntensity);
        sh->set("uHemiGround", env.hemiGround * env.hemiIntensity);
        sh->set("uSunColor", env.sunColor * env.sunIntensity);
        glm::vec3 sd = env.sunPosition - env.sunTarget;
        sh->set("uSunDir", glm::length(sd) > 0 ? glm::normalize(sd) : glm::vec3(0, 1, 0));
        sh->set("uReceiveShadow", (o.receiveShadow && shadowActive_) ? 1 : 0);
        sh->set("uShadowMatrix", shadowMatrix_);
        sh->set("uShadowBias", env.shadowBias);
        sh->set("uShadowNormalBias", env.shadowNormalBias);
        sh->set("uShadowTexel", 1.0f / (float)std::max(1, shadowMapSize));
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, shadowActive_ ? shadowTex_ : 0);
        sh->set("uShadowMap", 2);
        int n = (int)lights_.size();
        sh->set("uNumLights", n);
        if (n) {
            glm::vec3 pos[kMaxLights], col[kMaxLights];
            float dist[kMaxLights], dec[kMaxLights];
            for (int i = 0; i < n; ++i) {
                pos[i] = lights_[i].pos;
                col[i] = lights_[i].color;
                dist[i] = lights_[i].distance;
                dec[i] = lights_[i].decay;
            }
            sh->setArray("uLightPos", pos, n);
            sh->setArray("uLightColor", col, n);
            sh->setArray("uLightDist", dist, n);
            sh->setArray("uLightDecay", dec, n);
        }
        if (m.type == MaterialType::Custom && m.uniforms) m.uniforms(*sh);
    }

    it.geo->bind();
    bindInstances(o);
    GLenum mode = o.kind == ObjectKind::Points ? GL_POINTS : GL_TRIANGLES;
    it.geo->drawRange(mode, it.start, it.count, o.instanced ? (int)o.instances.size() : 0);
    if (o.instanced)
        for (int i = 0; i < 4; ++i) glVertexAttribDivisor(4 + i, 0);
}

void Renderer::render(Object3D& scene, const Camera& cam, const Environment& env) {
    scene.updateMatrixWorld();
    opaque_.clear();
    transparent_.clear();
    lights_.clear();
    glm::mat4 vp = cam.proj * cam.view;
    collect(scene, vp);
    // keep the point lights that matter most here (both towns are in the scene at once)
    std::sort(lights_.begin(), lights_.end(), [&](const PointLightData& a, const PointLightData& b) {
        float da = glm::length(a.pos - cam.position) - a.distance, db = glm::length(b.pos - cam.position) - b.distance;
        return da < db;
    });
    if (lights_.size() > (size_t)kMaxLights) lights_.resize(kMaxLights);

    // frustum culling of plain meshes (three.js frustumCulled)
    Frustum fr(vp);
    auto culled = [&](const Item& it) {
        Object3D& o = *it.obj;
        if (o.instanced || o.kind == ObjectKind::Points || o.kind == ObjectKind::Sprite) return false;
        glm::vec3 c = glm::vec3(o.matrixWorld * glm::vec4(it.geo->sphereCenter, 1));
        return !fr.sphere(c, it.geo->sphereRadius * maxScale(o.matrixWorld));
    };

    // ---------- shadow pass ----------
    setupShadow(env);
    if (shadowActive_) {
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
        glViewport(0, 0, shadowMapSize, shadowMapSize);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glDisable(GL_POLYGON_OFFSET_FILL);
        glClear(GL_DEPTH_BUFFER_BIT);
        Frustum sf(shadowViewProj_);
        for (auto* list : {&opaque_, &transparent_})
            for (auto& it : *list) {
                Object3D& o = *it.obj;
                if (!o.castShadow || o.kind != ObjectKind::Mesh || it.mat->type == MaterialType::Custom) continue;
                if (!o.instanced) {
                    glm::vec3 c = glm::vec3(o.matrixWorld * glm::vec4(it.geo->sphereCenter, 1));
                    if (!sf.sphere(c, it.geo->sphereRadius * maxScale(o.matrixWorld))) continue;
                }
                drawItem(it, cam, env, true);
            }
    }

    // ---------- main pass ----------
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    glViewport(0, 0, w_, h_);
    glDepthMask(GL_TRUE);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    std::stable_sort(opaque_.begin(), opaque_.end(), [](const Item& a, const Item& b) {
        if (a.obj->renderOrder != b.obj->renderOrder) return a.obj->renderOrder < b.obj->renderOrder;
        if (a.mat->id != b.mat->id) return a.mat->id < b.mat->id;
        return a.z < b.z;
    });
    std::stable_sort(transparent_.begin(), transparent_.end(), [](const Item& a, const Item& b) {
        if (a.obj->renderOrder != b.obj->renderOrder) return a.obj->renderOrder < b.obj->renderOrder;
        return a.z > b.z;
    });
    for (auto* list : {&opaque_, &transparent_})
        for (auto& it : *list) {
            if (culled(it)) continue;
            drawItem(it, cam, env, false);
        }
    glDisable(GL_BLEND);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);

    // resolve MSAA
    glBindFramebuffer(GL_READ_FRAMEBUFFER, msFbo_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFbo_);
    glBlitFramebuffer(0, 0, w_, h_, 0, 0, w_, h_, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::present() {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(emptyVao_);
    GLuint src = resolveTex_;
    if (tiltShift) {
        // horizontal pass into ping, vertical pass back into the resolve texture
        tilt_.use();
        tilt_.set("uRes", glm::vec2(w_, h_));
        tilt_.set("uAmount", tiltAmount);
        tilt_.set("uTex", 0);
        glViewport(0, 0, w_, h_);
        glActiveTexture(GL_TEXTURE0);
        glBindFramebuffer(GL_FRAMEBUFFER, pingFbo_);
        glBindTexture(GL_TEXTURE_2D, resolveTex_);
        tilt_.set("uDir", glm::vec2(1, 0));
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindFramebuffer(GL_FRAMEBUFFER, resolveFbo_);
        glBindTexture(GL_TEXTURE_2D, pingTex_);
        tilt_.set("uDir", glm::vec2(0, 1));
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, fbw_, fbh_);
    post_.use();
    post_.set("uTex", 0);
    post_.set("uVignette", 0.35f);
    post_.set("uWarm", 0.04f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, src);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glEnable(GL_DEPTH_TEST);
}
