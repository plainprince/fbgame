#pragma once
#include <string>
#include <functional>
extern "C" {
#include <lua5.4/lua.h>
#include <lua5.4/lauxlib.h>
#include <lua5.4/lualib.h>
}
#include <render.hpp>
#include <input.hpp>
#include <audio.hpp>
#include <save.hpp>
#include <font.hpp>
#include <theme.hpp>
#include <thread>
#include <atomic>

struct LuaGameState {
    Renderer2D* renderer;
    InputManager* input;
    AudioManager* audio;
    SaveManager* save;
    Font* font;
    Theme* theme;
    std::string gamePath;
    std::string namespace_;
    std::atomic<bool> running{true};
    std::function<void()> onShutdown;
};

class LuaBridge {
public:
    LuaBridge();
    ~LuaBridge();

    bool load(LuaGameState* state, const std::string& mainScript);
    void callSetup();
    bool callLoop(float dt);
    void callShutdown();
    void close();

private:
    static int luaRenderPixel(lua_State* L);
    static int luaFillRect(lua_State* L);
    static int luaDrawRect(lua_State* L);
    static int luaFillCircle(lua_State* L);
    static int luaDrawCircle(lua_State* L);
    static int luaLine(lua_State* L);
    static int luaText(lua_State* L);
    static int luaSprite(lua_State* L);
    static int luaClear(lua_State* L);
    static int luaMapColor(lua_State* L);
    static int luaMapColorRange(lua_State* L);
    static int luaGetWidth(lua_State* L);
    static int luaGetHeight(lua_State* L);
    static int luaSetFPS(lua_State* L);
    static int luaSetOrientation(lua_State* L);
    static int luaKeyHeld(lua_State* L);
    static int luaKeyPress(lua_State* L);
    static int luaSaveRead(lua_State* L);
    static int luaSaveWrite(lua_State* L);
    static int luaAudioPlay(lua_State* L);
    static int luaAudioPlaySfx(lua_State* L);
    static int luaAudioPlayMusic(lua_State* L);
    static int luaAudioStopMusic(lua_State* L);
    static int luaAudioStopAll(lua_State* L);
    static int luaAudioPauseAll(lua_State* L);
    static int luaAudioResumeAll(lua_State* L);
    static int luaAudioSetVolume(lua_State* L);
    static int luaAudioGetVolume(lua_State* L);
    static int luaThemeColor(lua_State* L);
    static int luaColor(lua_State* L);
    static int luaShutdown(lua_State* L);
    static int luaYield(lua_State* L);
    static int luaMenuCreate(lua_State* L);
    static int luaMenuTick(lua_State* L);

    void registerFuncs(lua_State* L);

    lua_State* L;
    LuaGameState* gs;
    int loopThreadRef{LUA_REFNIL};
    bool firstLoop{true};
};
