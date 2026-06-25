#include <config.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

std::string Properties::trim(const std::string& s) const {
    auto start = std::find_if_not(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c); });
    auto end = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char c) { return std::isspace(c); }).base();
    return (start < end) ? std::string(start, end) : "";
}

bool Properties::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;
    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));
        if (!key.empty()) {
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
                value = value.substr(1, value.size() - 2);
            props[key] = value;
        }
    }
    return true;
}

bool Properties::save(const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) return false;
    for (auto& [k, v] : props)
        file << k << "=\"" << v << "\"\n";
    return true;
}

std::string Properties::getString(const std::string& key, const std::string& def) const {
    auto it = props.find(key);
    return it != props.end() ? it->second : def;
}

int Properties::getInt(const std::string& key, int def) const {
    auto it = props.find(key);
    if (it == props.end()) return def;
    try { return std::stoi(it->second); } catch (...) { return def; }
}

bool Properties::getBool(const std::string& key, bool def) const {
    auto it = props.find(key);
    if (it == props.end()) return def;
    std::string v = it->second;
    std::transform(v.begin(), v.end(), v.begin(), ::tolower);
    return v == "true" || v == "1" || v == "yes";
}

float Properties::getFloat(const std::string& key, float def) const {
    auto it = props.find(key);
    if (it == props.end()) return def;
    try { return std::stof(it->second); } catch (...) { return def; }
}

void Properties::setString(const std::string& key, const std::string& value) { props[key] = value; }
void Properties::setInt(const std::string& key, int value) { props[key] = std::to_string(value); }
void Properties::setBool(const std::string& key, bool value) { props[key] = value ? "true" : "false"; }
bool Properties::has(const std::string& key) const { return props.find(key) != props.end(); }
std::vector<std::string> Properties::keys() const {
    std::vector<std::string> k;
    for (auto& [key, _] : props) k.push_back(key);
    return k;
}

std::string Properties::toString() const {
    std::string r;
    for (auto& [k, v] : props)
        r += k + "=" + v + "\n";
    return r;
}

bool Properties::fromString(const std::string& s) {
    props.clear();
    std::istringstream stream(s);
    std::string line;
    while (std::getline(stream, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));
        if (!key.empty()) props[key] = value;
    }
    return true;
}
