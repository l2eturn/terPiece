// Image decoding through macOS ImageIO (JPEG / PNG / WebP / GIF) — no extra library needed.
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#include "gfx/texture.h"

static bool decodeSource(CGImageSourceRef src, int& w, int& h, std::vector<uint8_t>& rgba) {
    if (!src) return false;
    CGImageRef img = CGImageSourceCreateImageAtIndex(src, 0, nullptr);
    CFRelease(src);
    if (!img) return false;
    w = (int)CGImageGetWidth(img);
    h = (int)CGImageGetHeight(img);
    rgba.assign((size_t)w * h * 4, 0);
    // Draw into an sRGB, premultiplied RGBA bitmap without colour conversion of the source bytes
    CGColorSpaceRef cs = CGImageGetColorSpace(img);
    CGColorSpaceRef own = nullptr;
    if (!cs || CGColorSpaceGetModel(cs) != kCGColorSpaceModelRGB) cs = own = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGContextRef ctx = CGBitmapContextCreate(rgba.data(), w, h, 8, (size_t)w * 4, cs, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    if (!ctx) {
        CGImageRelease(img);
        if (own) CGColorSpaceRelease(own);
        return false;
    }
    CGContextSetBlendMode(ctx, kCGBlendModeCopy);
    CGContextDrawImage(ctx, CGRectMake(0, 0, w, h), img);
    CGContextRelease(ctx);
    CGImageRelease(img);
    if (own) CGColorSpaceRelease(own);
    // un-premultiply so textures blend like the browser's straight-alpha uploads
    for (size_t i = 0; i < rgba.size(); i += 4) {
        uint8_t a = rgba[i + 3];
        if (a == 0 || a == 255) continue;
        for (int k = 0; k < 3; ++k) rgba[i + k] = (uint8_t)std::min(255, rgba[i + k] * 255 / a);
    }
    return true;
}

bool decodeImageFile(const std::string& path, int& w, int& h, std::vector<uint8_t>& rgba) {
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(nullptr, (const UInt8*)path.c_str(), (CFIndex)path.size(), false);
    if (!url) return false;
    CGImageSourceRef src = CGImageSourceCreateWithURL(url, nullptr);
    CFRelease(url);
    return decodeSource(src, w, h, rgba);
}

bool decodeImageMemory(const void* data, size_t size, int& w, int& h, std::vector<uint8_t>& rgba) {
    CFDataRef d = CFDataCreate(nullptr, (const UInt8*)data, (CFIndex)size);
    if (!d) return false;
    CGImageSourceRef src = CGImageSourceCreateWithData(d, nullptr);
    CFRelease(d);
    return decodeSource(src, w, h, rgba);
}

bool savePNG(const std::string& path, int w, int h, const std::vector<uint8_t>& rgba) {
    CGColorSpaceRef cs = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CFDataRef data = CFDataCreate(nullptr, rgba.data(), (CFIndex)rgba.size());
    CGDataProviderRef prov = CGDataProviderCreateWithCFData(data);
    CGImageRef img = CGImageCreate(w, h, 8, 32, (size_t)w * 4, cs, kCGImageAlphaLast | kCGBitmapByteOrder32Big, prov, nullptr, false,
                                   kCGRenderingIntentDefault);
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(nullptr, (const UInt8*)path.c_str(), (CFIndex)path.size(), false);
    CGImageDestinationRef dst = CGImageDestinationCreateWithURL(url, CFSTR("public.png"), 1, nullptr);
    bool ok = false;
    if (dst) {
        CGImageDestinationAddImage(dst, img, nullptr);
        ok = CGImageDestinationFinalize(dst);
        CFRelease(dst);
    }
    CFRelease(url);
    CGImageRelease(img);
    CGDataProviderRelease(prov);
    CFRelease(data);
    CGColorSpaceRelease(cs);
    return ok;
}
