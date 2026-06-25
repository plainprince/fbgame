#pragma once
#include <string>
#include <unordered_map>
#include <vector>

class Properties {
public:
    bool load(const std::string& path);
    bool save(const std::string& path);

    std::string getString(const std::string& key, const std::string& def = "") const;
    int getInt(const std::string& key, int def = 0) const;
    bool getBool(const std::string& key, bool def = false) const;
    float getFloat(const std::string& key, float def = 0.0f) const;

    void setString(const std::string& key, const std::string& value);
    void setInt(const std::string& key, int value);
    void setBool(const std::string& key, bool value);

    bool has(const std::string& key) const;
    std::vector<std::string> keys() const;
    std::string toString() const;
    bool fromString(const std::string& s);

private:
    std::unordered_map<std::string, std::string> props;
    std::string trim(const std::string& s) const;
};
