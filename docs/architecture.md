# Architecture

## Overview

FBGame is a minimal C++17 game engine that runs directly on the Linux framebuffer (no X11/Wayland). Games are written in Lua 5.4. The executable for ARM Cortex-A53 is ~195 kB.

The engine boots to a C++ main menu (PLAY → game list, SETTINGS → orientation/themes/fonts/volume, EXIT). Selecting a game loads and runs its `main.lua` in a Lua coroutine, yielding each frame.

### Rendering Pipeline

1. `/dev/fb0` is opened once, framebuffer geometry is queried via `ioctl` (both `FBIOGET_VSCREENINFO` and `FBIOGET_FSCREENINFO`)
2. The framebuffer is **`mmap`'d with `MAP_SHARED`** — a direct pointer to display memory
3. Virtual framebuffer (~32 KB for 128×64@32bpp) and parallel mono metadata buffer maintained in heap
4. All draw calls write to the virtual FB (plain memory stores, zero syscalls), with optional mono-mode override tags
5. `endFrame()` pixel-doubles the virtual canvas, converts pixels to native format (RGB565/ARGB/etc.), and applies mono color quantization (luminance or Sobel edge detection)
6. `pushDiff()` compares against the previous scaled frame — **only changed pixels** are written to the `mmap`'d memory
7. `drawDebugBorder()` draws a white 1px border around the scaled viewport

No GPU compositing, no framebuffer copy, no malloc/free per frame.

### Mono Color System

When `-m` flag is passed or the display is grayscale, the engine operates in mono mode:

- **2-color mono:** luminance-thresholded binary (black/white)
- **3-256 color mono:** luminance-quantized grayscale
- **Color mapping:** `mapColor()` and `mapColorRange()` override specific source colors to arbitrary gray levels
- **Edge mode** (`mono_mode = edge`): Sobel edge detection highlights boundaries in >2-color mono

The mono metadata buffer (`monoFB`) tracks a per-pixel mono mode tag (0=auto, 1=force white, 2=force black), enabling overlay UIs to always render with correct contrast.

---

## Source Files

### src/main.cpp

Engine entry point. Initializes all subsystems, scans `games/`, presents the main menu, and manages the game lifecycle.

**Key functions:**
- `scanGames()` — iterates `games/` subdirectories, finds those with `main.lua`, reads `config.properties`
- `runLuaGame()` — sets up Lua state, runs `setup()`/`loop()`/`shutdown()`, manages framerate timing, captures screenshots on F12
- `loadGameFont()` — attempts to load a per-game font from the game directory (prints error to stderr on failure, continues with current font)
- `scanFonts()` — lists available fonts in `fonts/`
- `loadAllSettings()` / `saveSetting()` / `saveFontSettings()` — read/write persistent settings via SaveManager
- `pinToCore()` — sets CPU affinity (menu on core 0, render thread on core 2)
- `signalHandler()` — restores cursor and sets exit flag on SIGINT/SIGTERM

**Game loop (per frame):**
```
poll input → beginFrame → callLoop(dt) → endFrame → sleep for remaining frame duration
```

The main menu is built with `DefaultMenu` callbacks: PLAY (submenu with game list), SETTINGS (orientation toggle, theme selector, font config with spacing, volume), EXIT.

**Command-line arguments parsed:**
- `-0` through `-9` — selects `/dev/fbN` (default 0)
- `-m` or `-m2` through `-m9` — enables mono mode with N shades

**Screenshots:** Press F12 in-game to save a PPM to `screenshots/<game>_<timestamp>.ppm`.

**Exit:** Ctrl+Escape forces game exit.

### src/lua_bridge.cpp

Bridges C++ engine systems to Lua. Registers all global functions (`render.*`, `input.*`, `save.*`, `audio.*`, `menu.*`, `theme.*`, `Color`, `shutdown`, `quit`, `yield`) and color constants.

**Registered Lua modules:**
- `render` — pixel, fillRect, drawRect, fillCircle, drawCircle, line, text, sprite, clear, mapColor, mapColorRange, getWidth, getHeight, setFPS, setOrientation, themeColor, color
- `input` — keyHeld, keyPress
- `save` — read, write
- `audio` — play, playSfx, playMusic, stopMusic, stopAll, pauseAll, resumeAll, setVolume, getVolume
- `menu` — create, tick
- `theme` — get

**Key components:**
- `registerFuncs()` — creates Lua tables for each module and binds C++ functions
- `load()` — creates Lua state, injects globals, executes the game script
- `callSetup()` — calls Lua `setup()` function via `lua_pcall`
- `callLoop()` — manages a persistent coroutine thread. On first call, pushes `loop` function and `dt`, then `lua_resume`. On subsequent calls, resumes the existing thread with `dt`. Handles `LUA_YIELD` for frame-by-frame execution.
- `callShutdown()` — calls Lua `shutdown()` function
- `luaMenuTick()` — renders a scrollable menu overlay with theme-aware colors, button hover effects, mono-mode-aware rendering, centered scrolling
- `luaSprite()` / `luaAudioPlay()` — path handling note: audio paths are prefixed with the game directory, sprite paths are used as-is (relative to CWD)

**Coroutine lifecycle:**
- A single persistent coroutine thread is created (stored via `luaL_ref` in the registry)
- Each frame, `lua_resume` is called with the current `dt`
- When the game calls `yield()`, the coroutine suspends until next frame
- On error or game exit, the coroutine reference is freed

### src/render.cpp

The full rendering pipeline: framebuffer init, virtual canvas management, double buffering, scaling, mono color quantization, and dirty-rectangle diffing.

**Key functions:**
- `init()` — opens `/dev/fb0`, `ioctl` to get display geometry, `mmap` with `MAP_SHARED`, allocates virtual, mono, and scaled framebuffers
- `cleanup()` — restores cursor, frees buffers, `munmap`, closes fd
- `beginFrame()` — swaps double buffers, clears current virtual FB and mono buffer
- `endFrame()` — calls `convertAndScale()` then `pushDiff()` then `drawDebugBorder()`
- `convertAndScale()` — pixel-doubles virtual FB into scaled FB, converts each pixel to native format via `nativePixel()`, applies mono quantization (luminance or edge detection) and color mapping
- `pushDiff()` — compares `scaledFB` to `prevScaledFB`, writes only differing pixels to the `mmap`'d real framebuffer
- `drawDebugBorder()` — draws a white 1px debug border around the scaled viewport on the real framebuffer

**Mono system:**
- `monoFB` — parallel int8 buffer tracking per-pixel mono mode (0=auto/luma, 1=force white, 2=force black)
- `mapColor()` / `mapColorRange()` — register source color → target gray overrides
- `clearColorMap()` — resets all color map entries
- `nativePixel()` — handles 2-color threshold, N-color quantization, and color mapping
- `checkColorMap()` / `checkColorMapRange()` — lookup in color map tables

**Primitives (all operate on virtual canvas coordinates, accept a monoMode parameter):**
- `pixel()` — single pixel write with bounds checking
- `fillRect()` — filled rectangle
- `drawRect()` — 1px border rectangle outline
- `fillCircle()` — integer-coordinate filled circle
- `drawCircle()` — circle outline (parametric, 2-degree steps)
- `line()` — Bresenham line algorithm
- `text()` — renders glyphs from bitmap font, supports word-wrap and char-wrap, centering, and max-width clipping via the `Font` object
- `sprite()` — draws pixel-by-pixel, skipping alpha=0 (transparent) pixels
- `clear()` — fills entire virtual canvas with a single color
- `saveScreenshot()` — writes a PPM file of the current virtual canvas

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
- `releaseKey()` — releases a specific key

**Input mappings:**
- ASCII chars → corresponding `Key::A` through `Key::Z`, `Key::Space`, `Key::Enter`
- ANSI escape sequences → `Key::Up/Down/Left/Right`
- Ctrl+Escape for game exit, F12 for screenshot
- WASD mapped as arrow key aliases
- Full evdev gamepad support: A/B/X/Y, Start/Select, L1/R1/L2/R2, D-pad

### src/audio.cpp

WAV playback via ALSA with a background mixer thread. Supports two independent channels: SFX (one-shot) and Music (looped).

**Key functions:**
- `init()` — starts the background mixer thread
- `shutdown()` — signals stop, joins mixer thread
- `loadWav()` — parses RIFF/WAVE chunks, extracts format, sample rate, channels, raw PCM data; converts multi-channel to mono; handles 8 and 16-bit
- `mixerFunc()` — background thread: opens ALSA `default` device (44.1kHz, S16_LE, mono), processes SFX/music queues, mixes active voices, handles underrun recovery (`EPIPE` → `snd_pcm_prepare`)
- `playSfx()` — queues a one-shot sound (interrupts previous SFX)
- `playMusic()` — queues a looping music track (stops previous music)
- `stopMusic()` / `stopAll()` / `pauseAll()` / `resumeAll()` — playback control
- `setVolume()` / `getVolume()` — master volume (0.0–1.0)

### src/save.cpp

Async file I/O with write-back caching for persistent game data.

**Key functions:**
- `init()` — creates base save directory, starts background worker thread
- `shutdown()` — signals worker, joins thread, flushes pending writes
- `read()` — checks in-memory cache first, falls back to reading from `saves/<path>.txt`, caches result
- `writeAsync()` — updates cache and enqueues write operation; worker thread writes to disk
- `flush()` — drains the write queue synchronously (called on game exit)

**Path structure:** `saves/<namespace>/<filename>.txt` where namespace comes from `game_namespace` in config.

### src/font.cpp

Loads monospaced bitmap fonts from ASCII-art data files.

**Key functions:**
- `load()` — reads `font.properties` for metrics (char_width, char_height, char_gap, top_gap, bottom_gap, wrap_mode) and `font_chars` string; reads `font.data` for glyph bitmaps; auto-calculates `actualWidth` per glyph for tighter kerning
- `textWidth()` — calculates pixel width of a string (respects `spaceAdvance()` and `charGap`)
- `textHeight()` — returns glyph height + top_gap + bottom_gap
- `glyph()` — lookup by character, falls back to space glyph on missing
- `hasGlyph()` — checks if a character has a defined glyph
- `spaceAdvance()` — returns `max(2, cw/2 + 1)` for space width

**Glyph format:** Each character occupies `char_height` rows in the data file. `#` and `@` are filled pixels; `.` and space are empty. `actualWidth` is automatically calculated as the rightmost filled column for tighter character spacing.

### src/sprite.cpp

Loads and saves sprites in a simple text RGBA format.

**Key functions:**
- `load()` — reads `width height` header, then `r g b a` for each pixel
- `save()` — writes sprite data back to file (matrix layout, spaces between pixels on same row)

Format is one pixel per line (row-major). Alpha=0 pixels are transparent during rendering.

### src/config.cpp

Generic `key=value` properties file parser.

**Key functions:**
- `load()` — reads file, parses `key = value` lines, supports `#` comments and quoted strings
- `save()` — writes properties back to file
- Typed getters: `getString()`, `getInt()`, `getBool()`, `getFloat()`, `has()`
- Setters: `setString()`, `setInt()`, `setBool()`
- `fromString()` / `toString()` — serialize/deserialize from in-memory string (used for save data)
- `keys()` — returns all property keys

### src/theme.cpp

Loads `.theme` files and provides named color lookups.

**Key functions:**
- `load()` — reads properties file, parses hex RGB/RGBA values (6-8 hex chars)
- `get()` — returns color by name with fallback
- `getInt()` — reads integer properties from the raw config (used for `mono_conversion`, `mono2_fill`, etc.)
- `defaultTheme()` — provides hardcoded default colors

Colors can be quoted or unquoted strings in the theme file.

### src/menu.cpp

The C++ main menu system (`DefaultMenu`). Provides a scrollable, selectable menu with title, subtitle, and buttons.

**Key features:**
- `addItem()` — adds a clickable button with label and callback action (supports `MenuItem` struct with `dynamicLabel`)
- `draw()` — renders title (primary color), subtitle (dim color), buttons with hover highlight and accent border, mono-mode-aware colors
- `handleInput()` — Up/Down/W/S to navigate (wrap-around), Enter/Space to select, Escape to close
- `centerOnSelected()` — auto-scrolls to keep the selected item centered on screen
- `wrappedLines()` — word-wrap calculation for multi-line button labels
- `contentHeight()` — calculates total content height including proper bottom margin
- `clampScroll()` — constrains scroll offset within valid range
- FPS is fixed at 60 for the menu

**Button height formula** (used consistently in `draw()`, `contentHeight()`, and `centerOnSelected()`):
```
buttonHeight = wrappedLines * itemHeight() + 2
```

### src/lua_bridge.cpp (Lua Menu)

The Lua menu counterpart renders a scrollable menu overlay within games (pause screens, level select, etc.). Mirrors the C++ menu layout with per-item dynamic heights, word-wrapping, scroll centering, and mono-mode-aware rendering.

---

## Include Files

### include/types.hpp

Core types:
- `Color` — 4× uint8_t (RGBA), `pack()` → uint32_t, `unpack()` from uint32_t, predefined constants (`COLOR_BLACK`, `COLOR_WHITE`, ... `COLOR_TRANSP`)
- `Vec2` — `x, y` int pair
- `Rect` — `x, y, w, h` int rectangle
- `Key` enum — all supported keys (A-Z, a-z, 0-9, modifiers, arrows, F12, gamepad buttons L1-R2)
- `Orientation` enum — `Horizontal`, `Vertical`
- `WrapMode` enum — `None`, `Word`, `Char`
- `LuaGameState` struct — holds pointers to all engine subsystems for Lua bridge access

### include/render.hpp

`Renderer2D` class declaration — framebuffer management, drawing primitives, virtual canvas, mono color system, color mapping.

### include/input.hpp

`InputManager` class declaration — stdin/evdev input, key state management, `keyPress`/`keyHeld`.

### include/audio.hpp

`AudioManager` class declaration — WAV loading, ALSA mixer playback, SFX/music channels.

### include/save.hpp

`SaveManager` class declaration — async file I/O, in-memory caching, background worker thread.

### include/font.hpp

`Font` class declaration — bitmap glyph storage, metrics, text measurement, gap settings.

### include/sprite.hpp

`Sprite` struct (width, height, pixel vector) and `SpriteLoader` class for file I/O.

### include/theme.hpp

`Theme` class declaration — color map, named lookups, hex parsing, integer config access.

### include/config.hpp

`Properties` class declaration — key-value parser, typed getters/setters, file I/O, in-memory serialization.

### include/menu.hpp

`DefaultMenu` class declaration — menu items with dynamic labels, callbacks, rendering, scrolling.

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
- `ccache` for faster rebuilds
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
CXXFLAGS=-std=c++17 -O3 -mcpu=cortex-a53 -mfpu=neon-fp-armv8 -pipe -Wall -Wextra -Iinclude \
    $(shell pkg-config --cflags lua5.4 2>/dev/null || echo "-I/usr/include/lua5.4")
LDFLAGS=$(shell pkg-config --libs lua5.4 alsa 2>/dev/null || echo "-llua5.4 -lasound") \
    -lpthread -lstdc++fs -flto -s
```
