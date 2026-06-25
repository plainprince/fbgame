#pragma once
#include <linux/fb.h>
#include <cstdint>
#include <cstddef>
#include <string>
#include <types.hpp>
#include <font.hpp>
#include <theme.hpp>
#include <sprite.hpp>

class Renderer2D {
public:
    Renderer2D();
    ~Renderer2D();

    bool init(int virtW, int virtH, Orientation orient, int fbNum = 0);
    void cleanup();

    void setFPS(int f) { fps = f; }
    int getFPS() const { return fps; }
    void setOrientation(Orientation o);
    void resize(int newW, int newH, Orientation orient);
    void setFont(Font* f) { font = f; }
    void setTheme(Theme* t) { theme = t; }

    void beginFrame();
    void endFrame();

    void clear(Color c = COLOR_BLACK);
    void pixel(int x, int y, Color c);
    void fillRect(int x, int y, int w, int h, Color c);
    void drawRect(int x, int y, int w, int h, Color c);
    void fillCircle(int cx, int cy, int r, Color c);
    void drawCircle(int cx, int cy, int r, Color c);
    void line(int x1, int y1, int x2, int y2, Color c);
    void text(int x, int y, const std::string& t, Color c, int maxW = 0, WrapMode mode = WrapMode::None, bool centered = false);
    void sprite(int x, int y, const Sprite& s);
    Color themeColor(const std::string& name) const;

    int width() const { return virtW; }
    int height() const { return virtH; }

private:
    void convertAndScale();
    void pushDiff();
    void drawDebugBorder();
    uint32_t nativePixel(const Color& c);
    int virtW, virtH;
    int pitchInPixels;
    int scaleFactor;
    int offsetX, offsetY;

    uint32_t* virtFB;
    uint32_t* prevFB;
    uint32_t* scaledFB;
    uint32_t* prevScaledFB;

    int fd;
    uint32_t* realFB;
    size_t realSize;
    struct fb_fix_screeninfo finfo;
    struct fb_var_screeninfo vinfo;

    Orientation orientation;
    int fps;
    Font* font;
    Theme* theme;
    bool initialized;
    int realW, realH;
};
