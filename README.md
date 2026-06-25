# FBGame Engine

**Small and fast.** The compiled executable for ARM Cortex-A53 is just **131 kB** and takes virtually no RAM. The framebuffer is `mmap`'d into process memory — pixels are written with plain memory stores, not syscalls. Dirty-rectangle diffing means only changed pixels are written each frame. No GPU compositing, no framebuffer copy, no bloat.

Game engine for Raspberry Pi Zero running directly on the Linux framebuffer (no X11/Wayland). Games are written in Lua 5.4.

## Rendering Pipeline

1. `/dev/fb0` is opened once and **`mmap`'d with `MAP_SHARED`** — framebuffer memory mapped directly into the process
2. Virtual framebuffer in heap (~32 KB for 128×64@32bpp)
3. Draw calls write to virtual FB (plain memory stores, zero syscalls)
4. `endFrame()` scales (pixel-doubles) and converts to native format (RGB565/ARGB)
5. `pushDiff()` — compares with previous frame, writes **only changed pixels** to `mmap`'d memory

## Quick Start

```sh
make clean && make
./app
```

Requires: Lua 5.4, ALSA dev libraries, Linux fbdev headers, C++17 compiler.

## Project Structure

```
├── config.properties          Engine defaults
├── Makefile                   Build system
├── README.md                  This file
├── fonts/
│   └── <name>/
│       ├── font.data          Bitmap glyph data
│       └── font.properties    Font metrics
├── themes/
│   └── <name>.theme           Color themes
├── saves/
│   └── <namespace>/           Persistent storage
├── games/
│   ├── demogame/
│   │   ├── main.lua           Reference game
│   │   └── config.properties
│   └── <yourgame>/
│       ├── main.lua           Required: entry point
│       └── config.properties  Required: game metadata
├── include/                   C++ headers
├── src/                       C++ sources
└── docs/                      Developer documentation
    ├── lua-api.md             Full Lua API reference
    ├── game-dev.md            Game development guide
    ├── architecture.md        Internal architecture
    └── file-formats.md        Sprite, font, theme formats
```

Games are auto-discovered from `games/` subdirectories. Just create a folder with `main.lua` and `config.properties`.

## Game Configuration

`games/<name>/config.properties`:

```properties
game_name = "My Game"
game_namespace = "mygame"
fps = 15
orientation = v
```

| Key | Description |
|-----|-------------|
| `game_name` | Display name in menu (defaults to directory name) |
| `game_namespace` | Save data subdirectory under `saves/` |
| `fps` | Target framerate |
| `orientation` | `h` (128×64) or `v` (64×128) |

## Engine Configuration

Root `config.properties`:

```properties
orientation = h
fps = 30
```

Default orientation and framerate for the menu and fallback.

## Further Reading

| Document | Contents |
|----------|----------|
| [docs/lua-api.md](docs/lua-api.md) | Complete Lua API: render, input, save, audio, theme, menu, Color, globals |
| [docs/game-dev.md](docs/game-dev.md) | Creating games, layout patterns, fonts, sprites, performance |
| [docs/architecture.md](docs/architecture.md) | Source file breakdown, build system, dependencies |
| [docs/file-formats.md](docs/file-formats.md) | Sprite, font, theme, save, and config file formats |
