# Game Development Guide

## Creating a New Game

1. Create a directory under `games/`:
   ```
   games/yourgame/
   ├── main.lua
   └── config.properties
   ```

2. Write `config.properties`:
   ```properties
   game_name = "Your Game"
   game_namespace = "yourgame"
   fps = 15
   orientation = v
   mono_mode = luma
   ```

3. Write `main.lua` with `setup()`, `loop()`, `shutdown()` functions (see [lua-api.md](lua-api.md)).

4. Build (`make clean && make`) and run `./app`. Your game appears automatically in the PLAY menu.

## Layout Patterns

Design for the virtual canvas orientation:

**Horizontal (128×64)** — side-scrollers, platformers, wide UIs:
```
+------------------------------------------+
|                                          |
|         128 pixels wide                   |
|                                          |
+------------------------------------------+
         64 pixels tall
```

**Vertical (64×128)** — Tetris, vertical shooters, score panels:
```
+----------+
|          |
|          |
| 128 tall |
|          |
|          |
+----------+
   64 wide
```

## Font System

Fonts live under `fonts/<name>/`. The engine ships with a default font at `fonts/default/`. To use a different font, add it under `fonts/` and select it from the Settings → Fonts menu.

```
fonts/
├── default/
│   ├── font.data          Bitmap glyph data (ASCII art)
│   └── font.properties    Font metrics
└── <yourfont>/
    ├── font.data
    └── font.properties
```

**Do not put font files in your game directory.** Per-game font files are technically loaded if present, but using them is strongly discouraged. Use one of the preset fonts from the `fonts/` folder instead. New font contributions are welcome — just add a directory under `fonts/` with the required files.

See [file-formats.md](file-formats.md) for the font file format specification.

## Sprite Usage

Place sprite files in your game directory (e.g. `games/yourgame/sprites/`).

**Sprite path caveat:** `render.sprite()` does NOT auto-prefix the game directory. Use a CWD-relative path:

```lua
render.sprite(x, y, "games/yourgame/sprites/block.spr")
```

This differs from `audio.play()` which IS relative to the game directory.

For performance, consider caching sprite data via `io.open()` in `setup()` and drawing with `render.pixel()`, since `render.sprite()` loads from disk each call.

See [file-formats.md](file-formats.md) for the sprite file format.

## Coroutine Pattern

The `loop()` function runs as a Lua coroutine. The engine calls `lua_resume()` each frame with `dt` as the argument. Your code must end with:

```lua
function loop(dt)
    -- game logic...
    dt = yield()   -- suspend until next frame
end
```

Capture the returned `dt` for the next frame.

## Input Patterns

```lua
-- Edge-triggered (one action per press):
if input.keyPress("KEY_SPACE") then
    fire()
end

-- Continuous (held down):
if input.keyHeld("KEY_LEFT") then
    moveLeft()
end
```

For grid-based movement, use `keyPress` exclusively to step one cell per press.

## Screenshots

Press **F12** at any time during gameplay to save a screenshot. Screenshots are saved as PPM files in `screenshots/<game>_<timestamp>.ppm`.

## Mono Mode

The engine can run in mono mode (pass `-m` flag, or automatically on grayscale displays). Game logic can control per-element mono rendering by passing a `monoMode` parameter to draw calls:

- **0** (default): auto — luminance-based quantization
- **1** force white — always renders as white
- **2** force black — always renders as black

This is useful for UI overlays that need to remain readable regardless of the background.

`render.mapColor()` and `render.mapColorRange()` allow overriding how specific colors map to gray levels for your game's color palette.

## Performance

- **Sprite loading:** `render.sprite()` reads from disk each call. For many sprites per frame, load data in `setup()` and draw with `pixel()`/`fillRect()`.
- **Framerate:** 10-15 FPS is typical. Higher FPS increases CPU usage.
- **Primitives:** `fillRect` is faster than many `pixel()` calls. Batch your drawing.
- **Memory:** Virtual framebuffer is ~32 KB. The engine footprint is tiny.

## Error Handling

Lua errors in `setup()`, `loop()`, and `shutdown()` are caught by the engine and printed to stderr. The game exits to the main menu on error.
