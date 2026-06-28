#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <vector>
#include <filesystem>
#include <sched.h>
#include <unistd.h>
#include <ctime>
#include <render.hpp>
#include <input.hpp>
#include <audio.hpp>
#include <save.hpp>
#include <font.hpp>
#include <theme.hpp>
#include <lua_bridge.hpp>
#include <menu.hpp>
#include <config.hpp>

static Renderer2D* gRenderer = nullptr;
static InputManager* gInput = nullptr;
static AudioManager* gAudio = nullptr;
static SaveManager* gSave = nullptr;
static Theme* gTheme = nullptr;
static std::string gFontName = "default";

volatile std::sig_atomic_t appRunning = 1;

void signalHandler(int) {
    const char* restore = "\033[?25h";
    write(STDOUT_FILENO, restore, 6);
    appRunning = 0;
}

static void saveFontSettings(const Font& font) {
    if (!gSave) return;
    Properties p;
    std::string existing = gSave->read("settings");
    if (!existing.empty()) p.fromString(existing);
    p.setInt("char_gap", font.charGap());
    p.setInt("top_gap", font.topGap());
    p.setInt("bottom_gap", font.bottomGap());
    gSave->writeAsync("settings", p.toString());
    std::string propPath = "fonts/" + gFontName + "/font.properties";
    Properties fp;
    fp.load(propPath);
    fp.setInt("char_gap", font.charGap());
    fp.setInt("top_gap", font.topGap());
    fp.setInt("bottom_gap", font.bottomGap());
    fp.save(propPath);
}

static void saveSetting(const std::string& key, const std::string& value) {
    if (!gSave) return;
    Properties p;
    std::string existing = gSave->read("settings");
    if (!existing.empty()) p.fromString(existing);
    p.setString(key, value);
    gSave->writeAsync("settings", p.toString());
}

static void loadAllSettings(Font& font, std::string& orient, std::string& themeName) {
    if (!gSave) return;
    std::string content = gSave->read("settings");
    if (content.empty()) return;
    Properties p;
    p.fromString(content);
    font.setCharGap(p.getInt("char_gap", font.charGap()));
    font.setTopGap(p.getInt("top_gap", font.topGap()));
    font.setBottomGap(p.getInt("bottom_gap", font.bottomGap()));
    orient = p.getString("orientation", orient);
    themeName = p.getString("theme", themeName);
    if (gAudio)
        gAudio->setVolume(p.getFloat("volume", gAudio->getVolume()));
}

static void pinToCore(int core) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

struct GameEntry {
    std::string name;
    std::string namespace_;
    std::string dir;
};

static void scanGames(const std::string& gamesPath, std::vector<GameEntry>& out) {
    if (!std::filesystem::exists(gamesPath)) return;
    for (auto& entry : std::filesystem::directory_iterator(gamesPath)) {
        if (!entry.is_directory()) continue;
        std::string gameDir = entry.path().string();
        std::string mainLua = gameDir + "/main.lua";
        if (!std::filesystem::exists(mainLua)) continue;
        GameEntry ge;
        ge.name = entry.path().filename().string();
        ge.namespace_ = ge.name;
        ge.dir = gameDir;
        std::string cfgPath = gameDir + "/config.properties";
        Properties cfg;
        if (cfg.load(cfgPath)) {
            if (cfg.has("game_name")) ge.name = cfg.getString("game_name");
            if (cfg.has("game_namespace")) ge.namespace_ = cfg.getString("game_namespace");
        }
        out.push_back(ge);
    }
}

static std::vector<std::string> scanFonts() {
    std::vector<std::string> fonts;
    if (!std::filesystem::exists("fonts")) return fonts;
    for (auto& entry : std::filesystem::directory_iterator("fonts")) {
        if (!entry.is_directory()) continue;
        std::string dir = entry.path().string();
        if (std::filesystem::exists(dir + "/font.data") && std::filesystem::exists(dir + "/font.properties"))
            fonts.push_back(entry.path().filename().string());
    }
    return fonts;
}

static void loadGameFont(Font& font, const GameEntry& game) {
    std::string dataPath = game.dir + "/font.data";
    std::string propPath = game.dir + "/font.properties";
    if (!std::filesystem::exists(dataPath) || !std::filesystem::exists(propPath))
        return;
    if (!font.load(dataPath, propPath))
        std::cerr << "Font load failed for " << game.name << "\n";
}

static void runLuaGame(const GameEntry& game, Font& activeFont) {
    gInput->resetKeys();
    loadGameFont(activeFont, game);
    gRenderer->setFont(&activeFont);

    Properties cfg;
    cfg.load(game.dir + "/config.properties");
    {
    std::string gameRuntime = cfg.getString("lua_runtime", "");
    (void)gameRuntime;
    }
    gRenderer->setFPS(cfg.getInt("fps", gRenderer->getFPS()));
    std::string gameOrient = cfg.getString("orientation", "");
    if (gameOrient == "v")
        gRenderer->resize(64, 128, Orientation::Vertical);
    else if (gameOrient == "h")
        gRenderer->resize(128, 64, Orientation::Horizontal);
    std::string monoModeStr = cfg.getString("mono_mode", "luma");
    gRenderer->setMonoConversion(monoModeStr == "edge" ? 1 : 0);

    LuaGameState gs;
    gs.renderer = gRenderer;
    gs.input = gInput;
    gs.audio = gAudio;
    gs.save = gSave;
    gs.font = &activeFont;
    gs.theme = gTheme;
    gs.gamePath = game.dir;
    gs.namespace_ = game.namespace_;
    gs.running = true;

    std::filesystem::create_directories("saves/" + game.namespace_);

    LuaBridge bridge;
    if (!bridge.load(&gs, game.dir + "/main.lua")) {
        std::cerr << "Failed to load game: " << game.name << "\n";
        return;
    }

    bridge.callSetup();

    auto lastTime = std::chrono::steady_clock::now();
    int targetFPS = gRenderer->getFPS();
    auto frameDuration = std::chrono::microseconds(1000000 / targetFPS);

    while (gs.running && appRunning) {
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;

        gInput->poll();

        if (gInput->keyHeld(Key::Ctrl) && gInput->keyPress(Key::Escape))
            gs.running = false;

        if (gInput->keyPress(Key::F12)) {
            std::string ssPath = "screenshots/" + game.name + "_" + std::to_string(std::time(nullptr)) + ".ppm";
            std::filesystem::create_directories("screenshots");
            gRenderer->saveScreenshot(ssPath);
            std::cerr << "Screenshot saved: " << ssPath << "\n";
        }

        gRenderer->beginFrame();
        if (!bridge.callLoop(dt)) {
            gRenderer->endFrame();
            break;
        }
        gRenderer->endFrame();

        auto elapsed = std::chrono::steady_clock::now() - now;
        auto sleepFor = frameDuration - std::chrono::duration_cast<std::chrono::microseconds>(elapsed);
        if (sleepFor.count() > 0)
            std::this_thread::sleep_for(sleepFor);
    }

    bridge.callShutdown();
    bridge.close();
    gSave->flush();
    if (gAudio) gAudio->stopAll();

    gRenderer->beginFrame();
    gRenderer->clear({});
    gRenderer->endFrame();
    gInput->poll();
    gInput->resetKeys();
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::cout << "FBGAME ENGINE v1.0 by plainprince\n";
    std::cout << "==================================\n";

    int fbNum = 0;
    int monoColors = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] >= '0' && argv[i][1] <= '9') {
            fbNum = std::atoi(argv[i] + 1);
        } else if (argv[i][0] == '-' && argv[i][1] == 'm') {
            if (argv[i][2] >= '2' && argv[i][2] <= '9') {
                monoColors = std::atoi(argv[i] + 2);
            } else {
                monoColors = 2;
            }
        }
    }
    std::cout << "  fb=/dev/fb" << fbNum << "\n";

    pinToCore(0);

    std::vector<GameEntry> games;
    scanGames("games", games);

    Properties engineCfg;
    engineCfg.load("config.properties");
    std::string orient = engineCfg.getString("orientation", "h");
    int fps = engineCfg.getInt("fps", 30);

    Renderer2D renderer;
    gRenderer = &renderer;

    int virtW = (orient == "v") ? 64 : 128;
    int virtH = (orient == "v") ? 128 : 64;

    if (!renderer.init(virtW, virtH,
                       orient == "v" ? Orientation::Vertical : Orientation::Horizontal,
                       fbNum)) {
        std::cerr << "Renderer init failed\n";
        return 1;
    }
    renderer.setFPS(fps);
    if (monoColors) renderer.setMonoColors(monoColors);

    InputManager input;
    gInput = &input;
    if (!input.init()) {
        std::cerr << "Input init failed\n";
        renderer.cleanup();
        return 1;
    }

    AudioManager audio;
    gAudio = &audio;
    if (!audio.init())
        std::cerr << "Audio init failed, continuing without audio\n";

    SaveManager save;
    gSave = &save;
    save.init("saves");

    Font menuFont;
    gFontName = "default";
    {
        std::string content = gSave->read("settings");
        if (!content.empty()) {
            Properties p;
            p.fromString(content);
            gFontName = p.getString("font_name", gFontName);
        }
    }
    {
        std::string dp = "fonts/" + gFontName + "/font.data";
        std::string pp = "fonts/" + gFontName + "/font.properties";
        if (!menuFont.load(dp, pp)) {
            std::cerr << "Font load failed for " << gFontName << ", trying default\n";
            gFontName = "default";
            menuFont.load("fonts/default/font.data", "fonts/default/font.properties");
        }
    }
    std::string themeName = "default";
    loadAllSettings(menuFont, orient, themeName);
    {
        int newW = (orient == "v") ? 64 : 128;
        int newH = (orient == "v") ? 128 : 64;
        renderer.resize(newW, newH,
                        orient == "v" ? Orientation::Vertical : Orientation::Horizontal);
    }
    renderer.setFont(&menuFont);

    Theme theme;
    gTheme = &theme;
    if (themeName == "default") {
        theme = Theme::defaultTheme();
    } else {
        std::string themePath = "themes/" + themeName + ".theme";
        if (!theme.load(themePath)) {
            std::cerr << "Theme load failed for " << themePath << ", using default\n";
            theme = Theme::defaultTheme();
        }
    }
    renderer.setTheme(&theme);
    renderer.setMonoConversion(theme.getInt("mono_conversion", 0));

    DefaultMenu menu(&renderer, &input, &menuFont, &theme);
    menu.setTitle("FBGAME");
    menu.setSubtitle("Main Menu");

    menu.addItem("PLAY", [&]() {
        DefaultMenu playMenu(&renderer, &input, &menuFont, &theme);
        playMenu.setTitle("SELECT GAME");
        playMenu.setSubtitle("Choose a game to play");
        playMenu.addItem("BACK", [&]() { playMenu.close(); });
        int selectedIndex = -1;
        for (int gi = 0; gi < (int)games.size(); gi++) {
            playMenu.addItem(games[gi].name, [&playMenu, &selectedIndex, gi]() {
                selectedIndex = gi;
                playMenu.close();
            });
        }
        playMenu.run();
        if (selectedIndex >= 0) {
            runLuaGame(games[selectedIndex], menuFont);
            menuFont.load("fonts/" + gFontName + "/font.data", "fonts/" + gFontName + "/font.properties");
            gRenderer->setFont(&menuFont);
            if (gTheme) gRenderer->setTheme(gTheme);
            if (orient == "v")
                gRenderer->resize(64, 128, Orientation::Vertical);
            else
                gRenderer->resize(128, 64, Orientation::Horizontal);
            input.poll();
        }
    });

    menu.addItem("SETTINGS", [&]() {
        DefaultMenu sub(&renderer, &input, &menuFont, &theme);
        sub.setTitle("SETTINGS");
        sub.setSubtitle("Configure your experience");
        sub.addItem("BACK", [&]() { saveFontSettings(menuFont); sub.close(); });
        sub.addItem("ORIENT", [&orient, &renderer]() {
            if (orient == "v") { orient = "h"; renderer.resize(128, 64, Orientation::Horizontal); }
            else { orient = "v"; renderer.resize(64, 128, Orientation::Vertical); }
            saveSetting("orientation", orient);
        });
        sub.addItem("THEMES", [&]() {
            DefaultMenu themeMenu(&renderer, &input, &menuFont, &theme);
            themeMenu.setTitle("THEMES");
            themeMenu.setSubtitle("Select a theme");
            themeMenu.addItem("BACK", [&]() { themeMenu.close(); });
            if (std::filesystem::exists("themes")) {
                for (auto& entry : std::filesystem::directory_iterator("themes")) {
                    std::string p = entry.path().string();
                    if (p.size() < 6 || p.substr(p.size() - 6) != ".theme") continue;
                    std::string tname = entry.path().stem().string();
                    themeMenu.addItem(tname, [&, p, tname]() {
                        Theme t2;
                        if (t2.load(p)) {
                            theme = t2;
                            renderer.setTheme(&theme);
                            renderer.setMonoConversion(theme.getInt("mono_conversion", 0));
                            saveSetting("theme", tname);
                        }
                    });
                }
            }
            themeMenu.run();
        });
        {
            auto volLabel = std::make_shared<std::string>("VOLUME: " + std::to_string((int)(gAudio->getVolume() * 100)) + "%");
            MenuItem volItem;
            volItem.dynamicLabel = volLabel;
            volItem.action = [&, volLabel]() {
                DefaultMenu volMenu(&renderer, &input, &menuFont, &theme);
                volMenu.setTitle("VOLUME");
                volMenu.setSubtitle("Select volume level");
                volMenu.addItem("BACK", [&]() { volMenu.close(); });
                int curPct = (int)(gAudio->getVolume() * 100 + 0.5f);
                for (int pct = 0; pct <= 100; pct++) {
                    std::string label = std::to_string(pct) + "%";
                    if (pct == curPct) label = "> " + label + " <";
                    volMenu.addItem(label, [&, pct, volLabel]() {
                        float v = pct / 100.0f;
                        gAudio->setVolume(v);
                        saveSetting("volume", std::to_string(v));
                        *volLabel = "VOLUME: " + std::to_string(pct) + "%";
                        volMenu.close();
                    });
                }
                volMenu.run();
            };
            sub.addItem(volItem);
        }
        sub.addItem("FONTS", [&]() {
            DefaultMenu fontMenu(&renderer, &input, &menuFont, &theme);
            fontMenu.setTitle("FONTS");
            fontMenu.setSubtitle("Letter and line spacing");
            fontMenu.addItem("BACK", [&]() { saveFontSettings(menuFont); fontMenu.close(); });
            fontMenu.addItem("SELECT FONT [" + gFontName + "]", [&]() {
                auto available = scanFonts();
                DefaultMenu selMenu(&renderer, &input, &menuFont, &theme);
                selMenu.setTitle("SELECT FONT");
                selMenu.setSubtitle("Choose display font");
                selMenu.addItem("BACK", [&]() { selMenu.close(); });
                for (auto& f : available) {
                    std::string label = (f == gFontName) ? "* " + f : "  " + f;
                    selMenu.addItem(label, [&, f]() {
                        std::string dp = "fonts/" + f + "/font.data";
                        std::string pp = "fonts/" + f + "/font.properties";
                        Font newFont;
                        if (newFont.load(dp, pp)) {
                            gFontName = f;
                            {
                                Properties p;
                                std::string existing = gSave->read("settings");
                                if (!existing.empty()) p.fromString(existing);
                                menuFont.setCharGap(p.getInt("char_gap", newFont.charGap()));
                                menuFont.setTopGap(p.getInt("top_gap", newFont.topGap()));
                                menuFont.setBottomGap(p.getInt("bottom_gap", newFont.bottomGap()));
                                saveSetting("font_name", f);
                            }
                            menuFont = std::move(newFont);
                            gRenderer->setFont(&menuFont);
                        }
                        selMenu.close();
                    });
                }
                selMenu.run();
            });
            fontMenu.addItem("CHAR GAP: " + std::to_string(menuFont.charGap()), [&]() {
                DefaultMenu cgMenu(&renderer, &input, &menuFont, &theme);
                cgMenu.setTitle("CHARACTER GAP");
                cgMenu.setSubtitle("Space between letters");
                cgMenu.addItem("BACK", [&]() { saveFontSettings(menuFont); cgMenu.close(); });
                for (int n = 0; n <= 3; n++) {
                    cgMenu.addItem(std::to_string(n) + "px", [&, n]() {
                        menuFont.setCharGap(n);
                        gRenderer->setFont(&menuFont);
                        saveFontSettings(menuFont);
                        cgMenu.close();
                    });
                }
                cgMenu.run();
            });
            fontMenu.addItem("TOP GAP: " + std::to_string(menuFont.topGap()), [&]() {
                DefaultMenu tgMenu(&renderer, &input, &menuFont, &theme);
                tgMenu.setTitle("TOP GAP");
                tgMenu.setSubtitle("Padding above text line");
                tgMenu.addItem("BACK", [&]() { saveFontSettings(menuFont); tgMenu.close(); });
                for (int n = 0; n <= 3; n++) {
                    tgMenu.addItem(std::to_string(n) + "px", [&, n]() {
                        menuFont.setTopGap(n);
                        gRenderer->setFont(&menuFont);
                        saveFontSettings(menuFont);
                        tgMenu.close();
                    });
                }
                tgMenu.run();
            });
            fontMenu.addItem("BOTTOM GAP: " + std::to_string(menuFont.bottomGap()), [&]() {
                DefaultMenu bgMenu(&renderer, &input, &menuFont, &theme);
                bgMenu.setTitle("BOTTOM GAP");
                bgMenu.setSubtitle("Padding below text line");
                bgMenu.addItem("BACK", [&]() { saveFontSettings(menuFont); bgMenu.close(); });
                for (int n = 0; n <= 3; n++) {
                    bgMenu.addItem(std::to_string(n) + "px", [&, n]() {
                        menuFont.setBottomGap(n);
                        gRenderer->setFont(&menuFont);
                        saveFontSettings(menuFont);
                        bgMenu.close();
                    });
                }
                bgMenu.run();
            });
            fontMenu.run();
        });
        sub.run();
    });

    menu.addItem("EXIT", [&]() {
        menu.close();
        appRunning = false;
    });

    menu.run();

    appRunning = false;
    audio.shutdown();
    save.shutdown();
    input.shutdown();
    renderer.cleanup();

    system("clear");
    std::cout << "\nThank you for using plainprince's software :D\n";
    return 0;
}
