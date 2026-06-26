#include <lua_bridge.hpp>
#include <fstream>
#include <iostream>
#include <sstream>

LuaBridge::LuaBridge() : L(nullptr), gs(nullptr) {}
LuaBridge::~LuaBridge() { close(); }

using GameState = LuaGameState;

static GameState* getGS(lua_State* s) {
    lua_getglobal(s, "__gs");
    GameState* gs = (GameState*)lua_touserdata(s, -1);
    lua_pop(s, 1);
    return gs;
}

int LuaBridge::luaRenderPixel(lua_State* S) {
    auto* g = getGS(S);
    int x = lua_tointeger(S, 1);
    int y = lua_tointeger(S, 2);
    Color c = Color::unpack((uint32_t)lua_tointeger(S, 3));
    int monoMode = lua_isnone(S, 4) ? 0 : lua_tointeger(S, 4);
    g->renderer->pixel(x, y, c, monoMode);
    return 0;
}

int LuaBridge::luaFillRect(lua_State* S) {
    auto* g = getGS(S);
    int x = lua_tointeger(S, 1), y = lua_tointeger(S, 2);
    int w = lua_tointeger(S, 3), h = lua_tointeger(S, 4);
    Color c = Color::unpack((uint32_t)lua_tointeger(S, 5));
    int monoMode = lua_isnone(S, 6) ? 0 : lua_tointeger(S, 6);
    g->renderer->fillRect(x, y, w, h, c, monoMode);
    return 0;
}

int LuaBridge::luaDrawRect(lua_State* S) {
    auto* g = getGS(S);
    int x = lua_tointeger(S, 1), y = lua_tointeger(S, 2);
    int w = lua_tointeger(S, 3), h = lua_tointeger(S, 4);
    Color c = Color::unpack((uint32_t)lua_tointeger(S, 5));
    int monoMode = lua_isnone(S, 6) ? 0 : lua_tointeger(S, 6);
    g->renderer->drawRect(x, y, w, h, c, monoMode);
    return 0;
}

int LuaBridge::luaFillCircle(lua_State* S) {
    auto* g = getGS(S);
    int cx = lua_tointeger(S, 1), cy = lua_tointeger(S, 2), r = lua_tointeger(S, 3);
    Color c = Color::unpack((uint32_t)lua_tointeger(S, 4));
    int monoMode = lua_isnone(S, 5) ? 0 : lua_tointeger(S, 5);
    g->renderer->fillCircle(cx, cy, r, c, monoMode);
    return 0;
}

int LuaBridge::luaDrawCircle(lua_State* S) {
    auto* g = getGS(S);
    int cx = lua_tointeger(S, 1), cy = lua_tointeger(S, 2), r = lua_tointeger(S, 3);
    Color c = Color::unpack((uint32_t)lua_tointeger(S, 4));
    int monoMode = lua_isnone(S, 5) ? 0 : lua_tointeger(S, 5);
    g->renderer->drawCircle(cx, cy, r, c, monoMode);
    return 0;
}

int LuaBridge::luaLine(lua_State* S) {
    auto* g = getGS(S);
    int x1 = lua_tointeger(S, 1), y1 = lua_tointeger(S, 2);
    int x2 = lua_tointeger(S, 3), y2 = lua_tointeger(S, 4);
    Color c = Color::unpack((uint32_t)lua_tointeger(S, 5));
    int monoMode = lua_isnone(S, 6) ? 0 : lua_tointeger(S, 6);
    g->renderer->line(x1, y1, x2, y2, c, monoMode);
    return 0;
}

int LuaBridge::luaText(lua_State* S) {
    auto* g = getGS(S);
    int x = lua_tointeger(S, 1), y = lua_tointeger(S, 2);
    const char* t = lua_tostring(S, 3);
    Color c = Color::unpack((uint32_t)lua_tointeger(S, 4));
    int maxW = lua_isnone(S, 5) ? 0 : lua_tointeger(S, 5);
    WrapMode mode = WrapMode::None;
    if (!lua_isnone(S, 6)) {
        int m = lua_tointeger(S, 6);
        if (m == 1) mode = WrapMode::Word;
        else if (m == 2) mode = WrapMode::Char;
    }
    bool centered = lua_isnone(S, 7) ? false : lua_toboolean(S, 7);
    int monoMode = lua_isnone(S, 8) ? 0 : lua_tointeger(S, 8);
    g->renderer->text(x, y, t ? t : "", c, maxW, mode, centered, monoMode);
    return 0;
}

int LuaBridge::luaSprite(lua_State* S) {
    auto* g = getGS(S);
    int x = lua_tointeger(S, 1), y = lua_tointeger(S, 2);
    int monoMode = lua_isnone(S, 4) ? 0 : lua_tointeger(S, 4);
    SpriteLoader loader;
    Sprite spr;
    if (loader.load(lua_tostring(S, 3), spr))
        g->renderer->sprite(x, y, spr, monoMode);
    return 0;
}

int LuaBridge::luaClear(lua_State* S) {
    auto* g = getGS(S);
    Color c = Color::unpack((uint32_t)lua_tointeger(S, 1));
    int monoMode = lua_isnone(S, 2) ? 0 : lua_tointeger(S, 2);
    g->renderer->clear(c, monoMode);
    return 0;
}

int LuaBridge::luaMapColor(lua_State* S) {
    auto* g = getGS(S);
    uint32_t src = (uint32_t)lua_tointeger(S, 1);
    int gray = lua_tointeger(S, 2);
    if (gray < 0) gray = 0;
    if (gray > 255) gray = 255;
    g->renderer->mapColor(src, (uint8_t)gray);
    return 0;
}

int LuaBridge::luaMapColorRange(lua_State* S) {
    auto* g = getGS(S);
    int rMin = lua_tointeger(S, 1), rMax = lua_tointeger(S, 2);
    int gMin = lua_tointeger(S, 3), gMax = lua_tointeger(S, 4);
    int bMin = lua_tointeger(S, 5), bMax = lua_tointeger(S, 6);
    int gray = lua_tointeger(S, 7);
    auto clamp = [](int v) -> uint8_t { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); };
    g->renderer->mapColorRange(clamp(rMin), clamp(rMax), clamp(gMin), clamp(gMax), clamp(bMin), clamp(bMax), clamp(gray));
    return 0;
}

int LuaBridge::luaGetWidth(lua_State* S) {
    auto* g = getGS(S);
    lua_pushinteger(S, g->renderer->width());
    return 1;
}

int LuaBridge::luaGetHeight(lua_State* S) {
    auto* g = getGS(S);
    lua_pushinteger(S, g->renderer->height());
    return 1;
}

int LuaBridge::luaSetFPS(lua_State* S) {
    auto* g = getGS(S);
    g->renderer->setFPS(lua_tointeger(S, 1));
    return 0;
}

int LuaBridge::luaSetOrientation(lua_State* S) {
    auto* g = getGS(S);
    const char* o = lua_tostring(S, 1);
    if (o && o[0] == 'v') g->renderer->setOrientation(Orientation::Vertical);
    else g->renderer->setOrientation(Orientation::Horizontal);
    return 0;
}

int LuaBridge::luaKeyHeld(lua_State* S) {
    auto* g = getGS(S);
    const char* k = lua_tostring(S, 1);
    if (!k) { lua_pushboolean(S, false); return 1; }

    static const std::unordered_map<std::string, Key> keyMap = {
        {"KEY_A", Key::A}, {"KEY_B", Key::B}, {"KEY_C", Key::C},
        {"KEY_D", Key::D}, {"KEY_E", Key::E}, {"KEY_F", Key::F},
        {"KEY_G", Key::G}, {"KEY_H", Key::H}, {"KEY_I", Key::I},
        {"KEY_J", Key::J}, {"KEY_K", Key::K}, {"KEY_L", Key::L},
        {"KEY_M", Key::M}, {"KEY_N", Key::N}, {"KEY_O", Key::O},
        {"KEY_P", Key::P}, {"KEY_Q", Key::Q}, {"KEY_R", Key::R},
        {"KEY_S", Key::S}, {"KEY_T", Key::T}, {"KEY_U", Key::U},
        {"KEY_V", Key::V}, {"KEY_W", Key::W}, {"KEY_X", Key::X},
        {"KEY_Y", Key::Y}, {"KEY_Z", Key::Z},
        {"KEY_SPACE", Key::Space}, {"KEY_ENTER", Key::Enter},
        {"KEY_ESCAPE", Key::Escape}, {"KEY_UP", Key::Up},
        {"KEY_DOWN", Key::Down}, {"KEY_LEFT", Key::Left},
        {"KEY_RIGHT", Key::Right},
        {"BUTTON_A", Key::BUTTON_A}, {"BUTTON_B", Key::BUTTON_B},
        {"BUTTON_X", Key::BUTTON_X}, {"BUTTON_Y", Key::BUTTON_Y},
        {"BUTTON_START", Key::BUTTON_START}, {"BUTTON_SELECT", Key::BUTTON_SELECT},
    };

    auto it = keyMap.find(k);
    if (it != keyMap.end()) {
        lua_pushboolean(S, g->input->keyHeld(it->second));
    } else {
        lua_pushboolean(S, false);
    }
    return 1;
}

int LuaBridge::luaKeyPress(lua_State* S) {
    auto* g = getGS(S);
    const char* k = lua_tostring(S, 1);
    if (!k) { lua_pushboolean(S, false); return 1; }

    static const std::unordered_map<std::string, Key> keyMap = {
        {"KEY_A", Key::A}, {"KEY_B", Key::B}, {"KEY_C", Key::C},
        {"KEY_D", Key::D}, {"KEY_E", Key::E}, {"KEY_F", Key::F},
        {"KEY_G", Key::G}, {"KEY_H", Key::H}, {"KEY_I", Key::I},
        {"KEY_J", Key::J}, {"KEY_K", Key::K}, {"KEY_L", Key::L},
        {"KEY_M", Key::M}, {"KEY_N", Key::N}, {"KEY_O", Key::O},
        {"KEY_P", Key::P}, {"KEY_Q", Key::Q}, {"KEY_R", Key::R},
        {"KEY_S", Key::S}, {"KEY_T", Key::T}, {"KEY_U", Key::U},
        {"KEY_V", Key::V}, {"KEY_W", Key::W}, {"KEY_X", Key::X},
        {"KEY_Y", Key::Y}, {"KEY_Z", Key::Z},
        {"KEY_SPACE", Key::Space}, {"KEY_ENTER", Key::Enter},
        {"KEY_ESCAPE", Key::Escape},
        {"KEY_UP", Key::Up}, {"KEY_DOWN", Key::Down},
        {"KEY_LEFT", Key::Left}, {"KEY_RIGHT", Key::Right},
        {"BUTTON_A", Key::BUTTON_A}, {"BUTTON_B", Key::BUTTON_B},
        {"BUTTON_X", Key::BUTTON_X}, {"BUTTON_Y", Key::BUTTON_Y},
        {"BUTTON_START", Key::BUTTON_START}, {"BUTTON_SELECT", Key::BUTTON_SELECT},
    };

    auto it = keyMap.find(k);
    if (it != keyMap.end()) {
        lua_pushboolean(S, g->input->keyPress(it->second));
    } else {
        lua_pushboolean(S, false);
    }
    return 1;
}

int LuaBridge::luaSaveRead(lua_State* S) {
    auto* g = getGS(S);
    const char* name = lua_tostring(S, 1);
    if (!name) { lua_pushstring(S, ""); return 1; }
    std::string data = g->save->read(g->namespace_ + "/" + name);
    lua_pushstring(S, data.c_str());
    return 1;
}

int LuaBridge::luaSaveWrite(lua_State* S) {
    auto* g = getGS(S);
    const char* name = lua_tostring(S, 1);
    const char* data = lua_tostring(S, 2);
    if (name && data)
        g->save->writeAsync(g->namespace_ + "/" + name, data);
    return 0;
}

int LuaBridge::luaAudioPlay(lua_State* S) {
    auto* g = getGS(S);
    const char* path = lua_tostring(S, 1);
    bool loop = false;
    if (!lua_isnone(S, 2)) loop = lua_toboolean(S, 2);
    if (!path) return 0;
    std::string full = g->gamePath + "/" + path;
    if (loop)
        g->audio->playMusic(full);
    else
        g->audio->playSfx(full);
    return 0;
}

int LuaBridge::luaAudioPlaySfx(lua_State* S) {
    auto* g = getGS(S);
    const char* path = luaL_checkstring(S, 1);
    g->audio->playSfx(g->gamePath + "/" + path);
    return 0;
}

int LuaBridge::luaAudioPlayMusic(lua_State* S) {
    auto* g = getGS(S);
    const char* path = luaL_checkstring(S, 1);
    g->audio->playMusic(g->gamePath + "/" + path);
    return 0;
}

int LuaBridge::luaAudioStopMusic(lua_State* S) {
    auto* g = getGS(S);
    g->audio->stopMusic();
    return 0;
}

int LuaBridge::luaAudioStopAll(lua_State* S) {
    auto* g = getGS(S);
    g->audio->stopAll();
    return 0;
}

int LuaBridge::luaAudioPauseAll(lua_State* S) {
    auto* g = getGS(S);
    g->audio->pauseAll();
    return 0;
}

int LuaBridge::luaAudioResumeAll(lua_State* S) {
    auto* g = getGS(S);
    g->audio->resumeAll();
    return 0;
}

int LuaBridge::luaAudioSetVolume(lua_State* S) {
    auto* g = getGS(S);
    float vol = lua_tonumber(S, 1);
    g->audio->setVolume(vol);
    return 0;
}

int LuaBridge::luaAudioGetVolume(lua_State* S) {
    auto* g = getGS(S);
    lua_pushnumber(S, g->audio->getVolume());
    return 1;
}

int LuaBridge::luaThemeColor(lua_State* S) {
    auto* g = getGS(S);
    const char* name = lua_tostring(S, 1);
    if (!name) { lua_pushinteger(S, COLOR_WHITE.pack()); return 1; }
    Color c = g->renderer->themeColor(name);
    lua_pushinteger(S, c.pack());
    return 1;
}

int LuaBridge::luaColor(lua_State* S) {
    int r = lua_tointeger(S, 1);
    int g = lua_tointeger(S, 2);
    int b = lua_tointeger(S, 3);
    int a = lua_isnone(S, 4) ? 255 : lua_tointeger(S, 4);
    lua_pushinteger(S, Color((uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a).pack());
    return 1;
}

int LuaBridge::luaShutdown(lua_State* S) {
    auto* g = getGS(S);
    g->running = false;
    if (g->onShutdown) g->onShutdown();
    return 0;
}

int LuaBridge::luaYield(lua_State* S) {
    return lua_yield(S, 0);
}

int LuaBridge::luaMenuCreate(lua_State* S) {
    const char* title = luaL_checkstring(S, 1);
    luaL_checktype(S, 2, LUA_TTABLE);

    lua_newtable(S);
    lua_pushstring(S, title);
    lua_setfield(S, -2, "title");
    lua_pushvalue(S, 2);
    lua_setfield(S, -2, "items");
    lua_pushinteger(S, 1);
    lua_setfield(S, -2, "selected");
    lua_setglobal(S, "_menu_state");
    return 0;
}

int LuaBridge::luaMenuTick(lua_State* S) {
    auto* g = getGS(S);
    if (!g || !g->input) return 0;

    lua_getglobal(S, "_menu_state");
    if (lua_isnil(S, -1)) { lua_pushinteger(S, -1); return 1; }

    lua_getfield(S, -1, "selected");
    int sel = lua_tointeger(S, -1);
    lua_pop(S, 1);

    lua_getfield(S, -1, "items");
    int n = lua_rawlen(S, -1);
    if (n == 0) { lua_pushinteger(S, -1); return 1; }

    if (g->input->keyPress(Key::Up) || g->input->keyPress(Key::W)) {
        sel = (sel - 2 + n) % n + 1;
    }
    if (g->input->keyPress(Key::Down) || g->input->keyPress(Key::S)) {
        sel = sel % n + 1;
    }
    if (g->input->keyPress(Key::Enter) || g->input->keyPress(Key::Space)) {
        lua_pushinteger(S, sel);
        return 1;
    }
    if (g->input->keyPress(Key::Escape)) {
        lua_pushinteger(S, -1);
        return 1;
    }

    lua_pushinteger(S, sel);
    lua_setfield(S, -3, "selected");

    lua_getfield(S, -2, "title");
    const char* title = lua_tostring(S, -1);
    lua_pop(S, 1);

    int vw = g->renderer->width();
    int vh = g->renderer->height();
    int fh = g->font ? g->font->textHeight() : 8;
    int margin = 4;
    int lineH = fh;
    int topGap = g->font ? g->font->topGap() : 0;

    int btnW = vw - margin * 2;
    int spacing = 2;
    int titleAreaH = margin + lineH + topGap + margin;

    auto btnLines = [&](const char* s) -> int {
        if (!g->font || !s || btnW - 4 <= 0) return 1;
        int maxW = btnW - 4;
        int lines = 1, lineW = 0;
        std::string text(s);
        size_t i = 0;
        while (i < text.size()) {
            if (text[i] == '\n') { lines++; lineW = 0; i++; continue; }
            if (text[i] == ' ') {
                if (lineW > 0) {
                    int sa = g->font->spaceAdvance();
                    if (lineW + sa > maxW) { lines++; lineW = 0; }
                    else lineW += sa;
                }
                i++; continue;
            }
            size_t wordEnd = i;
            while (wordEnd < text.size() && text[wordEnd] != ' ' && text[wordEnd] != '\n')
                wordEnd++;
            std::string word = text.substr(i, wordEnd - i);
            int wordW = g->font->textWidth(word);
            if (wordW > maxW) {
                size_t pos = 0;
                while (pos < word.size()) {
                    size_t end = pos;
                    int chunkW = 0;
                    while (end < word.size()) {
                        char ch = word[end];
                        int add = g->font->glyph(ch).actualWidth + g->font->charGap();
                        if (chunkW + add > maxW && end > pos) break;
                        chunkW += add;
                        end++;
                    }
                    if (end == pos) end = pos + 1;
                    pos = end;
                    if (pos < word.size()) lines++;
                }
                lineW = 0;
            } else if (lineW > 0 && lineW + g->font->spaceAdvance() + wordW > maxW) {
                lines++;
                lineW = wordW;
            } else {
                if (lineW > 0) lineW += g->font->spaceAdvance();
                lineW += wordW;
            }
            i = wordEnd;
        }
        return lines;
    };

    std::vector<int> btnHeights;
    int totalBtnH = 0;
    for (int i = 1; i <= n; i++) {
        lua_rawgeti(S, -1, i);
        const char* label = lua_tostring(S, -1);
        int l = label ? btnLines(label) : 1;
        int bh = l * fh + 2;
        btnHeights.push_back(bh);
        totalBtnH += bh + spacing;
        lua_pop(S, 1);
    }
    if (totalBtnH > 0) totalBtnH -= spacing;
    int contentEnd = titleAreaH + totalBtnH + margin;

    int scrollOffset = 0;
    if (contentEnd > vh) {
        int selY = titleAreaH;
        for (int i = 0; i < sel - 1 && i < (int)btnHeights.size(); i++)
            selY += btnHeights[i] + spacing;
        int selH = btnHeights[sel - 1];
        scrollOffset = selY - (vh - selH) / 2;
        if (scrollOffset < 0) scrollOffset = 0;
        int maxScroll = contentEnd - vh;
        if (scrollOffset > maxScroll) scrollOffset = maxScroll;
    }

    int mc = g->renderer->getMonoColors();
    bool lowMono = mc == 2 || mc == 3;
    int fillMode = 0, textMode = 0, borderMode = 0;
    if (mc == 2) {
        fillMode = g->theme ? g->theme->getInt("mono2_fill", 2) : 2;
        textMode = g->theme ? g->theme->getInt("mono2_text", 1) : 1;
        borderMode = g->theme ? g->theme->getInt("mono2_border", 1) : 1;
    } else if (mc == 3) {
        fillMode = g->theme ? g->theme->getInt("mono3_fill", 2) : 2;
        textMode = g->theme ? g->theme->getInt("mono3_text", 1) : 1;
        borderMode = g->theme ? g->theme->getInt("mono3_border", 1) : 1;
    }

    Color bgCol = g->theme ? g->theme->get("background", {17, 17, 17}) : Color(17, 17, 17);
    g->renderer->clear(bgCol, fillMode);

    if (title) {
        int titleY = margin - scrollOffset;
        if (titleY + lineH > 0 && titleY < vh) {
            Color c = g->theme ? g->theme->get("primary", {68, 136, 255}) : Color(68, 136, 255);
            g->renderer->text(margin, titleY, title, c, btnW, WrapMode::Word, false, textMode);
        }
    }

    int by = titleAreaH - scrollOffset;
    for (int i = 1; i <= n; i++) {
        lua_rawgeti(S, -1, i);
        const char* label = lua_tostring(S, -1);
        int bh = btnHeights[i - 1];
        if (label) {
            if (by + bh > 0 && by < vh) {
                bool hover = (i == sel);
                Color bg = hover
                    ? (g->theme ? g->theme->get("button_hover", {80, 80, 80}) : Color(80, 80, 80))
                    : (g->theme ? g->theme->get("button", {55, 55, 55}) : Color(55, 55, 55));
                Color accent = g->theme ? g->theme->get("accent", COLOR_WHITE) : COLOR_WHITE;
                Color borderCol = g->theme ? g->theme->get("border", Color(120, 120, 120)) : Color(120, 120, 120);

                int effFill = (mc == 3 && hover) ? 0 : fillMode;
                g->renderer->fillRect(margin, by, btnW, bh, bg, effFill);

                if (hover) {
                    g->renderer->drawRect(margin, by, btnW, bh, accent, borderMode);
                    g->renderer->fillRect(margin + 1, by + 1, btnW - 2, bh - 2, Color(70, 70, 70), effFill);
                    g->renderer->text(margin + 2, by + 1, label, accent, btnW - 4, WrapMode::Word, false, textMode);
                } else {
                    if (mc == 3 || !lowMono)
                        g->renderer->drawRect(margin, by, btnW, bh, borderCol, 0);
                    g->renderer->text(margin + 2, by + 1, label,
                        g->theme ? g->theme->get("text", COLOR_WHITE) : COLOR_WHITE,
                        btnW - 4, WrapMode::Word, false, textMode);
                }
            }
            by += bh + spacing;
        }
        lua_pop(S, 1);
    }

    lua_pushinteger(S, 0);
    return 1;
}

bool LuaBridge::callLoop(float dt) {
    if (!gs->running) return false;
    if (!L) { std::cerr << "callLoop: L is null\n"; return false; }

    if (loopThreadRef == LUA_REFNIL) {
        lua_newthread(L);
        if (lua_isnil(L, -1)) { std::cerr << "callLoop: newthread returned nil\n"; lua_pop(L, 1); return false; }
        loopThreadRef = luaL_ref(L, LUA_REGISTRYINDEX);
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, loopThreadRef);
    lua_State* thread = lua_tothread(L, -1);
    lua_pop(L, 1);
    if (!thread) { std::cerr << "callLoop: thread is null\n"; return false; }

    int nin;
    if (firstLoop) {
        lua_getglobal(thread, "loop");
        if (lua_isnil(thread, -1)) {
            std::cerr << "callLoop: 'loop' function not found\n";
            lua_pop(thread, 1);
            return false;
        }
        lua_pushnumber(thread, dt);
        int status = lua_resume(thread, L, 1, &nin);
        firstLoop = false;
        if (status == LUA_YIELD) return gs->running;
        if (status != LUA_OK) {
            std::cerr << "loop() error: " << lua_tostring(thread, -1) << "\n";
            lua_pop(thread, 1);
        }
        luaL_unref(L, LUA_REGISTRYINDEX, loopThreadRef);
        loopThreadRef = LUA_REFNIL;
        firstLoop = true;
        return gs->running;
    }

    lua_pushnumber(thread, dt);
    int status = lua_resume(thread, L, 1, &nin);

    if (status == LUA_YIELD) return gs->running;

    if (status != LUA_OK) {
        std::cerr << "loop() resume error: " << lua_tostring(thread, -1) << "\n";
        lua_pop(thread, 1);
    }

    luaL_unref(L, LUA_REGISTRYINDEX, loopThreadRef);
    loopThreadRef = LUA_REFNIL;
    firstLoop = true;
    return gs->running;
}

void LuaBridge::registerFuncs(lua_State* S) {
    static const luaL_Reg funcs[] = {
        {"pixel", luaRenderPixel},
        {"fillRect", luaFillRect},
        {"drawRect", luaDrawRect},
        {"fillCircle", luaFillCircle},
        {"drawCircle", luaDrawCircle},
        {"line", luaLine},
        {"text", luaText},
        {"sprite", luaSprite},
        {"clear", luaClear},
        {"mapColor", luaMapColor},
        {"mapColorRange", luaMapColorRange},
        {"getWidth", luaGetWidth},
        {"getHeight", luaGetHeight},
        {"setFPS", luaSetFPS},
        {"setOrientation", luaSetOrientation},
        {"keyHeld", luaKeyHeld},
        {"keyPress", luaKeyPress},
        {"saveRead", luaSaveRead},
        {"saveWrite", luaSaveWrite},
        {"audioPlay", luaAudioPlay},
        {"themeColor", luaThemeColor},
        {"color", luaColor},
        {"shutdown", luaShutdown},
        {nullptr, nullptr}
    };

    lua_newtable(S);
    luaL_setfuncs(S, funcs, 0);
    lua_setglobal(S, "render");

    lua_newtable(S);
    lua_pushcfunction(S, luaKeyHeld); lua_setfield(S, -2, "keyHeld");
    lua_pushcfunction(S, luaKeyPress); lua_setfield(S, -2, "keyPress");
    lua_setglobal(S, "input");

    lua_newtable(S);
    lua_pushcfunction(S, luaSaveRead); lua_setfield(S, -2, "read");
    lua_pushcfunction(S, luaSaveWrite); lua_setfield(S, -2, "write");
    lua_setglobal(S, "save");

    lua_newtable(S);
    lua_pushcfunction(S, luaAudioPlay); lua_setfield(S, -2, "play");
    lua_pushcfunction(S, luaAudioPlaySfx); lua_setfield(S, -2, "playSfx");
    lua_pushcfunction(S, luaAudioPlayMusic); lua_setfield(S, -2, "playMusic");
    lua_pushcfunction(S, luaAudioStopMusic); lua_setfield(S, -2, "stopMusic");
    lua_pushcfunction(S, luaAudioStopAll); lua_setfield(S, -2, "stopAll");
    lua_pushcfunction(S, luaAudioPauseAll); lua_setfield(S, -2, "pauseAll");
    lua_pushcfunction(S, luaAudioResumeAll); lua_setfield(S, -2, "resumeAll");
    lua_pushcfunction(S, luaAudioSetVolume); lua_setfield(S, -2, "setVolume");
    lua_pushcfunction(S, luaAudioGetVolume); lua_setfield(S, -2, "getVolume");
    lua_setglobal(S, "audio");

    lua_newtable(S);
    lua_pushcfunction(S, luaMenuCreate); lua_setfield(S, -2, "create");
    lua_pushcfunction(S, luaMenuTick); lua_setfield(S, -2, "tick");
    lua_setglobal(S, "menu");

    lua_newtable(S);
    lua_pushcfunction(S, luaThemeColor); lua_setfield(S, -2, "get");
    lua_setglobal(S, "theme");

    lua_pushcfunction(S, luaColor);
    lua_setglobal(S, "Color");

    lua_pushcfunction(S, luaShutdown);
    lua_setglobal(S, "shutdown");
    lua_pushcfunction(S, luaShutdown);
    lua_setglobal(S, "_engine_exit");
    lua_pushcfunction(S, luaShutdown);
    lua_setglobal(S, "quit");

    lua_pushcfunction(S, luaYield);
    lua_setglobal(S, "yield");
}

bool LuaBridge::load(GameState* state, const std::string& mainScript) {
    gs = state;
    L = luaL_newstate();
    if (!L) return false;
    luaL_openlibs(L);

    lua_pushlightuserdata(L, gs);
    lua_setglobal(L, "__gs");

    registerFuncs(L);

    auto injectColor = [&](const char* name, const Color& c) {
        lua_pushinteger(L, c.pack());
        lua_setglobal(L, name);
    };
    injectColor("COLOR_BLACK", COLOR_BLACK);
    injectColor("COLOR_WHITE", COLOR_WHITE);
    injectColor("COLOR_RED", COLOR_RED);
    injectColor("COLOR_GREEN", COLOR_GREEN);
    injectColor("COLOR_BLUE", COLOR_BLUE);
    injectColor("COLOR_YELLOW", COLOR_YELLOW);
    injectColor("COLOR_CYAN", COLOR_CYAN);
    injectColor("COLOR_MAGENTA", COLOR_MAGENTA);
    injectColor("COLOR_ORANGE", COLOR_ORANGE);
    injectColor("COLOR_PURPLE", COLOR_PURPLE);
    injectColor("COLOR_GREY", COLOR_GREY);
    injectColor("COLOR_PRIMARY", Color(68, 136, 255));
    injectColor("COLOR_TEXT_DIM", Color(136, 136, 136));
    injectColor("COLOR_ACCENT", Color(255, 68, 136));

    if (luaL_dofile(L, mainScript.c_str()) != LUA_OK) {
        std::cerr << "Lua error: " << lua_tostring(L, -1) << "\n";
        lua_pop(L, 1);
        return false;
    }
    return true;
}

void LuaBridge::callSetup() {
    lua_getglobal(L, "setup");
    if (lua_isfunction(L, -1)) {
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            std::cerr << "setup() error: " << lua_tostring(L, -1) << "\n";
            lua_pop(L, 1);
        }
    } else {
        lua_pop(L, 1);
    }
}

void LuaBridge::callShutdown() {
    lua_getglobal(L, "shutdown");
    if (lua_isfunction(L, -1)) {
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            std::cerr << "shutdown() error: " << lua_tostring(L, -1) << "\n";
            lua_pop(L, 1);
        }
    } else {
        lua_pop(L, 1);
    }
}

void LuaBridge::close() {
    if (L) {
        lua_close(L);
        L = nullptr;
    }
}
