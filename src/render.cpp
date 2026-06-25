#include <render.hpp>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>

Renderer2D::Renderer2D()
    : virtW(0), virtH(0), pitchInPixels(0), scaleFactor(1), offsetX(0), offsetY(0),
      virtFB(nullptr), prevFB(nullptr), scaledFB(nullptr), prevScaledFB(nullptr),
      fd(-1), realFB(nullptr), realSize(0), orientation(Orientation::Horizontal),
      fps(10), font(nullptr), theme(nullptr), initialized(false), realW(0), realH(0) {}

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
    prevFB = new uint32_t[virtW * virtH]();

    scaleFactor = std::min(realW / virtW, realH / virtH);
    if (scaleFactor < 1) scaleFactor = 1;

    int scaledW = virtW * scaleFactor;
    int scaledH = virtH * scaleFactor;
    offsetX = (realW - scaledW) / 2;
    offsetY = (realH - scaledH) / 2;

    scaledFB = new uint32_t[scaledW * scaledH]();
    prevScaledFB = new uint32_t[scaledW * scaledH]();

    std::cout << "  scaledW=" << scaledW << " scaledH=" << scaledH
              << " scale=" << scaleFactor
              << " offsetX=" << offsetX << " offsetY=" << offsetY << "\n";

    std::cout << "  virtual=" << virtW << "x" << virtH
              << " orientation=" << (orient == Orientation::Horizontal ? "H" : "V")
              << " fps=" << fps << "\n";

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
        int sw = virtW * scaleFactor;
        int sh = virtH * scaleFactor;
        std::memset(prevScaledFB, 0, sw * sh * sizeof(uint32_t));
        return;
    }

    delete[] virtFB;
    delete[] prevFB;
    virtW = newW;
    virtH = newH;
    virtFB = new uint32_t[virtW * virtH]();
    prevFB = new uint32_t[virtW * virtH]();
    std::memset(virtFB, 0, virtW * virtH * sizeof(uint32_t));
    std::memset(prevFB, 0, virtW * virtH * sizeof(uint32_t));

    setOrientation(orient);
    std::memset(realFB, 0, realH * pitchInPixels * sizeof(uint32_t));
}

void Renderer2D::beginFrame() {
    std::swap(virtFB, prevFB);
    std::memset(virtFB, 0, virtW * virtH * sizeof(uint32_t));
}

void Renderer2D::endFrame() {
    convertAndScale();
    pushDiff();
    drawDebugBorder();
}

uint32_t Renderer2D::nativePixel(const Color& c) {
    uint32_t p = 0;
    if (c.a == 0) return 0;
    p |= ((uint32_t)c.r >> (8 - vinfo.red.length)) << vinfo.red.offset;
    p |= ((uint32_t)c.g >> (8 - vinfo.green.length)) << vinfo.green.offset;
    p |= ((uint32_t)c.b >> (8 - vinfo.blue.length)) << vinfo.blue.offset;
    if (vinfo.transp.length)
        p |= ((uint32_t)c.a >> (8 - vinfo.transp.length)) << vinfo.transp.offset;
    return p;
}

void Renderer2D::convertAndScale() {
    int s = scaleFactor;

    std::memset(scaledFB, 0, virtW * virtH * s * s * sizeof(uint32_t));

    for (int y = 0; y < virtH; y++) {
        for (int x = 0; x < virtW; x++) {
            uint32_t col = virtFB[y * virtW + x];
            if (col == 0) continue;
            uint32_t np = nativePixel(Color::unpack(col));
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

void Renderer2D::clear(Color c) {
    uint32_t p = c.pack();
    for (int i = 0; i < virtW * virtH; i++)
        virtFB[i] = p;
}

void Renderer2D::pixel(int x, int y, Color c) {
    if (x < 0 || x >= virtW || y < 0 || y >= virtH) return;
    virtFB[y * virtW + x] = c.pack();
}

void Renderer2D::fillRect(int x, int y, int w, int h, Color c) {
    int x0 = std::max(0, x), y0 = std::max(0, y);
    int x1 = std::min(virtW, x + w), y1 = std::min(virtH, y + h);
    uint32_t p = c.pack();
    for (int row = y0; row < y1; row++)
        for (int col = x0; col < x1; col++)
            virtFB[row * virtW + col] = p;
}

void Renderer2D::drawRect(int x, int y, int w, int h, Color c) {
    line(x, y, x + w - 1, y, c);
    line(x + w - 1, y, x + w - 1, y + h - 1, c);
    line(x + w - 1, y + h - 1, x, y + h - 1, c);
    line(x, y + h - 1, x, y, c);
}

void Renderer2D::fillCircle(int cx, int cy, int r, Color c) {
    uint32_t p = c.pack();
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            if (x * x + y * y <= r * r) {
                int px = cx + x, py = cy + y;
                if (px >= 0 && px < virtW && py >= 0 && py < virtH)
                    virtFB[py * virtW + px] = p;
            }
        }
    }
}

void Renderer2D::drawCircle(int cx, int cy, int r, Color c) {
    uint32_t p = c.pack();
    for (int a = 0; a < 360; a += 2) {
        double rad = a * 3.14159 / 180.0;
        int px = cx + (int)(r * cos(rad)), py = cy + (int)(r * sin(rad));
        if (px >= 0 && px < virtW && py >= 0 && py < virtH)
            virtFB[py * virtW + px] = p;
    }
}

void Renderer2D::line(int x1, int y1, int x2, int y2, Color c) {
    int dx = abs(x2 - x1), dy = -abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;
    uint32_t p = c.pack();
    while (true) {
        if (x1 >= 0 && x1 < virtW && y1 >= 0 && y1 < virtH)
            virtFB[y1 * virtW + x1] = p;
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
}

void Renderer2D::text(int x, int y, const std::string& t, Color c, int maxW, WrapMode mode, bool centered) {
    if (!font || t.empty()) return;
    uint32_t p = c.pack();
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
                    if (dx >= 0 && dx < virtW && dy >= 0 && dy < virtH)
                        virtFB[dy * virtW + dx] = p;
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

void Renderer2D::sprite(int x, int y, const Sprite& s) {
    for (int sy = 0; sy < s.height; sy++) {
        for (int sx = 0; sx < s.width; sx++) {
            Color c = s.pixels[sy * s.width + sx];
            if (c.a == 0) continue;
            int px = x + sx, py = y + sy;
            if (px >= 0 && px < virtW && py >= 0 && py < virtH)
                virtFB[py * virtW + px] = c.pack();
        }
    }
}

Color Renderer2D::themeColor(const std::string& name) const {
    return theme ? theme->get(name, COLOR_WHITE) : COLOR_WHITE;
}
