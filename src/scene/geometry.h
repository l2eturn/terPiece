#pragma once
// BufferGeometry equivalent. Generators are line-by-line ports of three.js r151 so vertex order,
// UVs and normals match the web build (worldUV() and flat shading depend on that).
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "core/math.h"
#include "gfx/gl.h"

struct GeometryGroup {
    uint32_t start = 0, count = 0;
    int materialIndex = 0;
};

// Extra per-vertex float attribute for custom shaders (bound at location 8 + its slot)
struct ExtraAttribute {
    std::string name;
    int size = 1;
    std::vector<float> data;
};

class Geometry {
public:
    Geometry() = default;
    ~Geometry();
    Geometry(const Geometry&) = delete;
    Geometry& operator=(const Geometry&) = delete;

    std::vector<glm::vec3> position, normal, color;
    std::vector<glm::vec2> uv;
    std::vector<uint32_t> index;
    std::vector<GeometryGroup> groups;
    std::vector<ExtraAttribute> extra;

    bool indexed() const { return !index.empty(); }
    // number of elements a full draw covers (indices, or vertices when not indexed)
    uint32_t drawCount() const { return indexed() ? (uint32_t)index.size() : (uint32_t)position.size(); }

    // ---------- editing (all return *this like three.js) ----------
    Geometry& applyMatrix(const glm::mat4& m);
    Geometry& translate(float x, float y, float z);
    Geometry& rotateX(float a);
    Geometry& rotateY(float a);
    Geometry& rotateZ(float a);
    Geometry& scale(float x, float y, float z);
    Geometry& scale(float s) { return scale(s, s, s); }
    // move the bounding-box centre to the origin
    Geometry& center();
    void computeVertexNormals();
    std::shared_ptr<Geometry> toNonIndexed() const;
    std::shared_ptr<Geometry> clone() const;
    ExtraAttribute* attribute(const std::string& name);

    // ---------- bounds ----------
    void computeBounds();
    glm::vec3 bbMin{0}, bbMax{0}, sphereCenter{0};
    float sphereRadius = 0;

    // Call after editing the arrays of a geometry that is already on the GPU
    void markDirty() { dirty_ = true; }
    // Upload if needed and bind the VAO
    void bind();
    void drawRange(GLenum mode, uint32_t start, uint32_t count, int instances = 0);

private:
    void upload();
    GLuint vao_ = 0, ebo_ = 0;
    GLuint vbo_[4 + 4] = {};
    bool dirty_ = true;
    bool boundsValid_ = false;
};
using GeoPtr = std::shared_ptr<Geometry>;

// THREE.Shape: a closed outline in the XY plane
struct Shape {
    std::vector<glm::vec2> pts;
    Shape& moveTo(float x, float y) { pts.push_back({x, y}); return *this; }
    Shape& lineTo(float x, float y) { pts.push_back({x, y}); return *this; }
    // quadraticCurveTo, sampled with 12 segments like ExtrudeGeometry's default curveSegments
    Shape& quadTo(float cx, float cy, float x, float y) {
        glm::vec2 p0 = pts.empty() ? glm::vec2(0) : pts.back(), c(cx, cy), p1(x, y);
        for (int i = 1; i <= 12; ++i) {
            float t = i / 12.0f, u = 1 - t;
            pts.push_back(u * u * p0 + 2 * u * t * c + t * t * p1);
        }
        return *this;
    }
};

struct ExtrudeOptions {
    float depth = 1;
    bool bevelEnabled = true;
    float bevelThickness = 0.2f, bevelSize = 0.1f;
    int bevelSegments = 3;
};

namespace geo {
GeoPtr box(float w = 1, float h = 1, float d = 1, int ws = 1, int hs = 1, int ds = 1);
GeoPtr cylinder(float radiusTop = 1, float radiusBottom = 1, float height = 1, int radialSegments = 32, int heightSegments = 1,
                bool openEnded = false, float thetaStart = 0, float thetaLength = PI * 2);
GeoPtr cone(float radius = 1, float height = 1, int radialSegments = 32, int heightSegments = 1, bool openEnded = false,
            float thetaStart = 0, float thetaLength = PI * 2);
GeoPtr sphere(float radius = 1, int widthSegments = 32, int heightSegments = 16, float phiStart = 0, float phiLength = PI * 2,
              float thetaStart = 0, float thetaLength = PI);
GeoPtr plane(float w = 1, float h = 1, int ws = 1, int hs = 1);
GeoPtr circle(float radius = 1, int segments = 32, float thetaStart = 0, float thetaLength = PI * 2);
GeoPtr torus(float radius = 1, float tube = 0.4f, int radialSegments = 12, int tubularSegments = 48, float arc = PI * 2);
GeoPtr dodecahedron(float radius = 1, int detail = 0);
GeoPtr octahedron(float radius = 1);
GeoPtr icosahedron(float radius = 1);
GeoPtr shape(const Shape& s);
GeoPtr extrude(const Shape& s, const ExtrudeOptions& o);
// Raw triangles (no index) from a flat xyz list, like new BufferGeometry() + position attribute
GeoPtr fromPositions(const std::vector<float>& xyz);

// worldUV() from pixelart.js: non-indexed, flat normals, UV = box projection of world position
GeoPtr worldUV(const GeoPtr& g, float density = 1);
// mergeBufferGeometries(); with useGroups each input becomes one group (materialIndex = input order)
GeoPtr merge(const std::vector<GeoPtr>& geos, bool useGroups = false);
}  // namespace geo
