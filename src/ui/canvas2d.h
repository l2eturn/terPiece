#pragma once
// HTML-canvas-like 2D drawing on CoreGraphics + CoreText (correct Thai shaping, emoji, the web fonts).
// Used for speech bubbles, the Zzz sprite, the HUD, the minimap and the title screen.
// Coordinates are y-down like a canvas.
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Texture;

struct RGBA {
    float r = 0, g = 0, b = 0, a = 1;
};
// '#rgb', '#rrggbb', '#rrggbbaa', 'rgb(...)', 'rgba(...)', 'white' / 'black' / 'transparent'
RGBA cssColor(const std::string& s);

enum class TextAlign { Left, Center, Right };
enum class TextBaseline { Alphabetic, Top, Middle, Bottom };

struct GradientStop {
    float offset;
    RGBA color;
};

class Canvas2D {
public:
    Canvas2D(int w, int h);
    ~Canvas2D();
    Canvas2D(const Canvas2D&) = delete;
    Canvas2D& operator=(const Canvas2D&) = delete;

    int width() const { return w_; }
    int height() const { return h_; }
    void resize(int w, int h);

    // ---------- state ----------
    void save();
    void restore();
    void translate(float x, float y);
    void rotate(float a);
    void scale(float x, float y);
    void setFill(const RGBA& c);
    void setFill(const std::string& css) { setFill(cssColor(css)); }
    void setStroke(const RGBA& c);
    void setStroke(const std::string& css) { setStroke(cssColor(css)); }
    void setLineWidth(float w);
    void setLineCap(bool round);
    void setGlobalAlpha(float a);
    // CSS-like font: family ("Mali", "Cormorant Garamond", "Inter", or any installed font), px size, weight 100..900
    void setFont(const std::string& family, float size, int weight = 400, bool italic = false);
    void setTextAlign(TextAlign a) { align_ = a; }
    void setTextBaseline(TextBaseline b) { baseline_ = b; }
    void setLetterSpacing(float em) { tracking_ = em; }
    void setShadow(float blur, const RGBA& color, float dx = 0, float dy = 0);
    void clearShadow();
    // additive "lighter" composite (glows)
    void setAdditive(bool on);

    // ---------- shapes ----------
    void clear();
    void clearRect(float x, float y, float w, float h);
    void fillRect(float x, float y, float w, float h);
    void strokeRect(float x, float y, float w, float h);
    void beginPath();
    void closePath();
    void moveTo(float x, float y);
    void lineTo(float x, float y);
    void quadTo(float cx, float cy, float x, float y);
    void arc(float x, float y, float r, float a0, float a1, bool ccw = false);
    void ellipse(float x, float y, float rx, float ry, float rot = 0);
    void rect(float x, float y, float w, float h);
    void roundRect(float x, float y, float w, float h, float r);
    void fill();
    void stroke();
    void clip();
    // fill the current path with a gradient
    void fillLinearGradient(float x0, float y0, float x1, float y1, const std::vector<GradientStop>& stops);
    void fillRadialGradient(float x0, float y0, float r0, float x1, float y1, float r1, const std::vector<GradientStop>& stops);

    // ---------- text ----------
    float measureText(const std::string& utf8);
    void fillText(const std::string& utf8, float x, float y);
    void strokeText(const std::string& utf8, float x, float y);
    // split into lines that fit maxWidth (CoreText line breaking, handles Thai without spaces)
    std::vector<std::string> wrapText(const std::string& utf8, float maxWidth);

    // ---------- images ----------
    // draw a decoded RGBA image (rows top to bottom) into a rectangle; cover = CSS background-size: cover
    void drawImage(const std::vector<uint8_t>& rgba, int iw, int ih, float x, float y, float w, float h, bool cover = false);

    // drawImage(otherCanvas, 0, 0) for a canvas of the same size: copies its pixels (used by the minimap)
    void copyFrom(const Canvas2D& o);

    // straight-alpha RGBA8 pixels, rows top to bottom
    std::vector<uint8_t> pixels() const;
    // upload into a texture (linear encoding like CanvasTexture)
    void upload(Texture& t, bool nearest = false) const;

private:
    void textOp(const std::string& utf8, float x, float y, bool fill);
    struct Impl;
    std::unique_ptr<Impl> d;
    int w_, h_;
    TextAlign align_ = TextAlign::Left;
    TextBaseline baseline_ = TextBaseline::Alphabetic;
    float tracking_ = 0;
};

// Register the bundled fonts in assets/fonts (call once at start-up)
void registerFonts();
