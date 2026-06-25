#include <theme.hpp>
#include <config.hpp>
#include <sstream>

static Color parseColor(const std::string& s) {
    std::string c = s;
    if (c.size() >= 2 && c.front() == '"' && c.back() == '"')
        c = c.substr(1, c.size() - 2);
    uint8_t r = 0, g = 0, b = 0, a = 255;
    if (c.size() >= 6) {
        auto h = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return 0;
        };
        r = (h(c[0]) << 4) | h(c[1]);
        g = (h(c[2]) << 4) | h(c[3]);
        b = (h(c[4]) << 4) | h(c[5]);
        if (c.size() >= 8) a = (h(c[6]) << 4) | h(c[7]);
    }
    return {r, g, b, a};
}

bool Theme::load(const std::string& path) {
    Properties props;
    if (!props.load(path)) return false;
    themeName = props.getString("name", "default");
    for (auto& k : props.keys())
        if (k != "name")
            colors[k] = parseColor(props.getString(k));
    return true;
}

Color Theme::get(const std::string& name, const Color& fallback) const {
    auto it = colors.find(name);
    return it != colors.end() ? it->second : fallback;
}

Theme Theme::defaultTheme() {
    Theme t;
    t.themeName = "default";
    t.colors["primary"] = {68, 136, 255};
    t.colors["secondary"] = {136, 68, 255};
    t.colors["background"] = {17, 17, 17};
    t.colors["surface"] = {34, 34, 34};
    t.colors["text"] = {238, 238, 238};
    t.colors["text_dim"] = {136, 136, 136};
    t.colors["accent"] = {255, 68, 136};
    t.colors["success"] = {68, 255, 136};
    t.colors["error"] = {255, 68, 68};
    t.colors["warning"] = {255, 200, 68};
    t.colors["button"] = {51, 51, 51};
    t.colors["button_hover"] = {68, 68, 68};
    t.colors["border"] = {68, 68, 68};
    return t;
}
