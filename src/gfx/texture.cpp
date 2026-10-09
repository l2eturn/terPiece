#include "gfx/texture.h"
#include <algorithm>

Texture::~Texture() {
    if (id_) glDeleteTextures(1, &id_);
}

void Texture::upload(int w, int h, const std::vector<uint8_t>& rgba, const TextureOptions& o) {
    if (!id_) glGenTextures(1, &id_);
    w_ = w;
    h_ = h;
    glBindTexture(GL_TEXTURE_2D, id_);
    // GL starts at the bottom row, a canvas at the top: flipping here gives three.js flipY behaviour
    const std::vector<uint8_t>* src = &rgba;
    std::vector<uint8_t> flipped;
    if (o.flipY) {
        flipped.resize(rgba.size());
        for (int y = 0; y < h; ++y)
            std::copy(rgba.begin() + (size_t)y * w * 4, rgba.begin() + (size_t)(y + 1) * w * 4,
                      flipped.begin() + (size_t)(h - 1 - y) * w * 4);
        src = &flipped;
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, o.srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, src->data());
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
