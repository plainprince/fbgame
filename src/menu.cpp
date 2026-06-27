#include <menu.hpp>
#include <algorithm>
#include <iostream>
#include <thread>
#include <chrono>

DefaultMenu::DefaultMenu(Renderer2D* r, InputManager* inp, Font* f, Theme* t)
    : renderer(r), input(inp), font(f), theme(t) {
    title = "FBGAME MENU";
    subtitle = "Select a game";
}

DefaultMenu::~DefaultMenu() {}

void DefaultMenu::addItem(const MenuItem& item) { items.push_back(item); }
void DefaultMenu::addSubGame(const SubGame& game) { subGames.push_back(game); }
void DefaultMenu::setTitle(const std::string& t) { title = t; }
void DefaultMenu::setSubtitle(const std::string& s) { subtitle = s; }

void DefaultMenu::close() { running = false; }

int DefaultMenu::itemHeight() const {
    return font ? font->textHeight() : 8;
}

int DefaultMenu::wrappedLines(const std::string& text, int maxW) const {
    if (!font || maxW <= 0) return 1;
    int lines = 1, lineW = 0;
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '\n') { lines++; lineW = 0; i++; continue; }
        if (text[i] == ' ') {
            if (lineW > 0) {
                int sa = font->spaceAdvance();
                if (lineW + sa > maxW) {
                    lines++;
                    lineW = 0;
                } else {
                    lineW += sa;
                }
            }
            i++;
            continue;
        }
        size_t wordEnd = i;
        while (wordEnd < text.size() && text[wordEnd] != ' ' && text[wordEnd] != '\n')
            wordEnd++;
        std::string word = text.substr(i, wordEnd - i);
        int wordW = font->textWidth(word);
        if (wordW > maxW) {
            size_t pos = 0;
            while (pos < word.size()) {
                size_t end = pos;
                int chunkW = 0;
                while (end < word.size()) {
                    char ch = word[end];
                    int add = font->glyph(ch).actualWidth + font->charGap();
                    if (chunkW + add > maxW && end > pos) break;
                    chunkW += add;
                    end++;
                }
                if (end == pos) end = pos + 1;
                pos = end;
                if (pos < word.size()) lines++;
            }
            lineW = 0;
        } else if (lineW > 0 && lineW + font->spaceAdvance() + wordW > maxW) {
            lines++;
            lineW = wordW;
        } else {
            if (lineW > 0) lineW += font->spaceAdvance();
            lineW += wordW;
        }
        i = wordEnd;
    }
    return lines;
}

int DefaultMenu::contentHeight() const {
    if (!font) return 0;
    int vw = renderer->width();
    int maxW = vw - margin * 2;
    int fh = font->textHeight();
    int lineH = fh;

    int titleLines = std::max(1, wrappedLines(title, maxW));
    int subLines = std::max(1, wrappedLines(subtitle, maxW));
    int titleH = titleLines * lineH;
    int subH = subLines * lineH;

    int totalBtnH = 0;
    for (auto& item : items) {
        int l = wrappedLines(item.labelStr(), maxW - 4);
        int bh = l * itemHeight() + 2;
        totalBtnH += bh + itemSpacing;
    }
    if (totalBtnH > 0) totalBtnH -= itemSpacing;

    return margin + titleH + font->topGap() + subH + font->topGap() + margin + totalBtnH + margin;
}

void DefaultMenu::centerOnSelected() {
    if (!font || items.empty()) return;
    int vw = renderer->width();
    int maxW = vw - margin * 2;
    int lineH = font->textHeight();
    int titleLines = std::max(1, wrappedLines(title, maxW));
    int subLines = std::max(1, wrappedLines(subtitle, maxW));

    int selY = margin + titleLines * lineH + font->topGap() + subLines * lineH + font->topGap() + margin;
    for (int i = 0; i < selected && i < (int)items.size(); i++) {
        int l = wrappedLines(items[i].labelStr(), maxW - 4);
        int bh = l * itemHeight() + 2;
        selY += bh + itemSpacing;
    }

    int l = wrappedLines(items[selected].labelStr(), maxW - 4);
    int selH = l * itemHeight() + 2;
    int vh = renderer->height();
    scrollOffset = selY - (vh - selH) / 2;
    clampScroll();
}

void DefaultMenu::clampScroll() {
    int maxScroll = std::max(0, contentHeight() - renderer->height());
    if (scrollOffset > maxScroll) scrollOffset = maxScroll;
    if (scrollOffset < 0) scrollOffset = 0;
}

int DefaultMenu::run() {
    running = true;
    int result = -1;
    auto frameDuration = std::chrono::microseconds(1000000 / 60);

    while (running && appRunning) {
        auto frameStart = std::chrono::steady_clock::now();

        input->poll();
        draw();
        if (handleInput())
            break;

        auto elapsed = std::chrono::steady_clock::now() - frameStart;
        auto sleepFor = frameDuration - std::chrono::duration_cast<std::chrono::microseconds>(elapsed);
        if (sleepFor.count() > 0)
            std::this_thread::sleep_for(sleepFor);
    }
    running = false;
    return result;
}

bool DefaultMenu::handleInput() {
    int n = (int)items.size();
    if (n == 0) return false;

    if (input->keyPress(Key::Up) || input->keyPress(Key::W)) {
        selected = (selected - 1 + n) % n;
        centerOnSelected();
    }

    if (input->keyPress(Key::Down) || input->keyPress(Key::S)) {
        selected = (selected + 1) % n;
        centerOnSelected();
    }

    if (input->keyPress(Key::Enter) || input->keyPress(Key::Space)) {
        if (selected >= 0 && selected < n && items[selected].enabled) {
            items[selected].action();
            if (!running) return true;
        }
    }

    if (input->keyPress(Key::Escape)) {
        running = false;
        return true;
    }

    return false;
}

void DefaultMenu::drawButton(int x, int y, int w, int h, const std::string& label, bool hover) {
    int mc = renderer->getMonoColors();
    bool lowMono = mc == 2 || mc == 3;
    int fillMode = 0, textMode = 0, borderMode = 0;
    if (mc == 2) {
        fillMode = theme ? theme->getInt("mono2_fill", 2) : 2;
        textMode = theme ? theme->getInt("mono2_text", 1) : 1;
        borderMode = theme ? theme->getInt("mono2_border", 1) : 1;
    } else if (mc == 3) {
        fillMode = theme ? theme->getInt("mono3_fill", 2) : 2;
        textMode = theme ? theme->getInt("mono3_text", 1) : 1;
        borderMode = theme ? theme->getInt("mono3_border", 1) : 1;
    }
    if (mc == 3 && hover)
        fillMode = 0;
    Color bg = hover ? theme->get("button_hover", {80, 80, 80}) : theme->get("button", {55, 55, 55});
    Color border = hover ? theme->get("accent", COLOR_WHITE) : theme->get("border", {120, 120, 120});

    renderer->fillRect(x, y, w, h, bg, fillMode);

    if (hover) {
        renderer->drawRect(x, y, w, h, border, borderMode);
        renderer->fillRect(x + 1, y + 1, w - 2, h - 2, {70, 70, 70}, fillMode);
        drawText(x + 2, y + 1, label, border, w - 4, textMode);
    } else {
        if (mc == 3 || !lowMono)
            renderer->drawRect(x, y, w, h, border, 0);
        drawText(x + 2, y + 1, label, theme->get("text", COLOR_WHITE), w - 4, textMode);
    }
}

void DefaultMenu::drawText(int x, int y, const std::string& text, Color c, int maxW, int monoMode) {
    if (!font) return;
    renderer->text(x, y, text, c, std::max(1, maxW), WrapMode::Word, false, monoMode);
}

void DefaultMenu::draw() {
    int mc = renderer->getMonoColors();
    int fillMode = 0, textMode = 0;
    if (mc == 2) {
        fillMode = theme ? theme->getInt("mono2_fill", 2) : 2;
        textMode = theme ? theme->getInt("mono2_text", 1) : 1;
    } else if (mc == 3) {
        fillMode = theme ? theme->getInt("mono3_fill", 2) : 2;
        textMode = theme ? theme->getInt("mono3_text", 1) : 1;
    }
 
    renderer->beginFrame();
    renderer->clear(theme->get("background", {17, 17, 17}), fillMode);

    int vw = renderer->width();
    int vh = renderer->height();
    int fh = font ? font->textHeight() : 8;
    int lineH = fh;
    int maxW = vw - margin * 2;

    if (font) {
        clampScroll();
        int scrollY = -scrollOffset;

        int titleLines = std::max(1, wrappedLines(title, maxW));
        int subLines = std::max(1, wrappedLines(subtitle, maxW));

        int titleY = margin + scrollY;
        if (titleY + titleLines * lineH > 0 && titleY < vh)
            renderer->text(margin, titleY, title,
                           theme->get("primary", {68, 136, 255}),
                           maxW, WrapMode::Word, true, textMode);

        int subY = titleY + titleLines * lineH + font->topGap();
        if (subY + subLines * lineH > 0 && subY < vh)
            renderer->text(margin, subY, subtitle,
                           theme->get("text_dim", {136, 136, 136}),
                           maxW, WrapMode::Word, true, textMode);

        int btnW = maxW;
        int btnX = (vw - btnW) / 2;
        int by = subY + subLines * lineH + font->topGap() + margin;

        for (int i = 0; i < (int)items.size(); i++) {
            int lines = wrappedLines(items[i].labelStr(), btnW - 4);
            int bh = lines * itemHeight() + 2;
            if (by + bh > 0 && by < vh)
                drawButton(btnX, by, btnW, bh, items[i].labelStr(), i == selected);
            by += bh + itemSpacing;
        }
    }

    renderer->endFrame();
}
