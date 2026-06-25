#include <font.hpp>
#include <config.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

bool Font::load(const std::string& dataPath, const std::string& propPath) {
    cw = 8; ch = 8;
    chars.clear();
    glyphs.clear();
    charMap.clear();

    Properties props;
    if (!props.load(propPath)) {
        std::cerr << "font: failed to load " << propPath << "\n";
        return false;
    }

    cw = props.getInt("char_width", 8);
    ch = props.getInt("char_height", 8);
    cgap = props.getInt("char_gap", cgap);
    tgap = props.getInt("top_gap", tgap);
    bgap = props.getInt("bottom_gap", bgap);

    std::string wrapStr = props.getString("wrap_mode", "none");
    if (wrapStr == "word") wm = WrapMode::Word;
    else if (wrapStr == "char") wm = WrapMode::Char;
    else wm = WrapMode::None;

    std::string raw = props.getString("font_chars", "");
    if (raw.empty()) {
        std::cerr << "font: font_chars key missing in " << propPath << "\n";
        return false;
    }
    chars = raw;

    std::ifstream file(dataPath);
    if (!file.is_open()) {
        std::cerr << "font: failed to open " << dataPath << "\n";
        return false;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        lines.push_back(line);
    }

    int rowsPerChar = ch;
    int totalChars = chars.size();
    int expectedRows = totalChars * rowsPerChar;
    if ((int)lines.size() < expectedRows) {
        std::cerr << "font: " << dataPath << " has " << lines.size()
                  << " rows, need " << expectedRows
                  << " (" << totalChars << " chars x " << ch << " rows)\n";
        cw = 8; ch = 8; chars.clear(); glyphs.clear(); charMap.clear();
        return false;
    }

    for (int ci = 0; ci < totalChars; ci++) {
        Glyph g;
        g.width = cw;
        g.height = ch;
        g.actualWidth = cw;
        g.bitmap.resize(cw * ch);
        for (int row = 0; row < ch; row++) {
            const std::string& l = lines[ci * ch + row];
            for (int col = 0; col < cw && col < (int)l.size(); col++) {
                g.bitmap[row * cw + col] = (l[col] == '#' || l[col] == '@') ? 1 : 0;
            }
        }
        int aw = 0;
        for (int row = 0; row < ch; row++) {
            for (int col = cw - 1; col >= 0; col--) {
                if (g.bitmap[row * cw + col]) {
                    aw = std::max(aw, col + 1);
                    break;
                }
            }
        }
        g.actualWidth = std::max(1, aw);
        charMap[chars[ci]] = glyphs.size();
        glyphs.push_back(g);
    }
    return true;
}

Glyph Font::glyph(char c) const {
    auto it = charMap.find(c);
    if (it != charMap.end())
        return glyphs[it->second];
    auto fallback = charMap.find(' ');
    if (fallback != charMap.end())
        return glyphs[fallback->second];
    return Glyph{};
}

int Font::textWidth(const std::string& text) const {
    if (text.empty()) return 0;
    int w = 0;
    int sa = spaceAdvance();
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == ' ') {
            w += sa;
        } else {
            auto it = charMap.find(text[i]);
            w += (it != charMap.end()) ? glyphs[it->second].actualWidth : cw;
        }
        if (i + 1 < text.size()) w += cgap;
    }
    return w;
}

bool Font::hasGlyph(char c) const {
    return charMap.find(c) != charMap.end();
}
