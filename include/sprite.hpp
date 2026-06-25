#pragma once
#include <string>
#include <vector>
#include <types.hpp>

struct Sprite {
    int width = 0, height = 0;
    std::vector<Color> pixels;
};

class SpriteLoader {
public:
    bool load(const std::string& path, Sprite& out);
    bool save(const std::string& path, const Sprite& sprite);
};
