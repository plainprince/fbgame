#pragma once
#include <linux/fb.h>
#include <cstdint>
#include <cstddef>
#include <string>
#include <types.hpp>
#include <font.hpp>
#include <theme.hpp>
#include <sprite.hpp>

struct ColorMapEntry {
    uint32_t sourceColor;
    uint8_t targetGray;
};

struct ColorMapRange {
    uint8_t rMin, rMax;
    uint8_t gMin, gMax;
    uint8_t bMin, bMax;
    uint8_t targetGray;
};

class Renderer2D {
public:
    static constexpr int MAX_COLOR_MAP = 32;

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
    void setMonoColors(int n) { monoColors = n; }
    int getMonoColors() const { return monoColors; }
    void setMonoConversion(int mode) { monoConversion = mode; }
    int getMonoConversion() const { return monoConversion; }

    void beginFrame();
    void endFrame();

    void clear(Color c = COLOR_BLACK, int monoMode = 0);
    void pixel(int x, int y, Color c, int monoMode = 0);
    void fillRect(int x, int y, int w, int h, Color c, int monoMode = 0);
    void drawRect(int x, int y, int w, int h, Color c, int monoMode = 0);
    void fillCircle(int cx, int cy, int r, Color c, int monoMode = 0);
    void drawCircle(int cx, int cy, int r, Color c, int monoMode = 0);
    void line(int x1, int y1, int x2, int y2, Color c, int monoMode = 0);
    void text(int x, int y, const std::string& t, Color c, int maxW = 0, WrapMode mode = WrapMode::None, bool centered = false, int monoMode = 0);
    void sprite(int x, int y, const Sprite& s, int monoMode = 0);
    void saveScreenshot(const std::string& path) const;

    void mapColor(uint32_t sourceColor, uint8_t targetGray);
    void mapColorRange(uint8_t rMin, uint8_t rMax, uint8_t gMin, uint8_t gMax, uint8_t bMin, uint8_t bMax, uint8_t targetGray);
    void clearColorMap();

    Color themeColor(const std::string& name) const;

    int width() const { return virtW; }
    int height() const { return virtH; }

private:
    void convertAndScale();
    void pushDiff();
    void drawDebugBorder();
    int checkColorMap(uint32_t packedColor) const;
    int checkColorMapRange(uint32_t packedColor) const;
    uint32_t nativePixel(const Color& c, int monoMode = 0);
    int virtW, virtH;
    int pitchInPixels;
    int scaleFactor;
    int offsetX, offsetY;

    uint32_t* virtFB;
    int8_t* monoFB;
    ColorMapEntry colorMap[MAX_COLOR_MAP];
    int colorMapSize;
    ColorMapRange colorMapRanges[MAX_COLOR_MAP];
    int colorMapRangeSize;
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
    int monoColors;
    int monoConversion;
    int realW, realH;
};
