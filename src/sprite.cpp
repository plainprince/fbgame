#include <sprite.hpp>
#include <fstream>
#include <sstream>

bool SpriteLoader::load(const std::string& path, Sprite& out) {
    std::ifstream file(path);
    if (!file.is_open()) return false;
    std::string line;
    if (!std::getline(file, line)) return false;
    std::istringstream(line) >> out.width >> out.height;
    if (out.width <= 0 || out.height <= 0) return false;
    out.pixels.clear();
    out.pixels.reserve(out.width * out.height);
    int r, g, b, a;
    while (file >> r >> g >> b >> a) {
        out.pixels.push_back({(uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a});
    }
    return (int)out.pixels.size() == out.width * out.height;
}

bool SpriteLoader::save(const std::string& path, const Sprite& sprite) {
    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << sprite.width << " " << sprite.height << "\n";
    for (int i = 0; i < sprite.width * sprite.height; i++) {
        auto& c = sprite.pixels[i];
        file << (int)c.r << " " << (int)c.g << " " << (int)c.b << " " << (int)c.a;
        if ((i + 1) % sprite.width == 0) file << "\n";
        else file << "  ";
    }
    return true;
}
