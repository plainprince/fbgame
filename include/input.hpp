#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>

enum class Key : int {
    Unknown = 0,
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    a, b, c, d, e, f, g, h, i, j, k, l, m,
    n, o, p, q, r, s, t, u, v, w, x, y, z,
    _0, _1, _2, _3, _4, _5, _6, _7, _8, _9,
    Space, Enter, Escape, Backspace, Tab,
    Up, Down, Left, Right,
    Minus, Equals, LBracket, RBracket, Backslash,
    Semicolon, Quote, Backquote, Comma, Period, Slash,
    Shift, Ctrl, Alt,
    BUTTON_A, BUTTON_B, BUTTON_X, BUTTON_Y,
    BUTTON_L1, BUTTON_R1, BUTTON_L2, BUTTON_R2,
    BUTTON_START, BUTTON_SELECT,
    COUNT
};

class InputManager {
public:
    InputManager();
    ~InputManager();

    bool init();
    void shutdown();

    bool keyHeld(Key k) const;
    bool keyPress(Key k) const;

    void setKeyMapping(const std::string& preset);
    void poll();
    void releaseKey(Key k);
    void resetKeys();

private:
    void evdevThread(int fd);
    Key evdevToKey(int code);

    int evdevFd{-1};
    std::thread evdevReader;
    std::unordered_map<int, bool> held;
    mutable std::unordered_map<int, bool> pressed;
    mutable std::mutex mtx;

    std::vector<int> evdevPressQueue;
    mutable std::mutex evdevQueueMtx;
    std::atomic<bool> running{false};

    bool useStdin{false};
    int oldFlags{0};
    std::thread reader;
    std::vector<char> stdinBuf;
    std::mutex bufMtx;
    void stdinThread();
    Key charToKey(char c);
    std::unordered_map<char, Key> charMap;
};
