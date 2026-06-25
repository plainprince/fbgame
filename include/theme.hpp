#pragma once
#include <string>
#include <unordered_map>
#include <types.hpp>

class Theme {
public:
    bool load(const std::string& path);
    Color get(const std::string& name, const Color& fallback = COLOR_WHITE) const;
    std::string name() const { return themeName; }

    static Theme defaultTheme();

private:
    std::string themeName;
    std::unordered_map<std::string, Color> colors;
};
