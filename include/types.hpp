#pragma once
#include <cstdint>
#include <csignal>
#include <string>

extern volatile std::sig_atomic_t appRunning;

enum class Orientation { Horizontal, Vertical };
enum class WrapMode { None, Word, Char };

struct Color {
    uint8_t r, g, b, a;

    Color() : r(0), g(0), b(0), a(255) {}
    Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) : r(r), g(g), b(b), a(a) {}

    uint32_t pack() const { return (r << 24) | (g << 16) | (b << 8) | a; }

    static Color unpack(uint32_t p) {
        return Color((p >> 24) & 0xFF, (p >> 16) & 0xFF, (p >> 8) & 0xFF, p & 0xFF);
    }

    bool operator==(const Color& o) const { return r == o.r && g == o.g && b == o.b && a == o.a; }
    bool operator!=(const Color& o) const { return !(*this == o); }
};

const Color COLOR_BLACK   = {  0,   0,   0};
const Color COLOR_WHITE   = {255, 255, 255};
const Color COLOR_RED     = {255,   0,   0};
const Color COLOR_GREEN   = {  0, 255,   0};
const Color COLOR_BLUE    = {  0,   0, 255};
const Color COLOR_YELLOW  = {255, 255,   0};
const Color COLOR_CYAN    = {  0, 255, 255};
const Color COLOR_MAGENTA = {255,   0, 255};
const Color COLOR_ORANGE  = {255, 165,   0};
const Color COLOR_PURPLE  = {128,   0, 128};
const Color COLOR_GREY    = {128, 128, 128};
const Color COLOR_TRANSP  = {  0,   0,   0,   0};

struct Vec2 { int x, y; };
struct Rect { int x, y, w, h; };
