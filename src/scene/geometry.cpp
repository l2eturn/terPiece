#include "scene/geometry.h"
#include <cstddef>
#include <glm/gtc/matrix_transform.hpp>

// ---------------------------------------------------------------------------
// Geometry: editing
// ---------------------------------------------------------------------------

Geometry::~Geometry() {
    if (vao_) glDeleteVertexArrays(1, &vao_);
    glDeleteBuffers(8, vbo_);
    if (ebo_) glDeleteBuffers(1, &ebo_);
}

Geometry& Geometry::applyMatrix(const glm::mat4& m) {
    glm::mat3 nm = glm::transpose(glm::inverse(glm::mat3(m)));
    for (auto& p : position) p = glm::vec3(m * glm::vec4(p, 1));
    for (auto& n : normal) {
        n = nm * n;
        float l = glm::length(n);
        if (l > 0) n /= l;
    }
    dirty_ = true;
    boundsValid_ = false;
    return *this;
}

Geometry& Geometry::translate(float x, float y, float z) { return applyMatrix(glm::translate(glm::mat4(1), {x, y, z})); }
Geometry& Geometry::rotateX(float a) { return applyMatrix(glm::rotate(glm::mat4(1), a, {1, 0, 0})); }
Geometry& Geometry::rotateY(float a) { return applyMatrix(glm::rotate(glm::mat4(1), a, {0, 1, 0})); }
Geometry& Geometry::rotateZ(float a) { return applyMatrix(glm::rotate(glm::mat4(1), a, {0, 0, 1})); }
Geometry& Geometry::scale(float x, float y, float z) { return applyMatrix(glm::scale(glm::mat4(1), {x, y, z})); }

Geometry& Geometry::center() {
    computeBounds();
    glm::vec3 c = (bbMin + bbMax) * 0.5f;
    return translate(-c.x, -c.y, -c.z);
}

void Geometry::computeVertexNormals() {
    normal.assign(position.size(), glm::vec3(0));
    auto face = [&](uint32_t a, uint32_t b, uint32_t c) {
        glm::vec3 cb = position[c] - position[b], ab = position[a] - position[b];
        return glm::cross(cb, ab);
    };
    if (indexed()) {
        for (size_t i = 0; i + 2 < index.size(); i += 3) {
            glm::vec3 n = face(index[i], index[i + 1], index[i + 2]);
            normal[index[i]] += n;
            normal[index[i + 1]] += n;
            normal[index[i + 2]] += n;
        }
    } else {
        for (size_t i = 0; i + 2 < position.size(); i += 3) {
            glm::vec3 n = face((uint32_t)i, (uint32_t)i + 1, (uint32_t)i + 2);
            normal[i] = normal[i + 1] = normal[i + 2] = n;
        }
    }
    for (auto& n : normal) {
        float l = glm::length(n);
        if (l > 0) n /= l;
    }
    dirty_ = true;
}

std::shared_ptr<Geometry> Geometry::toNonIndexed() const {
    auto g = std::make_shared<Geometry>();
    if (!indexed()) {
        g->position = position;
        g->normal = normal;
        g->color = color;
        g->uv = uv;
        g->extra = extra;
        g->groups = groups;
        return g;
    }
    for (uint32_t i : index) {
        g->position.push_back(position[i]);
        if (!normal.empty()) g->normal.push_back(normal[i]);
        if (!color.empty()) g->color.push_back(color[i]);
        if (!uv.empty()) g->uv.push_back(uv[i]);
    }
    for (auto& a : extra) {
        ExtraAttribute e{a.name, a.size, {}};
        for (uint32_t i : index)
            for (int k = 0; k < a.size; ++k) e.data.push_back(a.data[(size_t)i * a.size + k]);
        g->extra.push_back(e);
    }
    g->groups = groups;
    return g;
}

std::shared_ptr<Geometry> Geometry::clone() const {
    auto g = std::make_shared<Geometry>();
    g->position = position;
    g->normal = normal;
    g->color = color;
    g->uv = uv;
    g->index = index;
    g->groups = groups;
    g->extra = extra;
    return g;
}

ExtraAttribute* Geometry::attribute(const std::string& name) {
    for (auto& a : extra)
        if (a.name == name) return &a;
    return nullptr;
}

void Geometry::computeBounds() {
    if (position.empty()) {
        bbMin = bbMax = sphereCenter = glm::vec3(0);
        sphereRadius = 0;
        boundsValid_ = true;
        return;
    }
    bbMin = bbMax = position[0];
    for (auto& p : position) {
        bbMin = glm::min(bbMin, p);
        bbMax = glm::max(bbMax, p);
    }
    // three.js boundingSphere: box centre + max distance
    sphereCenter = (bbMin + bbMax) * 0.5f;
    float r2 = 0;
    for (auto& p : position) {
        glm::vec3 d = p - sphereCenter;
        r2 = std::max(r2, glm::dot(d, d));
    }
    sphereRadius = std::sqrt(r2);
    boundsValid_ = true;
}

// ---------------------------------------------------------------------------
// Geometry: GPU
// ---------------------------------------------------------------------------

void Geometry::upload() {
    if (!vao_) {
        glGenVertexArrays(1, &vao_);
        glGenBuffers(8, vbo_);
        glGenBuffers(1, &ebo_);
    }
    if (!boundsValid_) computeBounds();
    glBindVertexArray(vao_);
    auto attr = [&](int loc, const void* data, size_t bytes, int comps) {
        if (!bytes) {
            glDisableVertexAttribArray(loc);
            return;
        }
        glBindBuffer(GL_ARRAY_BUFFER, vbo_[loc]);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)bytes, data, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, comps, GL_FLOAT, GL_FALSE, 0, nullptr);
    };
    attr(0, position.data(), position.size() * sizeof(glm::vec3), 3);
    attr(1, normal.data(), normal.size() == position.size() ? normal.size() * sizeof(glm::vec3) : 0, 3);
    attr(2, uv.data(), uv.size() == position.size() ? uv.size() * sizeof(glm::vec2) : 0, 2);
    attr(3, color.data(), color.size() == position.size() ? color.size() * sizeof(glm::vec3) : 0, 3);
    // custom attributes live at locations 8.. (locations 4..7 are the instance matrix)
    for (size_t i = 0; i < 4; ++i) {
        GLuint loc = 8 + (GLuint)i;
        if (i < extra.size()) {
            glBindBuffer(GL_ARRAY_BUFFER, vbo_[4 + i]);
            glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(extra[i].data.size() * sizeof(float)), extra[i].data.data(), GL_DYNAMIC_DRAW);
            glEnableVertexAttribArray(loc);
            glVertexAttribPointer(loc, extra[i].size, GL_FLOAT, GL_FALSE, 0, nullptr);
        } else {
            glDisableVertexAttribArray(loc);
        }
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(index.size() * sizeof(uint32_t)), index.data(), GL_DYNAMIC_DRAW);
    dirty_ = false;
}

void Geometry::bind() {
    if (dirty_ || !vao_) upload();
    glBindVertexArray(vao_);
    // missing attributes read these constants
    if (normal.size() != position.size()) glVertexAttrib3f(1, 0, 1, 0);
    if (uv.size() != position.size()) glVertexAttrib2f(2, 0, 0);
    if (color.size() != position.size()) glVertexAttrib3f(3, 1, 1, 1);
    if (extra.empty()) glVertexAttrib1f(8, 0);
}

void Geometry::drawRange(GLenum mode, uint32_t start, uint32_t count, int instances) {
    if (!count) return;
    if (indexed()) {
        const void* off = (const void*)(uintptr_t)(start * sizeof(uint32_t));
        if (instances > 0) glDrawElementsInstanced(mode, (GLsizei)count, GL_UNSIGNED_INT, off, instances);
        else glDrawElements(mode, (GLsizei)count, GL_UNSIGNED_INT, off);
    } else {
        if (instances > 0) glDrawArraysInstanced(mode, (GLint)start, (GLsizei)count, instances);
        else glDrawArrays(mode, (GLint)start, (GLsizei)count);
    }
}

// ---------------------------------------------------------------------------
// Generators (three.js r151)
// ---------------------------------------------------------------------------

namespace geo {

namespace {

struct Builder {
    GeoPtr g = std::make_shared<Geometry>();
    void v(glm::vec3 p, glm::vec3 n, glm::vec2 t) {
        g->position.push_back(p);
        g->normal.push_back(n);
        g->uv.push_back(t);
    }
    void tri(uint32_t a, uint32_t b, uint32_t c) { g->index.insert(g->index.end(), {a, b, c}); }
    void group(uint32_t start, uint32_t count, int mat) {
        if (count) g->groups.push_back({start, count, mat});
    }
};

}  // namespace

GeoPtr box(float width, float height, float depth, int ws, int hs, int ds) {
    Builder B;
    uint32_t numberOfVertices = 0, groupStart = 0;
    // u, v, w are axis indices (0 = x, 1 = y, 2 = z)
    auto buildPlane = [&](int u, int v, int w, float udir, float vdir, float width_, float height_, float depth_, int gridX, int gridY, int materialIndex) {
        float segmentWidth = width_ / gridX, segmentHeight = height_ / gridY;
        float widthHalf = width_ / 2, heightHalf = height_ / 2, depthHalf = depth_ / 2;
        int gridX1 = gridX + 1, gridY1 = gridY + 1;
        uint32_t vertexCounter = 0, groupCount = 0;
        for (int iy = 0; iy < gridY1; ++iy) {
            float y = iy * segmentHeight - heightHalf;
            for (int ix = 0; ix < gridX1; ++ix) {
                float x = ix * segmentWidth - widthHalf;
                glm::vec3 p, n;
                p[u] = x * udir;
                p[v] = y * vdir;
                p[w] = depthHalf;
                n[u] = 0;
                n[v] = 0;
                n[w] = depth_ > 0 ? 1.0f : -1.0f;
                B.v(p, n, {(float)ix / gridX, 1 - (float)iy / gridY});
                ++vertexCounter;
            }
        }
        for (int iy = 0; iy < gridY; ++iy)
            for (int ix = 0; ix < gridX; ++ix) {
                uint32_t a = numberOfVertices + ix + gridX1 * iy;
                uint32_t b = numberOfVertices + ix + gridX1 * (iy + 1);
                uint32_t c = numberOfVertices + (ix + 1) + gridX1 * (iy + 1);
                uint32_t d = numberOfVertices + (ix + 1) + gridX1 * iy;
                B.tri(a, b, d);
                B.tri(b, c, d);
                groupCount += 6;
            }
        B.group(groupStart, groupCount, materialIndex);
        groupStart += groupCount;
        numberOfVertices += vertexCounter;
    };
    buildPlane(2, 1, 0, -1, -1, depth, height, width, ds, hs, 0);
    buildPlane(2, 1, 0, 1, -1, depth, height, -width, ds, hs, 1);
    buildPlane(0, 2, 1, 1, 1, width, depth, height, ws, ds, 2);
    buildPlane(0, 2, 1, 1, -1, width, depth, -height, ws, ds, 3);
    buildPlane(0, 1, 2, 1, -1, width, height, depth, ws, hs, 4);
    buildPlane(0, 1, 2, -1, -1, width, height, -depth, ws, hs, 5);
    return B.g;
}

GeoPtr cylinder(float radiusTop, float radiusBottom, float height, int radialSegments, int heightSegments, bool openEnded,
                float thetaStart, float thetaLength) {
    Builder B;
    uint32_t idx = 0, groupStart = 0;
    float halfHeight = height / 2;
    std::vector<std::vector<uint32_t>> indexArray;
    {
        uint32_t groupCount = 0;
        float slope = (radiusBottom - radiusTop) / height;
        for (int y = 0; y <= heightSegments; ++y) {
            std::vector<uint32_t> row;
            float v = (float)y / heightSegments;
            float radius = v * (radiusBottom - radiusTop) + radiusTop;
            for (int x = 0; x <= radialSegments; ++x) {
                float u = (float)x / radialSegments;
                float theta = u * thetaLength + thetaStart;
                float s = std::sin(theta), c = std::cos(theta);
                B.v({radius * s, -v * height + halfHeight, radius * c}, glm::normalize(glm::vec3(s, slope, c)), {u, 1 - v});
                row.push_back(idx++);
            }
            indexArray.push_back(row);
        }
        for (int x = 0; x < radialSegments; ++x)
            for (int y = 0; y < heightSegments; ++y) {
                uint32_t a = indexArray[y][x], b = indexArray[y + 1][x], c = indexArray[y + 1][x + 1], d = indexArray[y][x + 1];
                if (radiusTop > 0 || y != 0) {
                    B.tri(a, b, d);
                    groupCount += 3;
                }
                if (radiusBottom > 0 || y != heightSegments - 1) {
                    B.tri(b, c, d);
                    groupCount += 3;
                }
            }
        B.group(groupStart, groupCount, 0);
        groupStart += groupCount;
    }
    auto cap = [&](bool top) {
        uint32_t centerIndexStart = idx;
        float radius = top ? radiusTop : radiusBottom;
        float sign = top ? 1.0f : -1.0f;
        for (int x = 1; x <= radialSegments; ++x) {
            B.v({0, halfHeight * sign, 0}, {0, sign, 0}, {0.5f, 0.5f});
            ++idx;
        }
        uint32_t centerIndexEnd = idx;
        for (int x = 0; x <= radialSegments; ++x) {
            float u = (float)x / radialSegments, theta = u * thetaLength + thetaStart;
            float c = std::cos(theta), s = std::sin(theta);
            B.v({radius * s, halfHeight * sign, radius * c}, {0, sign, 0}, {c * 0.5f + 0.5f, s * 0.5f * sign + 0.5f});
            ++idx;
        }
        uint32_t groupCount = 0;
        for (int x = 0; x < radialSegments; ++x) {
            uint32_t c = centerIndexStart + x, i = centerIndexEnd + x;
            if (top) B.tri(i, i + 1, c);
            else B.tri(i + 1, i, c);
            groupCount += 3;
        }
        B.group(groupStart, groupCount, top ? 1 : 2);
        groupStart += groupCount;
    };
    if (!openEnded) {
        if (radiusTop > 0) cap(true);
        if (radiusBottom > 0) cap(false);
    }
    return B.g;
}

GeoPtr cone(float radius, float height, int radialSegments, int heightSegments, bool openEnded, float thetaStart, float thetaLength) {
    return cylinder(0, radius, height, radialSegments, heightSegments, openEnded, thetaStart, thetaLength);
}

GeoPtr sphere(float radius, int widthSegments, int heightSegments, float phiStart, float phiLength, float thetaStart, float thetaLength) {
    Builder B;
    widthSegments = std::max(3, widthSegments);
    heightSegments = std::max(2, heightSegments);
    float thetaEnd = std::min(thetaStart + thetaLength, PI);
    uint32_t idx = 0;
    std::vector<std::vector<uint32_t>> grid;
    for (int iy = 0; iy <= heightSegments; ++iy) {
        std::vector<uint32_t> row;
        float v = (float)iy / heightSegments;
        float uOffset = 0;
        if (iy == 0 && thetaStart == 0) uOffset = 0.5f / widthSegments;
        else if (iy == heightSegments && thetaEnd == PI) uOffset = -0.5f / widthSegments;
        for (int ix = 0; ix <= widthSegments; ++ix) {
            float u = (float)ix / widthSegments;
            glm::vec3 p(-radius * std::cos(phiStart + u * phiLength) * std::sin(thetaStart + v * thetaLength),
                        radius * std::cos(thetaStart + v * thetaLength),
                        radius * std::sin(phiStart + u * phiLength) * std::sin(thetaStart + v * thetaLength));
            glm::vec3 n = glm::length(p) > 0 ? glm::normalize(p) : glm::vec3(0, 1, 0);
            B.v(p, n, {u + uOffset, 1 - v});
            row.push_back(idx++);
        }
        grid.push_back(row);
    }
    for (int iy = 0; iy < heightSegments; ++iy)
        for (int ix = 0; ix < widthSegments; ++ix) {
            uint32_t a = grid[iy][ix + 1], b = grid[iy][ix], c = grid[iy + 1][ix], d = grid[iy + 1][ix + 1];
            if (iy != 0 || thetaStart > 0) B.tri(a, b, d);
            if (iy != heightSegments - 1 || thetaEnd < PI) B.tri(b, c, d);
        }
    return B.g;
}

GeoPtr plane(float width, float height, int gridX, int gridY) {
    Builder B;
    float widthHalf = width / 2, heightHalf = height / 2;
    int gridX1 = gridX + 1, gridY1 = gridY + 1;
    float segW = width / gridX, segH = height / gridY;
    for (int iy = 0; iy < gridY1; ++iy) {
        float y = iy * segH - heightHalf;
        for (int ix = 0; ix < gridX1; ++ix) {
            float x = ix * segW - widthHalf;
            B.v({x, -y, 0}, {0, 0, 1}, {(float)ix / gridX, 1 - (float)iy / gridY});
        }
    }
    for (int iy = 0; iy < gridY; ++iy)
        for (int ix = 0; ix < gridX; ++ix) {
            uint32_t a = ix + gridX1 * iy, b = ix + gridX1 * (iy + 1), c = (ix + 1) + gridX1 * (iy + 1), d = (ix + 1) + gridX1 * iy;
            B.tri(a, b, d);
            B.tri(b, c, d);
        }
    return B.g;
}

GeoPtr circle(float radius, int segments, float thetaStart, float thetaLength) {
    Builder B;
    segments = std::max(3, segments);
    B.v({0, 0, 0}, {0, 0, 1}, {0.5f, 0.5f});
    for (int s = 0; s <= segments; ++s) {
        float seg = thetaStart + (float)s / segments * thetaLength;
        float x = radius * std::cos(seg), y = radius * std::sin(seg);
        B.v({x, y, 0}, {0, 0, 1}, {(x / radius + 1) / 2, (y / radius + 1) / 2});
    }
    for (int i = 1; i <= segments; ++i) B.tri(i, i + 1, 0);
    return B.g;
}

GeoPtr torus(float radius, float tube, int radialSegments, int tubularSegments, float arc) {
    Builder B;
    for (int j = 0; j <= radialSegments; ++j)
        for (int i = 0; i <= tubularSegments; ++i) {
            float u = (float)i / tubularSegments * arc, v = (float)j / radialSegments * PI * 2;
            glm::vec3 p((radius + tube * std::cos(v)) * std::cos(u), (radius + tube * std::cos(v)) * std::sin(u), tube * std::sin(v));
            glm::vec3 c(radius * std::cos(u), radius * std::sin(u), 0);
            B.v(p, glm::normalize(p - c), {(float)i / tubularSegments, (float)j / radialSegments});
        }
    for (int j = 1; j <= radialSegments; ++j)
        for (int i = 1; i <= tubularSegments; ++i) {
            uint32_t a = (tubularSegments + 1) * j + i - 1, b = (tubularSegments + 1) * (j - 1) + i - 1;
            uint32_t c = (tubularSegments + 1) * (j - 1) + i, d = (tubularSegments + 1) * j + i;
            B.tri(a, b, d);
            B.tri(b, c, d);
        }
    return B.g;
}

// PolyhedronGeometry: faces subdivided `detail` times, pushed onto the sphere, spherical UVs.
// detail 0 -> flat normals, otherwise smooth (normalised positions), like three.js.
static GeoPtr polyhedron(const std::vector<float>& verts, const std::vector<int>& faces, float radius, int detail = 0) {
    auto g = std::make_shared<Geometry>();
    auto P = [&](int k) { return glm::vec3(verts[k * 3], verts[k * 3 + 1], verts[k * 3 + 2]); };
    for (size_t f = 0; f + 2 < faces.size(); f += 3) {
        glm::vec3 a = P(faces[f]), b = P(faces[f + 1]), c = P(faces[f + 2]);
        int cols = detail + 1;
        std::vector<std::vector<glm::vec3>> v(cols + 1);
        for (int i = 0; i <= cols; ++i) {
            glm::vec3 aj = glm::mix(a, c, (float)i / cols), bj = glm::mix(b, c, (float)i / cols);
            int rows = cols - i;
            for (int j = 0; j <= rows; ++j) v[i].push_back(j == 0 && i == cols ? aj : glm::mix(aj, bj, rows ? (float)j / rows : 0.0f));
        }
        for (int i = 0; i < cols; ++i)
            for (int j = 0; j < 2 * (cols - i) - 1; ++j) {
                int k = j / 2;
                if (j % 2 == 0) g->position.insert(g->position.end(), {v[i][k + 1], v[i + 1][k], v[i][k]});
                else g->position.insert(g->position.end(), {v[i][k + 1], v[i + 1][k + 1], v[i + 1][k]});
            }
    }
    for (auto& p : g->position) p = glm::normalize(p) * radius;
    for (auto& p : g->position) {
        float u = std::atan2(p.z, -p.x) / 2 / PI + 0.5f;
        float vv = std::atan2(-p.y, std::sqrt(p.x * p.x + p.z * p.z)) / PI + 0.5f;
        g->uv.push_back({u, vv});
    }
    if (detail == 0) {
        g->computeVertexNormals();
    } else {
        for (auto& p : g->position) g->normal.push_back(glm::normalize(p));
    }
    return g;
}

GeoPtr dodecahedron(float radius, int detail) {
    const float t = (1 + std::sqrt(5.0f)) / 2, r = 1 / t;
    std::vector<float> v = {-1, -1, -1, -1, -1, 1, -1, 1, -1, -1, 1, 1, 1, -1, -1, 1, -1, 1, 1, 1, -1, 1, 1, 1,
                            0, -r, -t, 0, -r, t, 0, r, -t, 0, r, t, -r, -t, 0, -r, t, 0, r, -t, 0, r, t, 0,
                            -t, 0, -r, t, 0, -r, -t, 0, r, t, 0, r};
    std::vector<int> i = {3, 11, 7, 3, 7, 15, 3, 15, 13, 7, 19, 17, 7, 17, 6, 7, 6, 15, 17, 4, 8, 17, 8, 10, 17, 10, 6,
                          8, 0, 16, 8, 16, 2, 8, 2, 10, 0, 12, 1, 0, 1, 18, 0, 18, 16, 6, 10, 2, 6, 2, 13, 6, 13, 15,
                          2, 16, 18, 2, 18, 3, 2, 3, 13, 18, 1, 9, 18, 9, 11, 18, 11, 3, 4, 14, 12, 4, 12, 0, 4, 0, 8,
                          11, 9, 5, 11, 5, 19, 11, 19, 7, 19, 5, 14, 19, 14, 4, 19, 4, 17, 1, 12, 14, 1, 14, 5, 1, 5, 9};
    return polyhedron(v, i, radius, detail);
}

GeoPtr octahedron(float radius) {
    std::vector<float> v = {1, 0, 0, -1, 0, 0, 0, 1, 0, 0, -1, 0, 0, 0, 1, 0, 0, -1};
    std::vector<int> i = {0, 2, 4, 0, 4, 3, 0, 3, 5, 0, 5, 2, 1, 2, 5, 1, 5, 3, 1, 3, 4, 1, 4, 2};
    return polyhedron(v, i, radius);
}

GeoPtr icosahedron(float radius) {
    const float t = (1 + std::sqrt(5.0f)) / 2;
    std::vector<float> v = {-1, t, 0, 1, t, 0, -1, -t, 0, 1, -t, 0, 0, -1, t, 0, 1, t, 0, -1, -t, 0, 1, -t, t, 0, -1, t, 0, 1, -t, 0, -1, -t, 0, 1};
    std::vector<int> i = {0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2, 10, 7, 6, 7, 1, 8,
                          3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6, 7, 9, 8, 1};
    return polyhedron(v, i, radius);
}

// ---------- shapes ----------

namespace {

float signedArea(const std::vector<glm::vec2>& p) {
    float a = 0;
    for (size_t i = 0, n = p.size(); i < n; ++i) {
        const glm::vec2 &u = p[i], &v = p[(i + 1) % n];
        a += u.x * v.y - v.x * u.y;
    }
    return a * 0.5f;
}

// Ear clipping for a simple polygon; returns CCW triangles (indices into p)
std::vector<uint32_t> triangulate(std::vector<glm::vec2> p) {
    std::vector<uint32_t> out;
    if (p.size() >= 2 && glm::length(p.front() - p.back()) < 1e-6f) p.pop_back();
    size_t n = p.size();
    if (n < 3) return out;
    std::vector<uint32_t> ring(n);
    for (size_t i = 0; i < n; ++i) ring[i] = (uint32_t)i;
    if (signedArea(p) < 0) std::reverse(ring.begin(), ring.end());
    auto cross = [](glm::vec2 a, glm::vec2 b, glm::vec2 c) { return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x); };
    auto inside = [&](glm::vec2 q, glm::vec2 a, glm::vec2 b, glm::vec2 c) {
        return cross(a, b, q) >= 0 && cross(b, c, q) >= 0 && cross(c, a, q) >= 0;
    };
    int guard = 0;
    while (ring.size() > 3 && guard++ < 10000) {
        bool clipped = false;
        for (size_t i = 0; i < ring.size(); ++i) {
            uint32_t ia = ring[(i + ring.size() - 1) % ring.size()], ib = ring[i], ic = ring[(i + 1) % ring.size()];
            glm::vec2 a = p[ia], b = p[ib], c = p[ic];
            if (cross(a, b, c) <= 1e-9f) continue;
            bool ear = true;
            for (uint32_t k : ring) {
                if (k == ia || k == ib || k == ic) continue;
                if (inside(p[k], a, b, c)) {
                    ear = false;
                    break;
                }
            }
            if (!ear) continue;
            out.insert(out.end(), {ia, ib, ic});
            ring.erase(ring.begin() + (long)i);
            clipped = true;
            break;
        }
        if (!clipped) break;
    }
    if (ring.size() == 3) out.insert(out.end(), {ring[0], ring[1], ring[2]});
    return out;
}

}  // namespace

GeoPtr shape(const Shape& s) {
    Builder B;
    for (auto& p : s.pts) B.v({p.x, p.y, 0}, {0, 0, 1}, {p.x, p.y});
    for (uint32_t i : triangulate(s.pts)) B.g->index.push_back(i);
    return B.g;
}

GeoPtr extrude(const Shape& s, const ExtrudeOptions& o) {
    std::vector<glm::vec2> pts = s.pts;
    if (pts.size() >= 2 && glm::length(pts.front() - pts.back()) < 1e-6f) pts.pop_back();
    // three.js works on a clockwise contour
    if (signedArea(pts) > 0) std::reverse(pts.begin(), pts.end());
    size_t n = pts.size();
    // outward offset direction per vertex (miter of the two edge normals)
    std::vector<glm::vec2> bevelVec(n);
    for (size_t i = 0; i < n; ++i) {
        glm::vec2 prev = pts[(i + n - 1) % n], cur = pts[i], next = pts[(i + 1) % n];
        glm::vec2 e0 = glm::normalize(cur - prev), e1 = glm::normalize(next - cur);
        // clockwise contour: outward normal of edge e is (-e.y, e.x)
        glm::vec2 n0(-e0.y, e0.x), n1(-e1.y, e1.x);
        glm::vec2 m = n0 + n1;
        float ml = glm::length(m);
        m = ml > 1e-6f ? m / ml : n0;
        float d = glm::dot(m, n0);
        bevelVec[i] = m / std::max(d, 0.2f);
    }
    // layers of the side wall: (z, outward offset)
    std::vector<std::pair<float, float>> layers;
    if (o.bevelEnabled) {
        float bt = o.bevelThickness, bs = o.bevelSize;
        int segs = std::max(1, o.bevelSegments);
        for (int b = 0; b <= segs; ++b) {
            float t = (float)b / segs;
            layers.push_back({-bt * std::cos(t * PI / 2), bs * std::sin(t * PI / 2)});
        }
        layers.push_back({o.depth, bs});
        for (int b = segs - 1; b >= 0; --b) {
            float t = (float)b / segs;
            layers.push_back({o.depth + bt * std::cos(t * PI / 2), bs * std::sin(t * PI / 2)});
        }
    } else {
        layers = {{0, 0}, {o.depth, 0}};
    }
    auto P = [&](size_t layer, size_t i) {
        glm::vec2 q = pts[i] + bevelVec[i] * layers[layer].second;
        return glm::vec3(q, layers[layer].first);
    };
    auto g = std::make_shared<Geometry>();
    auto push = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c) {
        g->position.insert(g->position.end(), {a, b, c});
        for (auto& p : {a, b, c}) g->uv.push_back({p.x, p.y});
    };
    std::vector<uint32_t> tris = triangulate(pts);
    // caps: bottom faces -z, top faces +z (triangulate() returns CCW in XY)
    for (size_t k = 0; k + 2 < tris.size(); k += 3) push(P(0, tris[k + 2]), P(0, tris[k + 1]), P(0, tris[k]));
    size_t last = layers.size() - 1;
    for (size_t k = 0; k + 2 < tris.size(); k += 3) push(P(last, tris[k]), P(last, tris[k + 1]), P(last, tris[k + 2]));
    uint32_t capCount = (uint32_t)g->position.size();
    // side walls
    for (size_t l = 0; l < last; ++l)
        for (size_t i = 0; i < n; ++i) {
            size_t j = (i + 1) % n;
            glm::vec3 a = P(l, i), b = P(l, j), c = P(l + 1, j), d = P(l + 1, i);
            push(a, d, b);
            push(b, d, c);
        }
    g->groups.push_back({0, capCount, 0});
    g->groups.push_back({capCount, (uint32_t)g->position.size() - capCount, 1});
    g->computeVertexNormals();
    return g;
}

GeoPtr fromPositions(const std::vector<float>& xyz) {
    auto g = std::make_shared<Geometry>();
    for (size_t i = 0; i + 2 < xyz.size(); i += 3) g->position.push_back({xyz[i], xyz[i + 1], xyz[i + 2]});
    return g;
}

GeoPtr worldUV(const GeoPtr& src, float density) {
    GeoPtr g = src->indexed() ? src->toNonIndexed() : src;
    g->computeVertexNormals();
    g->uv.resize(g->position.size());
    for (size_t i = 0; i < g->position.size(); ++i) {
        glm::vec3 p = g->position[i], a = glm::abs(g->normal[i]);
        glm::vec2 t;
        if (a.y >= a.x && a.y >= a.z) t = {p.x, p.z};
        else if (a.x >= a.z) t = {p.z, p.y};
        else t = {p.x, p.y};
        g->uv[i] = t * density;
    }
    g->markDirty();
    return g;
}

GeoPtr merge(const std::vector<GeoPtr>& geos, bool useGroups) {
    auto g = std::make_shared<Geometry>();
    bool indexed = !geos.empty() && geos[0]->indexed();
    uint32_t offset = 0;
    for (size_t k = 0; k < geos.size(); ++k) {
        const Geometry& s = *geos[k];
        uint32_t start = g->drawCount();
        for (uint32_t i : s.index) g->index.push_back(i + offset);
        g->position.insert(g->position.end(), s.position.begin(), s.position.end());
        auto fill = [&](auto& dst, const auto& src, auto def) {
            if (src.size() == s.position.size()) dst.insert(dst.end(), src.begin(), src.end());
            else dst.insert(dst.end(), s.position.size(), def);
        };
        fill(g->normal, s.normal, glm::vec3(0, 1, 0));
        fill(g->uv, s.uv, glm::vec2(0));
        if (!s.color.empty() || !g->color.empty()) {
            if (g->color.size() < offset) g->color.resize(offset, glm::vec3(1));
            fill(g->color, s.color, glm::vec3(1));
        }
        offset += (uint32_t)s.position.size();
        if (useGroups) g->groups.push_back({start, (indexed ? (uint32_t)s.index.size() : (uint32_t)s.position.size()), (int)k});
    }
    return g;
}

}  // namespace geo
