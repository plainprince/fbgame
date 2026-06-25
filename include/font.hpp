#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <types.hpp>

struct Glyph {
    int width, height;
    int actualWidth;
    std::vector<uint8_t> bitmap;
};

class Font {
public:
    bool load(const std::string& dataPath, const std::string& propPath);
    Glyph glyph(char c) const;
    int charWidth() const { return cw; }
    int charHeight() const { return ch; }
    int charGap() const { return cgap; }
    int topGap() const { return tgap; }
    int bottomGap() const { return bgap; }
    void setCharGap(int g) { cgap = g; }
    void setTopGap(int g) { tgap = g; }
    void setBottomGap(int g) { bgap = g; }
    int textWidth(const std::string& text) const;
    int spaceAdvance() const { return std::max(2, cw / 2 + 1); }
    int textHeight() const { return ch + tgap + bgap; }
    bool hasGlyph(char c) const;
    WrapMode wrapMode() const { return wm; }
    void setWrapMode(WrapMode m) { wm = m; }

private:
    int cw = 8, ch = 8;
    int cgap = 1, tgap = 1, bgap = 0;
    WrapMode wm = WrapMode::None;
    std::string chars;
    std::vector<Glyph> glyphs;
    std::unordered_map<char, int> charMap;
};
