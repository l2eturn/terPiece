// Canvas2D on CoreGraphics / CoreText.
#include "ui/canvas2d.h"
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#include <cmath>
#include <cstdlib>
#include <map>
#include "gfx/texture.h"

namespace {

CFStringRef cfstr(const std::string& s) { return CFStringCreateWithBytes(nullptr, (const UInt8*)s.data(), (CFIndex)s.size(), kCFStringEncodingUTF8, false); }

// CSS font-weight -> CoreText weight trait
float ctWeight(int w) {
    if (w <= 100) return -0.8f;
    if (w <= 200) return -0.6f;
    if (w <= 300) return -0.4f;
    if (w <= 400) return 0.0f;
    if (w <= 500) return 0.23f;
    if (w <= 600) return 0.3f;
    if (w <= 700) return 0.4f;
    if (w <= 800) return 0.56f;
    return 0.62f;
}

// Mali (Thai) as the fallback of every font, like the CSS stacks "var(--serif), var(--thai)"
CTFontRef makeFont(const std::string& family, float size, int weight, bool italic) {
    static std::map<std::string, CTFontRef> cache;
    std::string key = family + "|" + std::to_string(size) + "|" + std::to_string(weight) + (italic ? "i" : "");
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    CTFontRef font = nullptr;
    if (family == "Mali") {
        // static files: pick the closest weight by PostScript name
        const char* ps = weight >= 600 ? "Mali-SemiBold" : weight >= 500 ? "Mali-Medium" : "Mali-Regular";
        CFStringRef n = cfstr(ps);
        font = CTFontCreateWithName(n, size, nullptr);
        CFRelease(n);
    } else {
        CFStringRef fam = cfstr(family);
        CFMutableDictionaryRef attrs = CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(attrs, kCTFontFamilyNameAttribute, fam);
        // traits: weight (+ italic)
        CFMutableDictionaryRef traits = CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        float wt = ctWeight(weight);
        CFNumberRef wn = CFNumberCreate(nullptr, kCFNumberFloatType, &wt);
        CFDictionarySetValue(traits, kCTFontWeightTrait, wn);
        if (italic) {
            uint32_t sym = kCTFontItalicTrait;
            CFNumberRef sn = CFNumberCreate(nullptr, kCFNumberSInt32Type, &sym);
            CFDictionarySetValue(traits, kCTFontSymbolicTrait, sn);
            CFRelease(sn);
        }
        CFDictionarySetValue(attrs, kCTFontTraitsAttribute, traits);
        // variable fonts (Cormorant Garamond, Inter): set the wght axis directly
        CFMutableDictionaryRef variation = CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        int wghtTag = 'wght';
        float wv = (float)weight;
        CFNumberRef tagN = CFNumberCreate(nullptr, kCFNumberIntType, &wghtTag);
        CFNumberRef valN = CFNumberCreate(nullptr, kCFNumberFloatType, &wv);
        CFDictionarySetValue(variation, tagN, valN);
        CFDictionarySetValue(attrs, kCTFontVariationAttribute, variation);
        CTFontDescriptorRef desc = CTFontDescriptorCreateWithAttributes(attrs);
        font = CTFontCreateWithFontDescriptor(desc, size, nullptr);
        CFRelease(desc);
        CFRelease(tagN);
        CFRelease(valN);
        CFRelease(variation);
        CFRelease(wn);
        CFRelease(traits);
        CFRelease(attrs);
        CFRelease(fam);
    }
    // Thai fallback
    if (family != "Mali") {
        CFStringRef maliName = cfstr(weight >= 600 ? "Mali-SemiBold" : weight >= 500 ? "Mali-Medium" : "Mali-Regular");
        CTFontDescriptorRef mali = CTFontDescriptorCreateWithNameAndSize(maliName, size);
        CFArrayRef cascade = CFArrayCreate(nullptr, (const void**)&mali, 1, &kCFTypeArrayCallBacks);
        CFDictionaryRef ca = CFDictionaryCreate(nullptr, (const void**)&kCTFontCascadeListAttribute, (const void**)&cascade, 1,
                                                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CTFontDescriptorRef cd = CTFontDescriptorCreateWithAttributes(ca);
        CTFontRef withCascade = CTFontCreateCopyWithAttributes(font, size, nullptr, cd);
        CFRelease(cd);
        CFRelease(ca);
        CFRelease(cascade);
        CFRelease(mali);
        CFRelease(maliName);
        CFRelease(font);
        font = withCascade;
    }
    cache[key] = font;
    return font;
}

CGColorRef cgColor(const RGBA& c) {
    static CGColorSpaceRef cs = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGFloat comps[4] = {c.r, c.g, c.b, c.a};
    return CGColorCreate(cs, comps);
}

}  // namespace

RGBA cssColor(const std::string& s) {
    RGBA c;
    if (s.empty() || s == "transparent") return {0, 0, 0, 0};
    if (s == "white") return {1, 1, 1, 1};
    if (s == "black") return {0, 0, 0, 1};
    if (s[0] == '#') {
        std::string h = s.substr(1);
        if (h.size() == 3 || h.size() == 4) {
            std::string e;
            for (char ch : h) e += std::string(2, ch);
            h = e;
        }
        unsigned long v = std::strtoul(h.c_str(), nullptr, 16);
        if (h.size() == 8) {
            c.r = (v >> 24 & 255) / 255.f;
            c.g = (v >> 16 & 255) / 255.f;
            c.b = (v >> 8 & 255) / 255.f;
            c.a = (v & 255) / 255.f;
        } else {
            c.r = (v >> 16 & 255) / 255.f;
            c.g = (v >> 8 & 255) / 255.f;
            c.b = (v & 255) / 255.f;
        }
        return c;
    }
    size_t p = s.find('(');
    if (p != std::string::npos) {
        float v[4] = {0, 0, 0, 1};
        const char* q = s.c_str() + p + 1;
        for (int i = 0; i < 4 && *q; ++i) {
            char* end;
            v[i] = std::strtof(q, &end);
            if (end == q) break;
            q = end;
            while (*q == ',' || *q == ' ' || *q == '/') ++q;
        }
        return {v[0] / 255, v[1] / 255, v[2] / 255, v[3]};
    }
    return c;
}

struct Canvas2D::Impl {
    std::vector<uint8_t> px;
    CGContextRef ctx = nullptr;
    struct FontState {
        std::string family = "Inter";
        float size = 10;
        int weight = 400;
        bool italic = false;
        RGBA fill{0, 0, 0, 1}, stroke{0, 0, 0, 1};
        float lineWidth = 1;
    };
    FontState st;
    std::vector<FontState> stack;
};

Canvas2D::Canvas2D(int w, int h) : d(std::make_unique<Impl>()), w_(0), h_(0) { resize(w, h); }

Canvas2D::~Canvas2D() {
    if (d->ctx) CGContextRelease(d->ctx);
}

void Canvas2D::resize(int w, int h) {
    w = std::max(1, w);
    h = std::max(1, h);
    if (w == w_ && h == h_ && d->ctx) return;
    if (d->ctx) CGContextRelease(d->ctx);
    w_ = w;
    h_ = h;
    d->px.assign((size_t)w * h * 4, 0);
    CGColorSpaceRef cs = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    d->ctx = CGBitmapContextCreate(d->px.data(), w, h, 8, (size_t)w * 4, cs, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(cs);
    // y-down like an HTML canvas
    CGContextTranslateCTM(d->ctx, 0, h);
    CGContextScaleCTM(d->ctx, 1, -1);
    CGContextSetTextMatrix(d->ctx, CGAffineTransformMakeScale(1, -1));
    CGContextSetShouldAntialias(d->ctx, true);
    CGContextSetShouldSmoothFonts(d->ctx, false);
    d->stack.clear();
    d->st = {};
    setFill(d->st.fill);
    setStroke(d->st.stroke);
}

void Canvas2D::save() {
    CGContextSaveGState(d->ctx);
    d->stack.push_back(d->st);
}

void Canvas2D::restore() {
    if (d->stack.empty()) return;
    CGContextRestoreGState(d->ctx);
    d->st = d->stack.back();
    d->stack.pop_back();
}

void Canvas2D::translate(float x, float y) { CGContextTranslateCTM(d->ctx, x, y); }
void Canvas2D::rotate(float a) { CGContextRotateCTM(d->ctx, a); }
void Canvas2D::scale(float x, float y) { CGContextScaleCTM(d->ctx, x, y); }

void Canvas2D::setFill(const RGBA& c) {
    d->st.fill = c;
    CGContextSetRGBFillColor(d->ctx, c.r, c.g, c.b, c.a);
}

void Canvas2D::setStroke(const RGBA& c) {
    d->st.stroke = c;
    CGContextSetRGBStrokeColor(d->ctx, c.r, c.g, c.b, c.a);
}

void Canvas2D::setLineWidth(float w) {
    d->st.lineWidth = w;
    CGContextSetLineWidth(d->ctx, w);
}

void Canvas2D::setLineCap(bool round) {
    CGContextSetLineCap(d->ctx, round ? kCGLineCapRound : kCGLineCapButt);
    CGContextSetLineJoin(d->ctx, round ? kCGLineJoinRound : kCGLineJoinMiter);
}

void Canvas2D::setGlobalAlpha(float a) { CGContextSetAlpha(d->ctx, a); }

void Canvas2D::setFont(const std::string& family, float size, int weight, bool italic) {
    d->st.family = family;
    d->st.size = size;
    d->st.weight = weight;
    d->st.italic = italic;
}

void Canvas2D::setShadow(float blur, const RGBA& color, float dx, float dy) {
    CGColorRef c = cgColor(color);
    // CTM is flipped, CG shadow offsets are not: negate y
    CGContextSetShadowWithColor(d->ctx, CGSizeMake(dx, -dy), blur, c);
    CGColorRelease(c);
}

void Canvas2D::clearShadow() { CGContextSetShadowWithColor(d->ctx, CGSizeZero, 0, nullptr); }

void Canvas2D::setAdditive(bool on) { CGContextSetBlendMode(d->ctx, on ? kCGBlendModePlusLighter : kCGBlendModeNormal); }

void Canvas2D::clear() { std::fill(d->px.begin(), d->px.end(), 0); }

void Canvas2D::clearRect(float x, float y, float w, float h) { CGContextClearRect(d->ctx, CGRectMake(x, y, w, h)); }
void Canvas2D::fillRect(float x, float y, float w, float h) { CGContextFillRect(d->ctx, CGRectMake(x, y, w, h)); }
void Canvas2D::strokeRect(float x, float y, float w, float h) { CGContextStrokeRect(d->ctx, CGRectMake(x, y, w, h)); }
void Canvas2D::beginPath() { CGContextBeginPath(d->ctx); }
void Canvas2D::closePath() { CGContextClosePath(d->ctx); }
void Canvas2D::moveTo(float x, float y) { CGContextMoveToPoint(d->ctx, x, y); }
void Canvas2D::lineTo(float x, float y) {
    if (CGContextIsPathEmpty(d->ctx)) CGContextMoveToPoint(d->ctx, x, y);
    else CGContextAddLineToPoint(d->ctx, x, y);
}
void Canvas2D::quadTo(float cx, float cy, float x, float y) { CGContextAddQuadCurveToPoint(d->ctx, cx, cy, x, y); }

void Canvas2D::arc(float x, float y, float r, float a0, float a1, bool ccw) {
    // canvas angles are clockwise on screen (y-down); the flipped CTM keeps that relationship with clockwise = ccw
    CGContextAddArc(d->ctx, x, y, r, a0, a1, ccw ? 1 : 0);
}

void Canvas2D::ellipse(float x, float y, float rx, float ry, float rot) {
    CGContextSaveGState(d->ctx);
    CGAffineTransform t = CGAffineTransformMakeTranslation(x, y);
    t = CGAffineTransformRotate(t, rot);
    CGMutablePathRef p = CGPathCreateMutable();
    CGPathAddEllipseInRect(p, &t, CGRectMake(-rx, -ry, rx * 2, ry * 2));
    CGContextRestoreGState(d->ctx);
    CGContextAddPath(d->ctx, p);
    CGPathRelease(p);
}

void Canvas2D::rect(float x, float y, float w, float h) { CGContextAddRect(d->ctx, CGRectMake(x, y, w, h)); }

void Canvas2D::roundRect(float x, float y, float w, float h, float r) {
    r = std::max(0.0f, std::min(r, std::min(w, h) / 2));
    CGMutablePathRef p = CGPathCreateMutable();
    CGPathAddRoundedRect(p, nullptr, CGRectMake(x, y, w, h), r, r);
    CGContextAddPath(d->ctx, p);
    CGPathRelease(p);
}

void Canvas2D::fill() { CGContextFillPath(d->ctx); }
void Canvas2D::stroke() { CGContextStrokePath(d->ctx); }
void Canvas2D::clip() { CGContextClip(d->ctx); }

static CGGradientRef makeGradient(const std::vector<GradientStop>& stops) {
    CGColorSpaceRef cs = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    std::vector<CGFloat> comps, locs;
    for (auto& s : stops) {
        comps.insert(comps.end(), {s.color.r, s.color.g, s.color.b, s.color.a});
        locs.push_back(s.offset);
    }
    CGGradientRef g = CGGradientCreateWithColorComponents(cs, comps.data(), locs.data(), locs.size());
    CGColorSpaceRelease(cs);
    return g;
}

void Canvas2D::fillLinearGradient(float x0, float y0, float x1, float y1, const std::vector<GradientStop>& stops) {
    CGContextSaveGState(d->ctx);
    CGContextClip(d->ctx);
    CGGradientRef g = makeGradient(stops);
    CGContextDrawLinearGradient(d->ctx, g, CGPointMake(x0, y0), CGPointMake(x1, y1), kCGGradientDrawsBeforeStartLocation | kCGGradientDrawsAfterEndLocation);
    CGGradientRelease(g);
    CGContextRestoreGState(d->ctx);
}

void Canvas2D::fillRadialGradient(float x0, float y0, float r0, float x1, float y1, float r1, const std::vector<GradientStop>& stops) {
    CGContextSaveGState(d->ctx);
    CGContextClip(d->ctx);
    CGGradientRef g = makeGradient(stops);
    CGContextDrawRadialGradient(d->ctx, g, CGPointMake(x0, y0), r0, CGPointMake(x1, y1), r1, kCGGradientDrawsAfterEndLocation);
    CGGradientRelease(g);
    CGContextRestoreGState(d->ctx);
}

static CTLineRef makeLine(const std::string& utf8, CTFontRef font, float tracking) {
    CFStringRef str = cfstr(utf8);
    if (!str) str = CFSTR("");
    CFMutableDictionaryRef attrs = CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(attrs, kCTFontAttributeName, font);
    CFDictionarySetValue(attrs, kCTForegroundColorFromContextAttributeName, kCFBooleanTrue);
    if (tracking != 0) {
        float k = tracking;
        CFNumberRef kn = CFNumberCreate(nullptr, kCFNumberFloatType, &k);
        CFDictionarySetValue(attrs, kCTKernAttributeName, kn);
        CFRelease(kn);
    }
    CFAttributedStringRef as = CFAttributedStringCreate(nullptr, str, attrs);
    CTLineRef line = CTLineCreateWithAttributedString(as);
    CFRelease(as);
    CFRelease(attrs);
    CFRelease(str);
    return line;
}

float Canvas2D::measureText(const std::string& utf8) {
    CTFontRef f = makeFont(d->st.family, d->st.size, d->st.weight, d->st.italic);
    CTLineRef line = makeLine(utf8, f, tracking_ * d->st.size);
    double w = CTLineGetTypographicBounds(line, nullptr, nullptr, nullptr);
    CFRelease(line);
    return (float)w;
}

void Canvas2D::textOp(const std::string& utf8, float x, float y, bool fillMode) {
    CTFontRef f = makeFont(d->st.family, d->st.size, d->st.weight, d->st.italic);
    CTLineRef line = makeLine(utf8, f, tracking_ * d->st.size);
    CGFloat ascent = 0, descent = 0, leading = 0;
    double w = CTLineGetTypographicBounds(line, &ascent, &descent, &leading);
    float fa = (float)CTFontGetAscent(f), fd = (float)CTFontGetDescent(f);
    if (align_ == TextAlign::Center) x -= (float)w / 2;
    else if (align_ == TextAlign::Right) x -= (float)w;
    if (baseline_ == TextBaseline::Top) y += fa;
    else if (baseline_ == TextBaseline::Middle) y += (fa - fd) / 2;
    else if (baseline_ == TextBaseline::Bottom) y -= fd;
    CGContextSetTextDrawingMode(d->ctx, fillMode ? kCGTextFill : kCGTextStroke);
    if (!fillMode) {
        // CTLine draws with the stroke colour when the foreground comes from the context
        CGContextSetRGBFillColor(d->ctx, d->st.stroke.r, d->st.stroke.g, d->st.stroke.b, d->st.stroke.a);
        CGContextSetLineJoin(d->ctx, kCGLineJoinRound);
    }
    CGContextSetTextPosition(d->ctx, x, y);
    CTLineDraw(line, d->ctx);
    if (!fillMode) CGContextSetRGBFillColor(d->ctx, d->st.fill.r, d->st.fill.g, d->st.fill.b, d->st.fill.a);
    CGContextSetTextDrawingMode(d->ctx, kCGTextFill);
    CFRelease(line);
}

void Canvas2D::fillText(const std::string& utf8, float x, float y) { textOp(utf8, x, y, true); }

std::vector<std::string> Canvas2D::wrapText(const std::string& utf8, float maxWidth) {
    std::vector<std::string> lines;
    CTFontRef f = makeFont(d->st.family, d->st.size, d->st.weight, d->st.italic);
    CFStringRef str = cfstr(utf8);
    if (!str) return {utf8};
    CFDictionaryRef attrs = CFDictionaryCreate(nullptr, (const void**)&kCTFontAttributeName, (const void**)&f, 1,
                                               &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFAttributedStringRef as = CFAttributedStringCreate(nullptr, str, attrs);
    CTTypesetterRef ts = CTTypesetterCreateWithAttributedString(as);
    CFIndex len = CFStringGetLength(str), start = 0;
    while (start < len) {
        CFIndex n = CTTypesetterSuggestLineBreak(ts, start, maxWidth);
        if (n <= 0) n = len - start;
        CFStringRef sub = CFStringCreateWithSubstring(nullptr, str, CFRangeMake(start, n));
        char buf[4096];
        if (CFStringGetCString(sub, buf, sizeof buf, kCFStringEncodingUTF8)) {
            std::string line = buf;
            while (!line.empty() && (line.back() == ' ' || line.back() == '\n')) line.pop_back();
            lines.push_back(line);
        }
        CFRelease(sub);
        start += n;
    }
    CFRelease(ts);
    CFRelease(as);
    CFRelease(attrs);
    CFRelease(str);
    return lines;
}
void Canvas2D::strokeText(const std::string& utf8, float x, float y) { textOp(utf8, x, y, false); }

void Canvas2D::drawImage(const std::vector<uint8_t>& rgba, int iw, int ih, float x, float y, float w, float h, bool cover) {
    if (rgba.empty()) return;
    CGColorSpaceRef cs = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CFDataRef data = CFDataCreate(nullptr, rgba.data(), (CFIndex)rgba.size());
    CGDataProviderRef prov = CGDataProviderCreateWithCFData(data);
    CGImageRef img = CGImageCreate(iw, ih, 8, 32, (size_t)iw * 4, cs, kCGImageAlphaLast | kCGBitmapByteOrder32Big, prov, nullptr, false, kCGRenderingIntentDefault);
    CGContextSaveGState(d->ctx);
    CGContextClipToRect(d->ctx, CGRectMake(x, y, w, h));
    float dx = x, dy = y, dw = w, dh = h;
    if (cover) {
        float s = std::max(w / iw, h / ih);
        dw = iw * s;
        dh = ih * s;
        dx = x + (w - dw) / 2;
        dy = y + (h - dh) / 2;
    }
    // undo the y flip locally so the image is upright
    CGContextTranslateCTM(d->ctx, dx, dy + dh);
    CGContextScaleCTM(d->ctx, 1, -1);
    CGContextSetInterpolationQuality(d->ctx, kCGInterpolationHigh);
    CGContextDrawImage(d->ctx, CGRectMake(0, 0, dw, dh), img);
    CGContextRestoreGState(d->ctx);
    CGImageRelease(img);
    CGDataProviderRelease(prov);
    CFRelease(data);
    CGColorSpaceRelease(cs);
}

void Canvas2D::copyFrom(const Canvas2D& o) {
    // copy in place: the CGBitmapContext keeps pointing at this buffer
    if (o.d->px.size() == d->px.size()) std::copy(o.d->px.begin(), o.d->px.end(), d->px.begin());
}

void Canvas2D::uploadPremultiplied(Texture& t) const {
    TextureOptions o;
    o.srgb = false;
    o.nearest = false;
    o.mipmaps = false;
    o.repeat = false;
    o.flipY = false;
    t.upload(w_, h_, d->px.data(), o);
}

void Canvas2D::upload(Texture& t, bool nearest, bool mipmaps) const {
    TextureOptions o;
    o.srgb = false;
    o.nearest = nearest;
    o.mipmaps = mipmaps && !nearest;
    o.repeat = false;
    // un-premultiply and flip (flipY) in one pass into a buffer reused by every canvas upload
    o.flipY = false;
    static std::vector<uint8_t> out;
    out.resize(d->px.size());
    size_t row = (size_t)w_ * 4;
    for (int y = 0; y < h_; ++y) {
        const uint8_t* s = d->px.data() + (size_t)y * row;
        uint8_t* dst = out.data() + (size_t)(h_ - 1 - y) * row;
        for (size_t i = 0; i < row; i += 4) {
            uint8_t a = s[i + 3];
            dst[i + 3] = a;
            for (int k = 0; k < 3; ++k) dst[i + k] = a == 0 || a == 255 ? s[i + k] : (uint8_t)std::min(255, s[i + k] * 255 / a);
        }
    }
    t.upload(w_, h_, out.data(), o);
}

void registerFonts() {
    static bool done = false;
    if (done) return;
    done = true;
    const char* files[] = {"Mali-Regular.ttf", "Mali-Medium.ttf", "Mali-SemiBold.ttf", "CormorantGaramond[wght].ttf",
                           "CormorantGaramond-Italic[wght].ttf", "Inter[opsz,wght].ttf"};
    for (const char* f : files) {
        std::string path = std::string(ASSET_DIR) + "/fonts/" + f;
        CFURLRef url = CFURLCreateFromFileSystemRepresentation(nullptr, (const UInt8*)path.c_str(), (CFIndex)path.size(), false);
        CTFontManagerRegisterFontsForURL(url, kCTFontManagerScopeProcess, nullptr);
        CFRelease(url);
    }
}
