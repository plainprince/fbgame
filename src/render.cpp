#include <render.hpp>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <fstream>

Renderer2D::Renderer2D()
    : virtW(0), virtH(0), pitchInPixels(0), scaleFactor(1), offsetX(0), offsetY(0),
      virtFB(nullptr), monoFB(nullptr), colorMapSize(0), colorMapRangeSize(0),
      prevFB(nullptr), scaledFB(nullptr), prevScaledFB(nullptr),
      fd(-1), realFB(nullptr), realSize(0), orientation(Orientation::Horizontal),
      fps(10), font(nullptr), theme(nullptr), initialized(false),
      monoColors(0), monoConversion(0), realW(0), realH(0) {}

Renderer2D::~Renderer2D() { cleanup(); }

bool Renderer2D::init(int vW, int vH, Orientation orient, int fbNum) {
    std::string fbPath = "/dev/fb" + std::to_string(fbNum);
    fd = open(fbPath.c_str(), O_RDWR);
    if (fd < 0) { perror(fbPath.c_str()); return false; }

    ioctl(fd, FBIOGET_VSCREENINFO, &vinfo);
    ioctl(fd, FBIOGET_FSCREENINFO, &finfo);

    realW = vinfo.xres;
    realH = vinfo.yres;
    realSize = finfo.smem_len;
    pitchInPixels = finfo.line_length / 4;

    realFB = (uint32_t*)mmap(nullptr, realSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (realFB == MAP_FAILED) { perror("mmap"); return false; }

    orientation = orient;
    virtW = vW;
    virtH = vH;
    fps = 10;

    virtFB = new uint32_t[virtW * virtH]();
    monoFB = new int8_t[virtW * virtH]();
    prevFB = new uint32_t[virtW * virtH]();

    scaleFactor = std::min(realW / virtW, realH / virtH);
    if (scaleFactor < 1) scaleFactor = 1;

    int scaledW = virtW * scaleFactor;
    int scaledH = virtH * scaleFactor;
    offsetX = (realW - scaledW) / 2;
    offsetY = (realH - scaledH) / 2;

    scaledFB = new uint32_t[scaledW * scaledH]();
    prevScaledFB = new uint32_t[scaledW * scaledH]();

    if (vinfo.grayscale && monoColors == 0) {
        if (vinfo.bits_per_pixel <= 1) monoColors = 2;
        else if (vinfo.bits_per_pixel <= 2) monoColors = 4;
        else if (vinfo.bits_per_pixel <= 4) monoColors = 16;
        else monoColors = 256;
    }

    std::cout << "  scaledW=" << scaledW << " scaledH=" << scaledH
              << " scale=" << scaleFactor
              << " offsetX=" << offsetX << " offsetY=" << offsetY << "\n";

    std::cout << "  virtual=" << virtW << "x" << virtH
              << " orientation=" << (orient == Orientation::Horizontal ? "H" : "V")
              << " fps=" << fps
              << " colors=" << (monoColors ? std::to_string(monoColors) + " (mono)" : "full") << "\n";

    std::cout << "\033[2J\033[H" << std::flush;
    std::cout << "\033[?25l" << std::flush;

    std::memset(realFB, 0, realH * pitchInPixels * sizeof(uint32_t));

    initialized = true;
    return true;
}

void Renderer2D::cleanup() {
    if (!initialized) return;
    initialized = false;
    std::cout << "\033[?25h" << std::flush;

    delete[] virtFB;
    delete[] monoFB;
    delete[] prevFB;
    delete[] scaledFB;
    delete[] prevScaledFB;

    if (realFB && realFB != MAP_FAILED) munmap(realFB, realSize);
    if (fd >= 0) close(fd);
}

void Renderer2D::setOrientation(Orientation o) {
    orientation = o;
    scaleFactor = std::min(realW / virtW, realH / virtH);
    if (scaleFactor < 1) scaleFactor = 1;
    offsetX = (realW - virtW * scaleFactor) / 2;
    offsetY = (realH - virtH * scaleFactor) / 2;

    delete[] scaledFB;
    delete[] prevScaledFB;
    int sw = virtW * scaleFactor;
    int sh = virtH * scaleFactor;
    scaledFB = new uint32_t[sw * sh]();
    prevScaledFB = new uint32_t[sw * sh]();
}

void Renderer2D::resize(int newW, int newH, Orientation orient) {
    if (newW == virtW && newH == virtH && orient == orientation) {
        std::memset(realFB, 0, realH * pitchInPixels * sizeof(uint32_t));
        std::memset(monoFB, 0, virtW * virtH * sizeof(int8_t));
        int sw = virtW * scaleFactor;
        int sh = virtH * scaleFactor;
        std::memset(prevScaledFB, 0, sw * sh * sizeof(uint32_t));
        colorMapSize = 0;
        colorMapRangeSize = 0;
        return;
    }

    delete[] virtFB;
    delete[] monoFB;
    delete[] prevFB;
    virtW = newW;
    virtH = newH;
    virtFB = new uint32_t[virtW * virtH]();
    monoFB = new int8_t[virtW * virtH]();
    prevFB = new uint32_t[virtW * virtH]();
    std::memset(virtFB, 0, virtW * virtH * sizeof(uint32_t));
    std::memset(monoFB, 0, virtW * virtH * sizeof(int8_t));
    std::memset(prevFB, 0, virtW * virtH * sizeof(uint32_t));
    colorMapSize = 0;
    colorMapRangeSize = 0;

    setOrientation(orient);
    std::memset(realFB, 0, realH * pitchInPixels * sizeof(uint32_t));
}

void Renderer2D::beginFrame() {
    std::swap(virtFB, prevFB);
    std::memset(virtFB, 0, virtW * virtH * sizeof(uint32_t));
    std::memset(monoFB, 0, virtW * virtH * sizeof(int8_t));
}

void Renderer2D::endFrame() {
    convertAndScale();
    pushDiff();
    drawDebugBorder();
}

uint32_t Renderer2D::nativePixel(const Color& c, int monoMode) {
    uint32_t p = 0;
    if (c.a == 0) return 0;
    Color col = c;
    if (monoColors >= 2) {
        if (monoMode == 1) {
            col = {255, 255, 255, c.a};
        } else if (monoMode == 2) {
            col = {0, 0, 0, c.a};
        } else if (monoColors == 2) {
            int lum = ((int)c.r * 77 + (int)c.g * 150 + (int)c.b * 29) >> 8;
            col = (lum < 50) ? Color{0, 0, 0, c.a} : Color{255, 255, 255, c.a};
        } else {
            int lum = ((int)c.r * 77 + (int)c.g * 150 + (int)c.b * 29) >> 8;
            int level = (lum * (monoColors - 1) + 127) / 255;
            if (level < 0) level = 0;
            if (level >= monoColors) level = monoColors - 1;
            uint8_t gray = (uint8_t)(level * 255 / (monoColors - 1));
            col = {gray, gray, gray, c.a};
        }
    }
    p |= ((uint32_t)col.r >> (8 - vinfo.red.length)) << vinfo.red.offset;
    p |= ((uint32_t)col.g >> (8 - vinfo.green.length)) << vinfo.green.offset;
    p |= ((uint32_t)col.b >> (8 - vinfo.blue.length)) << vinfo.blue.offset;
    if (vinfo.transp.length)
        p |= ((uint32_t)col.a >> (8 - vinfo.transp.length)) << vinfo.transp.offset;
    return p;
}

static float sobelGradient(const uint32_t* fb, int w, int h, int cx, int cy) {
    auto getLum = [&](int px, int py) -> float {
        if (px < 0 || px >= w || py < 0 || py >= h) return 0.0f;
        uint32_t p = fb[py * w + px];
        uint8_t r = (p >> 24) & 0xFF;
        uint8_t g = (p >> 16) & 0xFF;
        uint8_t b = (p >> 8) & 0xFF;
        return (r * 0.299f + g * 0.587f + b * 0.114f) / 255.0f;
    };

    float tl = getLum(cx-1, cy-1), tc = getLum(cx, cy-1), tr = getLum(cx+1, cy-1);
    float ml = getLum(cx-1, cy),                            mr = getLum(cx+1, cy);
    float bl = getLum(cx-1, cy+1), bc = getLum(cx, cy+1), br = getLum(cx+1, cy+1);

    float gx = (tr + 2.0f*mr + br) - (tl + 2.0f*ml + bl);
    float gy = (bl + 2.0f*bc + br) - (tl + 2.0f*tc + tr);
    return std::sqrt(gx*gx + gy*gy);
}

void Renderer2D::convertAndScale() {
    int s = scaleFactor;

    std::memset(scaledFB, 0, virtW * virtH * s * s * sizeof(uint32_t));

    for (int y = 0; y < virtH; y++) {
        for (int x = 0; x < virtW; x++) {
            int idx = y * virtW + x;
            uint32_t col = virtFB[idx];
            if (col == 0 && monoFB[idx] == 0) continue;
            uint32_t np;
            if (monoColors >= 2) {
                int mm = monoFB[idx];
                if (mm == 1) {
                    np = nativePixel(COLOR_WHITE, 0);
                } else if (mm == 2) {
                    np = nativePixel(COLOR_BLACK, 0);
                } else {
                    int gray = checkColorMap(col);
                    if (gray >= 0) {
                        np = nativePixel(Color{(uint8_t)gray, (uint8_t)gray, (uint8_t)gray, 255}, 0);
                    } else {
                        Color c = Color::unpack(col);
                        if (monoConversion == 1 && monoColors > 2) {
                            float grad = sobelGradient(virtFB, virtW, virtH, x, y);
                            if (grad > 0.5f) {
                                int lum = (int)c.r * 77 + (int)c.g * 150 + (int)c.b * 29;
                                np = nativePixel(lum > 32640 ? COLOR_WHITE : COLOR_BLACK, 0);
                            } else {
                                np = nativePixel(c, 0);
                            }
                        } else {
                            np = nativePixel(c, 0);
                        }
                    }
                }
            } else {
                np = nativePixel(Color::unpack(col), 0);
            }
            for (int dy = 0; dy < s; dy++)
                for (int dx = 0; dx < s; dx++)
                    scaledFB[(y * s + dy) * (virtW * s) + (x * s + dx)] = np;
        }
    }
}

void Renderer2D::pushDiff() {
    int sw = virtW * scaleFactor;
    int sh = virtH * scaleFactor;

    uint32_t* cur = scaledFB;
    uint32_t* prev = prevScaledFB;

    int rp = pitchInPixels;

    for (int y = 0; y < sh; y++) {
        int realY = offsetY + y;
        if (realY < 0 || realY >= realH) continue;
        for (int x = 0; x < sw; x++) {
            int realX = offsetX + x;
            if (realX < 0 || realX >= realW) continue;
            uint32_t nc = cur[y * sw + x];
            uint32_t pc = prev[y * sw + x];
            if (nc != pc) {
                realFB[realY * rp + realX] = nc;
                prev[y * sw + x] = nc;
            }
        }
    }
}

void Renderer2D::drawDebugBorder() {
    int sw = virtW * scaleFactor;
    int sh = virtH * scaleFactor;
    int rp = pitchInPixels;
    uint32_t bc = 0xFFFFFFFF;

    if (sw <= 0 || sh <= 0) return;

    for (int x = 0; x < sw; x++) {
        int rx = offsetX + x;
        if (rx >= 0 && rx < realW) {
            if (offsetY >= 0 && offsetY < realH)
                realFB[offsetY * rp + rx] = bc;
            int by = offsetY + sh - 1;
            if (by >= 0 && by < realH)
                realFB[by * rp + rx] = bc;
        }
    }
    for (int y = 0; y < sh; y++) {
        int ry = offsetY + y;
        if (ry >= 0 && ry < realH) {
            if (offsetX >= 0 && offsetX < realW)
                realFB[ry * rp + offsetX] = bc;
            int rx = offsetX + sw - 1;
            if (rx >= 0 && rx < realW)
                realFB[ry * rp + rx] = bc;
        }
    }
}

void Renderer2D::clear(Color c, int monoMode) {
    uint32_t p = c.pack();
    int8_t mm = (int8_t)monoMode;
    for (int i = 0; i < virtW * virtH; i++) {
        virtFB[i] = p;
        monoFB[i] = mm;
    }
}

void Renderer2D::pixel(int x, int y, Color c, int monoMode) {
    if (x < 0 || x >= virtW || y < 0 || y >= virtH) return;
    int idx = y * virtW + x;
    virtFB[idx] = c.pack();
    monoFB[idx] = (int8_t)monoMode;
}

void Renderer2D::fillRect(int x, int y, int w, int h, Color c, int monoMode) {
    int x0 = std::max(0, x), y0 = std::max(0, y);
    int x1 = std::min(virtW, x + w), y1 = std::min(virtH, y + h);
    uint32_t p = c.pack();
    int8_t mm = (int8_t)monoMode;
    for (int row = y0; row < y1; row++)
        for (int col = x0; col < x1; col++) {
            int idx = row * virtW + col;
            virtFB[idx] = p;
            monoFB[idx] = mm;
        }
}

void Renderer2D::drawRect(int x, int y, int w, int h, Color c, int monoMode) {
    line(x, y, x + w - 1, y, c, monoMode);
    line(x + w - 1, y, x + w - 1, y + h - 1, c, monoMode);
    line(x + w - 1, y + h - 1, x, y + h - 1, c, monoMode);
    line(x, y + h - 1, x, y, c, monoMode);
}

void Renderer2D::fillCircle(int cx, int cy, int r, Color c, int monoMode) {
    uint32_t p = c.pack();
    int8_t mm = (int8_t)monoMode;
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            if (x * x + y * y <= r * r) {
                int px = cx + x, py = cy + y;
                if (px >= 0 && px < virtW && py >= 0 && py < virtH) {
                    int idx = py * virtW + px;
                    virtFB[idx] = p;
                    monoFB[idx] = mm;
                }
            }
        }
    }
}

void Renderer2D::drawCircle(int cx, int cy, int r, Color c, int monoMode) {
    uint32_t p = c.pack();
    int8_t mm = (int8_t)monoMode;
    for (int a = 0; a < 360; a += 2) {
        double rad = a * 3.14159 / 180.0;
        int px = cx + (int)(r * cos(rad)), py = cy + (int)(r * sin(rad));
        if (px >= 0 && px < virtW && py >= 0 && py < virtH) {
            int idx = py * virtW + px;
            virtFB[idx] = p;
            monoFB[idx] = mm;
        }
    }
}

void Renderer2D::line(int x1, int y1, int x2, int y2, Color c, int monoMode) {
    int dx = abs(x2 - x1), dy = -abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;
    uint32_t p = c.pack();
    int8_t mm = (int8_t)monoMode;
    while (true) {
        if (x1 >= 0 && x1 < virtW && y1 >= 0 && y1 < virtH) {
            int idx = y1 * virtW + x1;
            virtFB[idx] = p;
            monoFB[idx] = mm;
        }
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
}

void Renderer2D::text(int x, int y, const std::string& t, Color c, int maxW, WrapMode mode, bool centered, int monoMode) {
    if (!font || t.empty()) return;
    uint32_t p = c.pack();
    int8_t mm = (int8_t)monoMode;
    int fw = font->charWidth();
    int fh = font->charHeight();
    int cg = font->charGap();
    int tg = font->topGap();
    int bg = font->bottomGap();
    int sa = font->spaceAdvance();
    int lineH = fh + tg + bg;

    auto charAdvance = [&](char ch) -> int {
        if (ch == ' ') return sa;
        return font->glyph(ch).actualWidth + cg;
    };

    auto drawGlyph = [&](int px, int py, char ch) -> int {
        if (ch == ' ') return sa;
        Glyph g = font->glyph(ch);
        int bx = px;
        int by = py + tg;
        for (int gy = 0; gy < fh && gy < g.height; gy++) {
            for (int gx = 0; gx < fw && gx < g.actualWidth; gx++) {
                if (g.bitmap[gy * fw + gx]) {
                    int dx = bx + gx, dy = by + gy;
                    if (dx >= 0 && dx < virtW && dy >= 0 && dy < virtH) {
                        int idx = dy * virtW + dx;
                        virtFB[idx] = p;
                        monoFB[idx] = mm;
                    }
                }
            }
        }
        return g.actualWidth + cg;
    };

    auto drawLine = [&](int px, int py, const std::string& s) {
        for (size_t i = 0; i < s.size(); i++)
            px += drawGlyph(px, py, s[i]);
    };

    if (maxW <= 0 || mode == WrapMode::None) {
        int px = x;
        for (size_t i = 0; i < t.size(); i++)
            px += drawGlyph(px, y, t[i]);
        return;
    }

    if (mode == WrapMode::Char) {
        int px = x;
        int py = y;
        for (size_t i = 0; i < t.size(); i++) {
            int ga = charAdvance(t[i]);
            if (px + ga > x + maxW && px > x) {
                py += lineH;
                px = x;
            }
            px += drawGlyph(px, py, t[i]);
        }
        return;
    }

    int curY = y;
    std::string curLine;
    int flushCount = 0;

    auto flushLine = [&]() {
        std::string line = curLine;
        if (centered) {
            size_t s = line.find_first_not_of(' ');
            size_t e = line.find_last_not_of(' ');
            if (s != std::string::npos && e != std::string::npos)
                line = line.substr(s, e - s + 1);
            else
                line.clear();
        }
        if (!line.empty()) {
            int startX = centered ? x + (maxW - font->textWidth(line)) / 2 : x;
            drawLine(startX, curY, line);
        }
        curLine.clear();
        curY += lineH;
        if (centered) flushCount++;
    };

    bool pendingSpace = false;
    size_t i = 0;
    while (i < t.size()) {
        if (t[i] == '\n') {
            pendingSpace = false;
            flushLine();
            i++;
            continue;
        }

        if (t[i] == ' ') {
            if (!curLine.empty()) {
                std::string test = curLine + " ";
                if (font->textWidth(test) <= maxW) {
                    pendingSpace = true;
                } else {
                    pendingSpace = false;
                    flushLine();
                }
            }
            i++;
            continue;
        }

        size_t wordEnd = i;
        while (wordEnd < t.size() && t[wordEnd] != ' ' && t[wordEnd] != '\n')
            wordEnd++;

        std::string word = t.substr(i, wordEnd - i);
        int wordW = font->textWidth(word);

        if (wordW > maxW) {
            pendingSpace = false;
            if (!curLine.empty()) flushLine();
            std::vector<int> advances(word.size());
            for (size_t k = 0; k < word.size(); k++)
                advances[k] = charAdvance(word[k]);
            size_t pos = 0;
            while (pos < word.size()) {
                size_t chunkEnd = pos;
                int chunkW = 0;
                while (chunkEnd < word.size()) {
                    int nextW = chunkW + advances[chunkEnd];
                    if (nextW > maxW && chunkEnd > pos) break;
                    chunkW = nextW;
                    chunkEnd++;
                }
                if (chunkEnd == pos) chunkEnd = pos + 1;
                int csx = centered ? x + (maxW - chunkW) / 2 : x;
                for (size_t k = pos; k < chunkEnd; k++)
                    csx += drawGlyph(csx, curY, word[k]);
                pos = chunkEnd;
                if (pos < word.size()) curY += lineH + (centered && flushCount > 0 ? 1 : 0);
            }
            i = wordEnd;
            continue;
        }

        std::string test = pendingSpace ? curLine + " " + word : (curLine.empty() ? word : curLine + " " + word);
        if (font->textWidth(test) > maxW && !curLine.empty()) {
            pendingSpace = false;
            flushLine();
        }

        if (curLine.empty()) {
            curLine = word;
        } else {
            if (pendingSpace) curLine += " ";
            curLine += word;
        }
        pendingSpace = false;

        i = wordEnd;
    }

    if (!curLine.empty()) flushLine();
}

void Renderer2D::sprite(int x, int y, const Sprite& s, int monoMode) {
    int8_t mm = (int8_t)monoMode;
    for (int sy = 0; sy < s.height; sy++) {
        for (int sx = 0; sx < s.width; sx++) {
            Color c = s.pixels[sy * s.width + sx];
            if (c.a == 0) continue;
            int px = x + sx, py = y + sy;
            if (px >= 0 && px < virtW && py >= 0 && py < virtH) {
                int idx = py * virtW + px;
                virtFB[idx] = c.pack();
                monoFB[idx] = mm;
            }
        }
    }
}

void Renderer2D::saveScreenshot(const std::string& path) const {
    std::ofstream f(path, std::ios::binary);
    if (!f) return;
    f << "P6\n" << virtW << " " << virtH << "\n255\n";
    for (int y = 0; y < virtH; y++) {
        for (int x = 0; x < virtW; x++) {
            int idx = y * virtW + x;
            if (monoColors >= 2) {
                int mm = monoFB[idx];
                Color c = Color::unpack(virtFB[idx]);
                if (mm == 1) {
                    c = {255, 255, 255, 255};
                } else if (mm == 2) {
                    c = {0, 0, 0, 255};
                } else {
                    int gray = checkColorMap(virtFB[idx]);
                    if (gray >= 0) {
                        uint8_t g = (uint8_t)gray;
                        c = {g, g, g, 255};
                    } else {
                        if (monoConversion == 1 && monoColors > 2) {
                            float grad = sobelGradient(virtFB, virtW, virtH, x, y);
                            if (grad > 0.5f) {
                                int lum = (int)c.r * 77 + (int)c.g * 150 + (int)c.b * 29;
                                uint8_t v = lum > 32640 ? 255 : 0;
                                c = {v, v, v, 255};
                            } else {
                                int lum = ((int)c.r * 77 + (int)c.g * 150 + (int)c.b * 29) >> 8;
                                int level = (lum * (monoColors - 1) + 127) / 255;
                                if (level < 0) level = 0;
                                if (level >= monoColors) level = monoColors - 1;
                                uint8_t gray = (uint8_t)(level * 255 / (monoColors - 1));
                                c = {gray, gray, gray, 255};
                            }
                        } else {
                            int lum = ((int)c.r * 77 + (int)c.g * 150 + (int)c.b * 29) >> 8;
                            int level = (lum * (monoColors - 1) + 127) / 255;
                            if (level < 0) level = 0;
                            if (level >= monoColors) level = monoColors - 1;
                            uint8_t gray = (uint8_t)(level * 255 / (monoColors - 1));
                            c = {gray, gray, gray, 255};
                        }
                    }
                }
                f.put(c.r);
                f.put(c.g);
                f.put(c.b);
            } else {
                Color c = Color::unpack(virtFB[idx]);
                f.put(c.r);
                f.put(c.g);
                f.put(c.b);
            }
        }
    }
}

Color Renderer2D::themeColor(const std::string& name) const {
    return theme ? theme->get(name, COLOR_WHITE) : COLOR_WHITE;
}

int Renderer2D::checkColorMap(uint32_t packedColor) const {
    for (int i = 0; i < colorMapSize; i++) {
        if (colorMap[i].sourceColor == packedColor)
            return colorMap[i].targetGray;
    }
    Color c = Color::unpack(packedColor);
    for (int i = 0; i < colorMapRangeSize; i++) {
        auto& r = colorMapRanges[i];
        if (c.r >= r.rMin && c.r <= r.rMax &&
            c.g >= r.gMin && c.g <= r.gMax &&
            c.b >= r.bMin && c.b <= r.bMax)
            return r.targetGray;
    }
    return -1;
}

int Renderer2D::checkColorMapRange(uint32_t packedColor) const {
    Color c = Color::unpack(packedColor);
    for (int i = 0; i < colorMapRangeSize; i++) {
        auto& r = colorMapRanges[i];
        if (c.r >= r.rMin && c.r <= r.rMax &&
            c.g >= r.gMin && c.g <= r.gMax &&
            c.b >= r.bMin && c.b <= r.bMax)
            return r.targetGray;
    }
    return -1;
}

void Renderer2D::mapColor(uint32_t sourceColor, uint8_t targetGray) {
    for (int i = 0; i < colorMapSize; i++) {
        if (colorMap[i].sourceColor == sourceColor) {
            colorMap[i].targetGray = targetGray;
            return;
        }
    }
    if (colorMapSize < MAX_COLOR_MAP) {
        colorMap[colorMapSize].sourceColor = sourceColor;
        colorMap[colorMapSize].targetGray = targetGray;
        colorMapSize++;
    }
}

void Renderer2D::mapColorRange(uint8_t rMin, uint8_t rMax, uint8_t gMin, uint8_t gMax, uint8_t bMin, uint8_t bMax, uint8_t targetGray) {
    if (colorMapRangeSize < MAX_COLOR_MAP) {
        colorMapRanges[colorMapRangeSize].rMin = rMin;
        colorMapRanges[colorMapRangeSize].rMax = rMax;
        colorMapRanges[colorMapRangeSize].gMin = gMin;
        colorMapRanges[colorMapRangeSize].gMax = gMax;
        colorMapRanges[colorMapRangeSize].bMin = bMin;
        colorMapRanges[colorMapRangeSize].bMax = bMax;
        colorMapRanges[colorMapRangeSize].targetGray = targetGray;
        colorMapRangeSize++;
    }
}

void Renderer2D::clearColorMap() {
    colorMapSize = 0;
    colorMapRangeSize = 0;
}
