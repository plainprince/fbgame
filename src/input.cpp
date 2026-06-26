#include <input.hpp>
#include <iostream>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <cerrno>
#include <linux/input.h>

InputManager::InputManager() {
    charMap['w'] = Key::W; charMap['W'] = Key::W;
    charMap['a'] = Key::A; charMap['A'] = Key::A;
    charMap['s'] = Key::S; charMap['S'] = Key::S;
    charMap['d'] = Key::D; charMap['D'] = Key::D;
    charMap[' '] = Key::Space;
    charMap['\n'] = Key::Enter;
    charMap['\t'] = Key::Tab;
    charMap[127] = Key::Backspace;
}

InputManager::~InputManager() { shutdown(); }

bool InputManager::init() {
    running = true;

    termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    oldFlags = raw.c_lflag;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_lflag |= ISIG;
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    char drain[64];
    while (read(STDIN_FILENO, drain, sizeof(drain)) > 0) {}

    useStdin = true;
    reader = std::thread(&InputManager::stdinThread, this);

    for (int i = 0; i < 32; i++) {
        std::string evpath = "/dev/input/event" + std::to_string(i);
        int fd = open(evpath.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        evdevFd = fd;
        std::cerr << "input: evdev on " << evpath << "\n";
        evdevReader = std::thread(&InputManager::evdevThread, this, evdevFd);
        break;
    }
    return true;
}

void InputManager::shutdown() {
    running = false;

    if (evdevReader.joinable()) {
        evdevReader.join();
        if (evdevFd >= 0) close(evdevFd);
        evdevFd = -1;
    }

    if (reader.joinable()) reader.join();

    termios cooked;
    tcgetattr(STDIN_FILENO, &cooked);
    cooked.c_lflag = oldFlags;
    tcsetattr(STDIN_FILENO, TCSANOW, &cooked);
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
}

void InputManager::stdinThread() {
    while (running) {
        char buf[64];
        ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
        if (n > 0) {
            std::lock_guard<std::mutex> lk(bufMtx);
            stdinBuf.insert(stdinBuf.end(), buf, buf + n);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void InputManager::evdevThread(int fd) {
    struct input_event ev;
    while (running) {
        ssize_t n = read(fd, &ev, sizeof(ev));
        if (n < 0) {
            if (errno == EAGAIN) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            break;
        }
        if (n < (ssize_t)sizeof(ev)) continue;
        if (ev.type != EV_KEY) continue;

        Key k = evdevToKey(ev.code);
        if (k == Key::Unknown) continue;

        std::lock_guard<std::mutex> lk(mtx);
        if (ev.value == 1) {
            held[(int)k] = true;
            {
                std::lock_guard<std::mutex> lk2(evdevQueueMtx);
                evdevPressQueue.push_back((int)k);
            }
        } else if (ev.value == 0) {
            held[(int)k] = false;
        }
    }
}

Key InputManager::charToKey(char c) {
    auto it = charMap.find(c);
    if (it != charMap.end()) return it->second;
    if (c >= 'a' && c <= 'z') return (Key)((int)Key::a + (c - 'a'));
    if (c >= 'A' && c <= 'Z') return (Key)((int)Key::A + (c - 'A'));
    if (c >= '0' && c <= '9') return (Key)((int)Key::_0 + (c - '0'));
    switch (c) {
        case '-': return Key::Minus;
        case '=': return Key::Equals;
        case '[': return Key::LBracket;
        case ']': return Key::RBracket;
        case '\\': return Key::Backslash;
        case ';': return Key::Semicolon;
        case '\'': return Key::Quote;
        case ',': return Key::Comma;
        case '.': return Key::Period;
        case '/': return Key::Slash;
    }
    return Key::Unknown;
}

void InputManager::poll() {
    std::lock_guard<std::mutex> lk(mtx);

    for (int i = 0; i < (int)Key::COUNT; i++)
        pressed[i] = false;

    if (evdevFd < 0)
        for (int i = 0; i < (int)Key::COUNT; i++)
            held[i] = false;

    {
        std::lock_guard<std::mutex> lk2(evdevQueueMtx);
        for (int code : evdevPressQueue)
            pressed[code] = true;
        evdevPressQueue.clear();
    }

    std::vector<char> buf;
    {
        std::lock_guard<std::mutex> lk2(bufMtx);
        buf.swap(stdinBuf);
    }

    bool seen[(int)Key::COUNT]{false};

    for (size_t i = 0; i < buf.size(); i++) {
        unsigned char c = (unsigned char)buf[i];
        if (c == 27 && i + 2 < buf.size() && buf[i+1] == '[') {
            Key k = Key::Unknown;
            switch (buf[i+2]) {
                case 'A': k = Key::Up;    break;
                case 'B': k = Key::Down;  break;
                case 'C': k = Key::Right; break;
                case 'D': k = Key::Left;  break;
            }
            i += 2;
            if (k != Key::Unknown && !seen[(int)k]) {
                seen[(int)k] = true;
                pressed[(int)k] = true;
            }
            continue;
        }
        if (c == 27) {
            if (!seen[(int)Key::Escape]) {
                seen[(int)Key::Escape] = true;
                pressed[(int)Key::Escape] = true;
            }
            continue;
        }
        Key k = charToKey((char)c);
        if (k != Key::Unknown && !seen[(int)k]) {
            seen[(int)k] = true;
            pressed[(int)k] = true;
        }
    }
}

bool InputManager::keyHeld(Key k) const {
    std::lock_guard<std::mutex> lk(mtx);
    auto it = held.find((int)k);
    return it != held.end() && it->second;
}

bool InputManager::keyPress(Key k) const {
    std::lock_guard<std::mutex> lk(mtx);
    auto it = pressed.find((int)k);
    if (it != pressed.end() && it->second) {
        it->second = false;
        return true;
    }
    return false;
}

void InputManager::releaseKey(Key k) {
    std::lock_guard<std::mutex> lk(mtx);
    held[(int)k] = false;
    pressed[(int)k] = false;
}

void InputManager::resetKeys() {
    std::lock_guard<std::mutex> lk(mtx);
    for (int i = 0; i < (int)Key::COUNT; i++) {
        held[i] = false;
        pressed[i] = false;
    }
}

void InputManager::setKeyMapping(const std::string& preset) {
    (void)preset;
}

Key InputManager::evdevToKey(int code) {
    switch (code) {
        case 1: return Key::Escape;
        case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: case 10:
            return (Key)((int)Key::_1 + (code - 2));
        case 11: return Key::_0;
        case 14: return Key::Backspace;
        case 15: return Key::Tab;
        case 16: return Key::Q;
        case 17: return Key::W;
        case 18: return Key::E;
        case 19: return Key::R;
        case 20: return Key::T;
        case 21: return Key::Y;
        case 22: return Key::U;
        case 23: return Key::I;
        case 24: return Key::O;
        case 25: return Key::P;
        case 26: return Key::LBracket;
        case 27: return Key::RBracket;
        case 28: return Key::Enter;
        case 29: return Key::Ctrl;
        case 30: return Key::A;
        case 31: return Key::S;
        case 32: return Key::D;
        case 33: return Key::F;
        case 34: return Key::G;
        case 35: return Key::H;
        case 36: return Key::J;
        case 37: return Key::K;
        case 38: return Key::L;
        case 39: return Key::Semicolon;
        case 40: return Key::Quote;
        case 41: return Key::Backquote;
        case 42: return Key::Shift;
        case 43: return Key::Backslash;
        case 44: return Key::Z;
        case 45: return Key::X;
        case 46: return Key::C;
        case 47: return Key::V;
        case 48: return Key::B;
        case 49: return Key::N;
        case 50: return Key::M;
        case 51: return Key::Comma;
        case 52: return Key::Period;
        case 53: return Key::Slash;
        case 54: return Key::Shift;
        case 55: return Key::Unknown;
        case 56: return Key::Alt;
        case 57: return Key::Space;
        case 97: return Key::Ctrl;
        case 100: return Key::Alt;
        case 103: return Key::Up;
        case 105: return Key::Left;
        case 106: return Key::Right;
        case 88: return Key::F12;
        case 108: return Key::Down;
        default: return Key::Unknown;
    }
}
