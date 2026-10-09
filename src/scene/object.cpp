#include "scene/object.h"
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include "gfx/texture.h"

// ---------------------------------------------------------------------------
// Material
// ---------------------------------------------------------------------------

Material::Material() {
    static int next = 1;
    id = next++;
}

std::shared_ptr<Material> Material::clone() const {
    auto m = std::make_shared<Material>(*this);
    static int next = 1000000;
    m->id = next++;
    return m;
}

MatPtr basic(glm::vec3 color, std::shared_ptr<Texture> map) {
    auto m = std::make_shared<Material>();
    m->type = MaterialType::Basic;
    m->color = color;
    m->map = std::move(map);
    return m;
}

MatPtr lambert(std::shared_ptr<Texture> map, glm::vec3 color) {
    auto m = std::make_shared<Material>();
    m->type = MaterialType::Lambert;
    m->map = std::move(map);
    m->color = color;
    return m;
}

MatPtr lambertColor(glm::vec3 color) { return lambert(nullptr, color); }

MatPtr standard(glm::vec3 color, float roughness, float metalness) {
    auto m = std::make_shared<Material>();
    m->type = MaterialType::Standard;
    m->color = color;
    m->roughness = roughness;
    m->metalness = metalness;
    return m;
}

MatPtr spriteMaterial(std::shared_ptr<Texture> map, glm::vec3 color, float opacity) {
    auto m = std::make_shared<Material>();
    m->type = MaterialType::Sprite;
    m->map = std::move(map);
    m->color = color;
    m->opacity = opacity;
    // SpriteMaterial defaults to transparent
    m->transparent = true;
    return m;
}

// ---------------------------------------------------------------------------
// Object3D
// ---------------------------------------------------------------------------

Object3D& Object3D::add(const ObjPtr& child) {
    if (!child) return *this;
    if (child->parent) child->parent->remove(child.get());
    child->parent = this;
    children.push_back(child);
    return *this;
}

void Object3D::remove(Object3D* child) {
    auto it = std::find_if(children.begin(), children.end(), [&](const ObjPtr& c) { return c.get() == child; });
    if (it == children.end()) return;
    (*it)->parent = nullptr;
    children.erase(it);
}

void Object3D::removeFromParent() {
    if (parent) parent->remove(this);
}

void Object3D::traverse(const std::function<void(Object3D&)>& fn) {
    fn(*this);
    // copy: callbacks may add children
    auto kids = children;
    for (auto& c : kids) c->traverse(fn);
}

glm::quat eulerToQuat(glm::vec3 e) {
    // three.js Quaternion.setFromEuler, order 'XYZ'
    float c1 = std::cos(e.x / 2), c2 = std::cos(e.y / 2), c3 = std::cos(e.z / 2);
    float s1 = std::sin(e.x / 2), s2 = std::sin(e.y / 2), s3 = std::sin(e.z / 2);
    glm::quat q;
    q.x = s1 * c2 * c3 + c1 * s2 * s3;
    q.y = c1 * s2 * c3 - s1 * c2 * s3;
    q.z = c1 * c2 * s3 + s1 * s2 * c3;
    q.w = c1 * c2 * c3 - s1 * s2 * s3;
    return q;
}

glm::quat Object3D::localQuaternion() const { return useQuaternion ? quaternion : eulerToQuat(rotation); }

glm::mat4 composeMatrix(glm::vec3 p, glm::quat q, glm::vec3 s) {
    glm::mat4 m = glm::mat4_cast(q);
    m[0] *= s.x;
    m[1] *= s.y;
    m[2] *= s.z;
    m[3] = glm::vec4(p, 1);
    return m;
}

glm::mat4 Object3D::localMatrix() const { return composeMatrix(position, localQuaternion(), scale); }

void Object3D::updateMatrixWorld(const glm::mat4& parentWorld) {
    matrixWorld = parentWorld * localMatrix();
    for (auto& c : children) c->updateMatrixWorld(matrixWorld);
}

void Object3D::updateWorldFromParents() {
    glm::mat4 m = localMatrix();
    for (Object3D* p = parent; p; p = p->parent) m = p->localMatrix() * m;
    matrixWorld = m;
}

float Object3D::worldYaw() const {
    // Euler 'YXZ' y component of the world rotation
    glm::mat3 r(matrixWorld);
    for (int i = 0; i < 3; ++i) {
        float l = glm::length(r[i]);
        if (l > 0) r[i] /= l;
    }
    float m13 = r[2][0], m33 = r[2][2], m23 = r[2][1];
    if (std::abs(m23) < 0.9999999f) return std::atan2(m13, m33);
    float m31 = r[0][2], m11 = r[0][0];
    return std::atan2(-m31, m11);
}

void Object3D::lookAt(glm::vec3 target) {
    updateWorldFromParents();
    glm::vec3 eye = worldPosition();
    glm::vec3 z = target - eye;
    if (glm::length(z) < 1e-8f) return;
    z = glm::normalize(z);
    glm::vec3 up(0, 1, 0);
    glm::vec3 x = glm::cross(up, z);
    if (glm::length(x) < 1e-8f) {
        z.z += 0.0001f;
        x = glm::cross(up, glm::normalize(z));
    }
    x = glm::normalize(x);
    glm::vec3 y = glm::cross(z, x);
    glm::quat q = glm::quat_cast(glm::mat3(x, y, z));
    if (parent) {
        glm::mat3 pr(parent->matrixWorld);
        for (int i = 0; i < 3; ++i) pr[i] = glm::normalize(pr[i]);
        q = glm::inverse(glm::quat_cast(pr)) * q;
    }
    setQuaternion(q);
}

ObjPtr Object3D::clone() const {
    auto o = std::make_shared<Object3D>();
    o->name = name;
    o->kind = kind;
    o->position = position;
    o->rotation = rotation;
    o->scale = scale;
    o->useQuaternion = useQuaternion;
    o->quaternion = quaternion;
    o->visible = visible;
    o->castShadow = castShadow;
    o->receiveShadow = receiveShadow;
    o->renderOrder = renderOrder;
    o->geometry = geometry;
    o->materials = materials;
    o->multiMaterial = multiMaterial;
    o->instanced = instanced;
    o->instances = instances;
    o->lightColor = lightColor;
    o->intensity = intensity;
    o->distance = distance;
    o->decay = decay;
    o->cw = cw;
    for (auto& c : children) o->add(c->clone());
    return o;
}

ObjPtr makeGroup(const std::string& name) {
    auto o = std::make_shared<Object3D>();
    o->name = name;
    return o;
}

ObjPtr makeMesh(const GeoPtr& g, const MatPtr& m) {
    auto o = std::make_shared<Object3D>();
    o->kind = ObjectKind::Mesh;
    o->geometry = g;
    o->materials = {m};
    return o;
}

ObjPtr makeMesh(const GeoPtr& g, const std::vector<MatPtr>& mats) {
    auto o = makeMesh(g, mats.empty() ? nullptr : mats[0]);
    o->materials = mats;
    o->multiMaterial = mats.size() > 1;
    return o;
}

ObjPtr makeInstanced(const GeoPtr& g, const std::vector<MatPtr>& mats, size_t count) {
    auto o = makeMesh(g, mats);
    o->instanced = true;
    o->instances.assign(count, glm::mat4(1));
    return o;
}

ObjPtr makePoints(const GeoPtr& g, const MatPtr& m) {
    auto o = makeMesh(g, m);
    o->kind = ObjectKind::Points;
    return o;
}

ObjPtr makeSprite(const MatPtr& m) {
    auto o = std::make_shared<Object3D>();
    o->kind = ObjectKind::Sprite;
    o->materials = {m};
    return o;
}

ObjPtr makePointLight(glm::vec3 color, float intensity, float distance, float decay) {
    auto o = std::make_shared<Object3D>();
    o->kind = ObjectKind::PointLight;
    o->lightColor = color;
    o->intensity = intensity;
    o->distance = distance;
    o->decay = decay;
    return o;
}
