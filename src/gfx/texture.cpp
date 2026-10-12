#include "gfx/texture.h"
#include <algorithm>

Texture::~Texture() {
    if (id_) glDeleteTextures(1, &id_);
}

namespace {
// one scratch buffer for flipping / shrinking, reused by every upload (no per-frame allocations)
std::vector<uint8_t>& scratch() {
    static std::vector<uint8_t> s;
    return s;
}

// 2x2 box filter
std::vector<uint8_t> halve(const uint8_t* src, int w, int h, int& nw, int& nh) {
    nw = std::max(1, w / 2);
    nh = std::max(1, h / 2);
    std::vector<uint8_t> out((size_t)nw * nh * 4);
    for (int y = 0; y < nh; ++y)
        for (int x = 0; x < nw; ++x)
            for (int c = 0; c < 4; ++c) {
                int sum = 0;
                for (int dy = 0; dy < 2; ++dy)
                    for (int dx = 0; dx < 2; ++dx) sum += src[((size_t)std::min(h - 1, y * 2 + dy) * w + std::min(w - 1, x * 2 + dx)) * 4 + c];
                out[((size_t)y * nw + x) * 4 + c] = (uint8_t)(sum / 4);
            }
    return out;
}
}  // namespace

void Texture::upload(int w, int h, const std::vector<uint8_t>& rgba, const TextureOptions& o) { upload(w, h, rgba.data(), o); }

void Texture::upload(int w, int h, const uint8_t* rgba, const TextureOptions& o) {
    // oversized images are shrunk first
    std::vector<uint8_t> small;
    while (o.maxSize > 0 && (w > o.maxSize || h > o.maxSize)) {
        int nw, nh;
        small = halve(rgba, w, h, nw, nh);
        rgba = small.data();
        w = nw;
        h = nh;
    }
    // GL starts at the bottom row, a canvas at the top: flipping here gives three.js flipY behaviour
    const uint8_t* src = rgba;
    if (o.flipY) {
        auto& f = scratch();
        f.resize((size_t)w * h * 4);
        size_t row = (size_t)w * 4;
        for (int y = 0; y < h; ++y) std::copy(rgba + y * row, rgba + (y + 1) * row, f.begin() + (size_t)(h - 1 - y) * row);
        src = f.data();
    }
    GLenum format = o.srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (id_ && w == w_ && h == h_ && format == format_ && o.mipmaps == mipmaps_) {
        // same size: overwrite in place instead of allocating new storage
        glBindTexture(GL_TEXTURE_2D, id_);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, src);
    } else {
        if (!id_) glGenTextures(1, &id_);
        glBindTexture(GL_TEXTURE_2D, id_);
        glTexImage2D(GL_TEXTURE_2D, 0, (GLint)format, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, src);
        w_ = w;
        h_ = h;
        format_ = format;
        mipmaps_ = o.mipmaps;
    }
    if (o.mipmaps) glGenerateMipmap(GL_TEXTURE_2D);
    GLint mag = o.nearest ? GL_NEAREST : GL_LINEAR;
    GLint min = o.mipmaps ? (o.nearest ? GL_NEAREST_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR) : mag;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min);
    GLint wrap = o.repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
}

bool Texture::load(const std::string& path, const TextureOptions& o) {
    int w, h;
    std::vector<uint8_t> px;
    if (!decodeImageFile(path, w, h, px)) return false;
    upload(w, h, px, o);
    return true;
}

void Texture::bind(int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, id_);
}
