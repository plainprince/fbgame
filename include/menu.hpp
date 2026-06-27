#pragma once
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <utility>
#include <types.hpp>
#include <render.hpp>
#include <input.hpp>
#include <theme.hpp>
#include <font.hpp>
struct MenuItem {
    std::string label;
    std::function<void()> action;
    std::shared_ptr<std::string> dynamicLabel;
    bool enabled = true;

    const std::string& labelStr() const {
        return dynamicLabel ? *dynamicLabel : label;
    }
};

struct SubGame {
    std::string name;
    std::string namespace_;
    std::string path;
    std::string mainScript;
};

class DefaultMenu {
public:
    DefaultMenu(Renderer2D* r, InputManager* inp, Font* f, Theme* t);
    ~DefaultMenu();

    void addItem(const MenuItem& item);
    template<typename Func>
    void addItem(std::string label, Func&& action) {
        items.push_back({std::move(label), std::function<void()>(std::forward<Func>(action)), nullptr});
    }
    void addSubGame(const SubGame& game);
    void setTitle(const std::string& t);
    void setSubtitle(const std::string& s);
    bool keyPress(Key k) const { return input->keyPress(k); }

    int run();
    void close();

private:
    void draw();
    bool handleInput();
    void drawButton(int x, int y, int w, int h, const std::string& label, bool hover);
    void drawText(int x, int y, const std::string& text, Color c, int maxW = 0, int monoMode = 0);
    int itemHeight() const;
    int wrappedLines(const std::string& text, int maxW) const;
    int contentHeight() const;
    void centerOnSelected();
    void clampScroll();

    std::vector<MenuItem> items;
    std::vector<SubGame> subGames;
    int selected = 0;
    int scrollOffset = 0;
    bool running = false;

    Renderer2D* renderer;
    InputManager* input;
    Font* font;
    Theme* theme;

    std::string title;
    std::string subtitle;
    int margin = 4;
    int itemSpacing = 2;
};
