# Architecture

## Overview

FBGame is a minimal C++17 game engine that runs directly on the Linux framebuffer (no X11/Wayland). Games are written in Lua 5.4. The executable for ARM Cortex-A53 is ~131 kB.

The engine boots to a C++ main menu (PLAY → game list, SETTINGS → orientation/themes/fonts, EXIT). Selecting a game loads and runs its `main.lua` in a Lua coroutine, yielding each frame.

### Rendering Pipeline

1. `/dev/fb0` is opened once, framebuffer geometry is queried via `ioctl`
2. The framebuffer is **`mmap`'d with `MAP_SHARED`** — a direct pointer to display memory
3. A virtual framebuffer (~32 KB for 128×64@32bpp) is maintained in heap
4. All draw calls write to the virtual FB (plain memory stores, zero syscalls)
5. `endFrame()` pixel-doubles the virtual canvas and converts pixels to the native framebuffer format (RGB565/ARGB/etc.)
6. `pushDiff()` compares against the previous frame — **only changed pixels** are written to the `mmap`'d memory

No GPU compositing, no framebuffer copy, no malloc/free per frame.

---

## Source Files

### src/main.cpp

Engine entry point. Initializes all subsystems, scans `games/`, presents the main menu, and manages the game lifecycle.

**Key functions:**
- `scanGames()` — iterates `games/` subdirectories, finds those with `main.lua`, reads `config.properties`
- `runLuaGame()` — sets up Lua state, runs `setup()`/`loop()`/`shutdown()`, manages framerate timing
- `loadGameFont()` — attempts to load a per-game font from the game directory (prints error to stderr on failure, continues with current font)
- `scanFonts()` — lists available fonts in `fonts/`
- `loadAllSettings()` / `saveSetting()` / `saveFontSettings()` — read/write persistent settings via SaveManager
- `pinToCore()` — sets CPU affinity (menu on core 0, render thread on core 2)
- `signalHandler()` — restores cursor and sets exit flag on SIGINT/SIGTERM

**Game loop (per frame):**
```
poll input → beginFrame → callLoop(dt) → endFrame → sleep for remaining frame duration
```

The main menu is built with `DefaultMenu` callbacks: PLAY (submenu with game list), SETTINGS (orientation toggle, theme selector, font config), EXIT.

### src/lua_bridge.cpp

Bridges C++ engine systems to Lua. Registers all global functions (`render.*`, `input.*`, `save.*`, `audio.*`, `menu.*`, `theme.*`, `Color`, `shutdown`, `quit`, `yield`) and color constants.

**Key components:**
- `registerFuncs()` — creates Lua tables for each module and binds C++ functions
- `load()` — creates Lua state, injects globals, executes the game script
- `callSetup()` — calls Lua `setup()` function via `lua_pcall`
- `callLoop()` — manages a persistent coroutine thread. On first call, pushes `loop` function and `dt`, then `lua_resume`. On subsequent calls, resumes the existing thread with `dt`. Handles `LUA_YIELD` for frame-by-frame execution.
- `callShutdown()` — calls Lua `shutdown()` function
- `luaMenuTick()` — renders a scrollable menu overlay with theme-aware colors, button hover effects, centered scrolling
- `luaSprite()` / `luaAudioPlay()` — path handling note: audio paths are prefixed with the game directory, sprite paths are used as-is (relative to CWD)

**Coroutine lifecycle:**
- A single persistent coroutine thread is created (stored via `luaL_ref` in the registry)
- Each frame, `lua_resume` is called with the current `dt`
- When the game calls `yield()`, the coroutine suspends until next frame
- On error or game exit, the coroutine reference is freed

### src/render.cpp

The full rendering pipeline: framebuffer init, virtual canvas management, double buffering, scaling, and dirty-rectangle diffing.

**Key functions:**
- `init()` — opens `/dev/fb0`, `ioctl` to get display geometry, `mmap` with `MAP_SHARED`, allocates virtual and scaled framebuffers
- `cleanup()` — restores cursor, frees buffers, `munmap`, closes fd
- `beginFrame()` — swaps double buffers, clears current virtual FB with background color
- `endFrame()` — calls `convertAndScale()` then `pushDiff()`
- `convertAndScale()` — pixel-doubles virtual FB into scaled FB, converts each pixel to native format via `nativePixel()`
- `pushDiff()` — compares `scaledFB` to `prevScaledFB`, writes only differing pixels to the `mmap`'d real framebuffer

**Primitives (all operate on virtual canvas coordinates):**
- `pixel()` — single pixel write with bounds checking
- `fillRect()` — filled rectangle (optimized with `std::fill` for full-width spans)
- `drawRect()` — 1px border rectangle outline
- `fillCircle()` — integer-coordinate filled circle (midpoint algorithm)
- `drawCircle()` — circle outline
- `line()` — Bresenham line algorithm
- `text()` — renders glyphs from bitmap font, supports word-wrap and char-wrap, centering, and max-width clipping via the `Font` object
- `sprite()` — draws pixel-by-pixel, skipping alpha=0 (transparent) pixels
- `clear()` — fills entire virtual canvas with a single color

**Orientation:**
- `resize()` and `setOrientation()` — switch between 128×64 (horizontal) and 64×128 (vertical), recalculate scale factor and centering offsets
- Scale factor is `min(realW / virtW, realH / virtH)`, auto-centered on the physical display

### src/input.cpp

Reads from stdin (serial terminal, ASCII/ANSI escape sequences) and evdev (USB gamepad) in separate threads.

**Key components:**
- `init()` — sets stdin to raw mode (non-blocking), starts stdin reader thread, scans `/dev/input/eventN` for gamepad
- `shutdown()` — joins threads, restores terminal settings
- `stdinThread()` — reads characters from stdin, buffers them; converts ANSI escape codes (`\e[A` etc.) to virtual keycodes
- `evdevThread()` — reads `/dev/input/eventN`, maps Linux input event codes to `Key` enum
- `poll()` — called each frame, processes buffered stdin input, merges with evdev events, updates key states
- `keyPress()` — edge-triggered: returns true once per press, then clears the flag
- `keyHeld()` — level-triggered: returns true while the key is held down
- `resetKeys()` — clears all key state (used between game sessions)

**Input mappings:**
- ASCII chars → corresponding `Key::A` through `Key::Z`, `Key::Space`, `Key::Enter`
- ANSI escape sequences → `Key::Up/Down/Left/Right`
- Ctrl+C/Escape handling for exit
- WASD mapped as arrow key aliases

### src/audio.cpp

WAV playback via ALSA. Loads 8/16-bit WAV files, plays them asynchronously.

**Key functions:**
- `loadWav()` — parses RIFF/WAVE chunks, extracts format, sample rate, channels, raw PCM data
- `play()` — if `running`, spawns a one-shot thread that opens ALSA (`snd_pcm_open`), sets parameters (format, rate, channels), writes PCM data, then closes
- Only one sound plays at a time (replaces any currently playing sound)
- Engine continues without audio if ALSA is unavailable or init fails

### src/save.cpp

Async file I/O with write-back caching for persistent game data.

**Key functions:**
- `init()` — creates base save directory, starts background worker thread
- `shutdown()` — signals worker, joins thread, flushes pending writes
- `read()` — checks in-memory cache first, falls back to reading from `saves/<name>.txt`, caches result
- `writeAsync()` — updates cache and enqueues write operation; worker thread writes to disk
- `flush()` — drains the write queue synchronously (called on game exit)

**Path structure:** `saves/<namespace>/<filename>.txt` where namespace comes from `game_namespace` in config.

### src/font.cpp

Loads monospaced bitmap fonts from ASCII-art data files.

**Key functions:**
- `load()` — reads `font.properties` for metrics (char_width, char_height, char_gap, etc.) and `font_chars` string; reads `font.data` for glyph bitmaps; auto-calculates `actualWidth` per glyph for tighter kerning
- `textWidth()` — calculates pixel width of a string
- `textHeight()` — returns glyph height

**Glyph format:** Each character occupies `char_height` rows in the data file. `#` and `@` are filled pixels; `.` and space are empty. `actualWidth` is automatically calculated as the rightmost filled column for tighter character spacing.

### src/sprite.cpp

Loads and saves sprites in a simple text RGBA format.

**Key functions:**
- `load()` — reads `width height` header, then `r g b a` for each pixel
- `save()` — writes sprite data back to file

Format is one pixel per line (row-major). Alpha=0 pixels are transparent during rendering.

### src/config.cpp

Generic `key=value` properties file parser.

**Key functions:**
- `load()` — reads file, parses `key = value` lines, supports `#` comments and quoted strings
- `save()` — writes properties back to file
- Typed getters: `getString()`, `getInt()`, `getBool()`, `getFloat()`, `has()`
- `fromString()` / `toString()` — serialize/deserialize from in-memory string (used for save data)

### src/theme.cpp

Loads `.theme` files and provides named color lookups.

**Key functions:**
- `load()` — reads properties file, parses hex RGB/RGBA values (6-8 hex chars)
- `get()` — returns color by name with fallback
- `defaultTheme()` — provides hardcoded default colors

Colors can be quoted or unquoted strings in the theme file.

### src/menu.cpp

The C++ main menu system (`DefaultMenu`). Provides a scrollable, selectable menu with title, subtitle, and buttons.

**Key features:**
- `addItem()` — adds a clickable button with label and callback action
- `draw()` — renders title (primary color), subtitle (dim color), buttons with hover highlight and accent border
- `handleInput()` — Up/Down/W/S to navigate (wrap-around), Enter/Space to select, Escape to close
- `centerOnSelected()` — auto-scrolls to keep the selected item centered on screen
- `wrappedLines()` — word-wrap calculation for multi-line button labels
- `contentHeight()` — calculates total content height including proper bottom margin
- `itemHeight()` — returns `font->textHeight()` (char height + top/bottom gaps)
- FPS is fixed at 60 for the menu

**Button height formula** (used consistently in `draw()`, `contentHeight()`, and `centerOnSelected()`):
```
buttonHeight = wrappedLines * itemHeight() + 2
```
The `+2` provides 1px top and 1px bottom padding per button, regardless of line count. No extra `(lines-1) * itemSpacing` is added between wrapped lines within a single button, because the renderer's `text()` already advances by `textHeight()` per line via the font's built-in top/bottom gaps.

The Lua menu counterpart in `lua_bridge.cpp` mirrors this layout with per-item dynamic heights computed from wrapped-line count.

---

## Include Files

### include/types.hpp

Core types:
- `Color` — 4× uint8_t (RGBA), `pack()` → uint32_t, `unpack()` from uint32_t, predefined constants (`COLOR_BLACK`, `COLOR_WHITE`, etc.)
- `Vec2` — `x, y` int pair
- `Rect` — `x, y, w, h` int rectangle
- `Key` enum — all supported keys (A-Z, modifiers, arrows, gamepad buttons)
- `Orientation` enum — `Horizontal`, `Vertical`
- `WrapMode` enum — `None`, `Word`, `Char`
- `LuaGameState` struct — holds pointers to all engine subsystems for Lua bridge access

### include/render.hpp

`Renderer2D` class declaration — framebuffer management, drawing primitives, virtual canvas.

### include/input.hpp

`InputManager` class declaration — stdin/evdev input, key state management, `keyPress`/`keyHeld`.

### include/audio.hpp

`AudioManager` class declaration — WAV loading and ALSA playback.

### include/save.hpp

`SaveManager` class declaration — async file I/O, in-memory caching, background worker thread.

### include/font.hpp

`Font` class declaration — bitmap glyph storage, metrics, text measurement.

### include/sprite.hpp

`Sprite` struct (width, height, pixel vector) and `SpriteLoader` class for file I/O.

### include/theme.hpp

`Theme` class declaration — color map, named lookups, hex parsing.

### include/config.hpp

`Properties` class declaration — key-value parser, typed getters, file I/O.

### include/menu.hpp

`DefaultMenu` class declaration — menu items, callbacks, rendering, scrolling.

### include/lua_bridge.hpp

`LuaBridge` class declaration — Lua state management, coroutine handling, function registration.

---

## Build System

### Makefile

```makefile
CXX=ccache g++
CXXFLAGS=-std=c++17 -O3 -mcpu=native -pipe -Wall -Wextra -Iinclude \
    $(shell pkg-config --cflags lua5.4 2>/dev/null || echo "-I/usr/include/lua5.4")
LDFLAGS=$(shell pkg-config --libs lua5.4 alsa 2>/dev/null || echo "-llua5.4 -lasound") \
    -lpthread -lstdc++fs -flto -s

SRC=$(wildcard src/*pp)
OBJ=$(SRC:.cpp=.o)

app: $(OBJ)
    $(CXX) $(OBJ) -o app $(LDFLAGS)
```

- Wildcard-based — automatically picks up any `.cpp` in `src/`
- LTO (`-flto`) and stripped (`-s`) for minimal binary size
- `pkg-config` with fallback for Lua and ALSA
- `-mcpu=native` for host-native optimization; use `-mcpu=cortex-a53` for Pi Zero cross-compiles
- Requires `-lstdc++fs` for `<filesystem>`

### Dependencies

- **Lua 5.4** — development headers and library (`liblua5.4-dev`, `lua5.4`)
- **ALSA** — `libasound2-dev` (optional — engine runs without audio)
- **Linux fbdev headers** — part of kernel headers, usually pre-installed
- **C++17 compiler** — GCC recommended (`g++`)
- **pkg-config** — for locating library flags

### Building

```sh
make clean && make
```

### Cross-compiling for Pi Zero

```makefile
CXX=arm-linux-gnueabihf-g++
CXXFLAGS=-std=c++17 -O3 -mcpu=cortex-a53 -mfpu=neon-fp-armv8 -pipe -Wall -Wextra
```
