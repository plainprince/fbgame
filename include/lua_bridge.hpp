#pragma once
#include <string>
#include <functional>
extern "C" {
#include <lua.hpp>
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
    static int luaHash11(lua_State* L);
    static int luaBshl(lua_State* L);
    static int luaBshr(lua_State* L);
    static int luaBnot(lua_State* L);
    static int luaBand(lua_State* L);
    static int luaBor(lua_State* L);
    static int luaBxor(lua_State* L);
    static int luaClamp(lua_State* L);
    static int luaLerp(lua_State* L);
    static int luaMap(lua_State* L);
    static int luaSign(lua_State* L);
    static int luaWrap(lua_State* L);
    static int luaTablePack(lua_State* L);
    static int luaTableMerge(lua_State* L);
    static int luaTimeNow(lua_State* L);
    static int luaTimeMs(lua_State* L);
    static int luaMathClamp(lua_State* L);
    static int luaMathLerp(lua_State* L);
    static int luaMathSign(lua_State* L);
    static int luaMathRound(lua_State* L);
    static int luaMathAbsfast(lua_State* L);
    static int luaRngNew(lua_State* L);
    static int luaRngInt(lua_State* L);
    static int luaRngFloat(lua_State* L);
    static int luaStringSplit(lua_State* L);
    static int luaStringTrim(lua_State* L);
    static int luaStringStartswith(lua_State* L);
    static int luaStringEndswith(lua_State* L);
    static int luaSchedulerAfter(lua_State* L);
    static int luaRlNew(lua_State* L);
    static int luaRlAct(lua_State* L);
    static int luaRlTrain(lua_State* L);
    static int luaRlSave(lua_State* L);
    static int luaRlLoad(lua_State* L);

    void registerFuncs(lua_State* L);

    lua_State* L;
    LuaGameState* gs;
    int loopThreadRef{LUA_REFNIL};
    bool firstLoop{true};
};

inline int lua_resume_loop(lua_State* L, lua_State* from, int narg, int* nresults) {
#if LUA_VERSION_NUM >= 502
    return lua_resume(L, from, narg, nresults);
#else
    (void)from; (void)nresults;
    return lua_resume(L, narg);
#endif
}
inline void luaL_setfuncs_loop(lua_State* L, const luaL_Reg* reg, int nup) {
    luaL_setfuncs(L, reg, nup);
}
