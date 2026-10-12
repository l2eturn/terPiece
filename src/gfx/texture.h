#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "gfx/gl.h"

// How a texture is sampled — mirrors the three.js texture properties the web code sets.
struct TextureOptions {
    // sRGBEncoding (decoded to linear when sampled). Canvas pixel art / glTF colour maps = true,
    // CanvasTexture glow / TextureLoader jpg (LinearEncoding in r151) = false.
    bool srgb = true;
    // NearestFilter + NearestMipmapNearestFilter (pixel art) vs Linear + LinearMipmapLinear
    bool nearest = true;
    bool mipmaps = true;
    bool repeat = true;
    // three.js flipY: canvas / image row 0 ends up at v = 1. glTF textures use flipY = false.
    bool flipY = true;
    // halve larger images until both sides fit (0 = keep the full size)
    int maxSize = 0;
};

class Texture {
public:
    Texture() = default;
    ~Texture();
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    // rgba = w*h*4 bytes, rows top to bottom (like a canvas)
    void upload(int w, int h, const std::vector<uint8_t>& rgba, const TextureOptions& o = {});
    // same, from raw pixels; re-uploading the same size and options reuses the GPU storage
    void upload(int w, int h, const uint8_t* rgba, const TextureOptions& o = {});
    void bind(int unit = 0) const;
    int width() const { return w_; }
    int height() const { return h_; }
    GLuint id() const { return id_; }

    // Decode a JPEG / PNG / WebP file through ImageIO. Returns false if it cannot be read.
    bool load(const std::string& path, const TextureOptions& o = {});

private:
    GLuint id_ = 0;
    int w_ = 0, h_ = 0;
    // what the GPU storage was allocated as (to reuse it)
    GLenum format_ = 0;
    bool mipmaps_ = false;
};

// Image decoding (ImageIO on macOS) into straight-alpha RGBA8, rows top to bottom.
bool decodeImageFile(const std::string& path, int& w, int& h, std::vector<uint8_t>& rgba);
bool decodeImageMemory(const void* data, size_t size, int& w, int& h, std::vector<uint8_t>& rgba);
// Write straight-alpha RGBA8 (rows top to bottom) as a PNG file
bool savePNG(const std::string& path, int w, int h, const std::vector<uint8_t>& rgba);
