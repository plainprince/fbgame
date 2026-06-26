#pragma once
#include <string>
#include <unordered_map>
#include <types.hpp>
#include <config.hpp>

class Theme {
public:
    bool load(const std::string& path);
    Color get(const std::string& name, const Color& fallback = COLOR_WHITE) const;
    int getInt(const std::string& name, int fallback = 0) const;
    std::string name() const { return themeName; }

    static Theme defaultTheme();

private:
    std::string themeName;
    Properties raw;
    std::unordered_map<std::string, Color> colors;
};
