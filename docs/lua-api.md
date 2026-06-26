# Lua API Reference

## Required Game Functions

Every game **must** define these three functions:

```lua
function setup()
    -- Called once when the game is loaded.
end

function loop(dt)
    -- Called every frame. dt = delta time in seconds (float).
    dt = yield()  -- MUST call at end to suspend until next frame
end

function shutdown()
    -- Called when the game exits.
end
```

**Critical:** `loop(dt)` is a coroutine. You **must** call `dt = yield()` at the end of each frame iteration. Without this, the loop consumes 100% CPU.

---

## render.* — Drawing

All render functions accept an optional `monoMode` parameter (0=auto/luminance, 1=force white, 2=force black). This overrides how the pixel is rendered on mono displays.

| Function | Description |
|----------|-------------|
| `render.pixel(x, y, color [, monoMode])` | Single pixel at (x, y). |
| `render.fillRect(x, y, w, h, color [, monoMode])` | Filled rectangle. |
| `render.drawRect(x, y, w, h, color [, monoMode])` | Rectangle outline (1px border). |
| `render.fillCircle(cx, cy, r, color [, monoMode])` | Filled circle (integer coords). |
| `render.drawCircle(cx, cy, r, color [, monoMode])` | Circle outline. |
| `render.line(x1, y1, x2, y2, color [, monoMode])` | Bresenham line. |
| `render.text(x, y, text, color [, maxW [, wrapMode [, centered [, monoMode]]]])` | Render text. `maxW` enables wrapping: `wrapMode` 0=None (clips), 1=Word, 2=Char. `centered` centers text within `maxW`. |
| `render.sprite(x, y, path [, monoMode])` | Load and draw sprite from CWD-relative path (e.g. `"games/yourgame/spr.spr"`). |
| `render.clear(color [, monoMode])` | Clear entire virtual canvas. |
| `render.getWidth()` → int | Virtual canvas width (128 horizontal, 64 vertical). |
| `render.getHeight()` → int | Virtual canvas height (64 horizontal, 128 vertical). |
| `render.setFPS(n)` | Override target framerate at runtime. |
| `render.setOrientation("h" \| "v")` | Switch orientation (clears canvas). |
| `render.mapColor(sourceColor, gray)` | Map a specific packed color to a gray level (0–255) for mono rendering. |
| `render.mapColorRange(rMin, rMax, gMin, gMax, bMin, bMax, gray)` | Map a range of colors to a gray level. All 7 parameters are 0–255. |

### `mapColor` / `mapColorRange` Caveat

These functions only affect rendering when the engine is in mono mode (>2 colors). They register lookup-table entries that override the default luminance calculation for specific source colors or color ranges.

### Sprite Path Caveat

`render.sprite()` does **not** auto-prefix the game directory path. Unlike `audio.play()` which is relative to your game directory, sprite paths are resolved from the engine's working directory. Use the full relative path:

```lua
render.sprite(x, y, "games/yourgame/sprites/block.spr")
```

---

## input.* — Keyboard / Gamepad

**Key name strings:**

```
KEY_A..KEY_Z        KEY_0..KEY_9         KEY_SPACE    KEY_ENTER
KEY_ESCAPE          KEY_UP/LEFT/DOWN/RIGHT
KEY_TAB             KEY_BACKSPACE        KEY_SHIFT
KEY_CTRL            KEY_ALT              KEY_F12
BUTTON_A..BUTTON_Y  BUTTON_START         BUTTON_SELECT
BUTTON_L1           BUTTON_R1            BUTTON_L2     BUTTON_R2
```

| Function | Description |
|----------|-------------|
| `input.keyHeld("KEY_SPACE")` → bool | True while key is held (continuous). |
| `input.keyPress("KEY_A")` → bool | True only on the frame key transitions up→down. **One-shot — returns true once per press, then clears.** Only the first call per frame returns true. |

**Note:** `KEY_SHIFT`, `KEY_CTRL`, `KEY_ALT` only work via evdev (USB gamepad/keyboard), not stdin. `KEY_F12` triggers screenshot capture in the engine.

---

## save.* — Persistent Storage

Data stored as text files under `saves/<namespace>/`. Namespace comes from `game_namespace` in `config.properties`.

| Function | Description |
|----------|-------------|
| `save.read("filename")` → string | Read saved string. Returns `""` if file doesn't exist. Cached in memory after first read. |
| `save.write("filename", "data")` | Write a string. **Async** — queued to background thread, flushed on game exit. |

---

## audio.* — Sound

Audio has two independent channels: SFX (one-shot) and Music (looped). Neither blocks the main thread.

| Function | Description |
|----------|-------------|
| `audio.play("path.wav" [, loop])` | Play a WAV file. If `loop=true`, plays as looped music (stops current music); otherwise plays as one-shot SFX. |
| `audio.playSfx("path.wav")` | Play a one-shot SFX. Interrupts previous SFX. Returns immediately. |
| `audio.playMusic("path.wav")` | Play looped background music. Stops previous music. Returns immediately. |
| `audio.stopMusic()` | Stop background music playback. |
| `audio.stopAll()` | Stop all audio (SFX + music). |
| `audio.pauseAll()` | Pause all audio playback. |
| `audio.resumeAll()` | Resume all audio playback. |
| `audio.setVolume(0.0–1.0)` | Set master volume. |
| `audio.getVolume()` → float | Get current master volume (0.0–1.0). |

Paths are **relative to the game directory** (e.g. `"beep.wav"` or `"sounds/explode.wav"`).

---

## theme.* — Theming

| Function | Description |
|----------|-------------|
| `theme.get("color_name")` → int | Returns packed 32-bit color from active `.theme` file. Falls back to white if missing. |

Default theme keys: `primary`, `secondary`, `background`, `surface`, `text`, `text_dim`, `accent`, `success`, `error`, `warning`, `button`, `button_hover`, `border`.

---

## menu.* — Overlay Menu

Creates a scrollable menu overlay within the game (pause screens, level select, etc.).

```lua
menu.create("Title", {"Option 1", "Option 2", "Option 3"})
local choice = 0
while choice == 0 do
    choice = menu.tick()   -- handles input AND rendering
    if choice == 0 then dt = yield() end
end
-- choice > 0: index of selected item (1-based)
-- choice < 0: ESC pressed (cancel)
```

Rendered with current theme colors. Up/Down or W/S to navigate, Enter/Space to select, Escape to cancel.

---

## Color() Constructor

```lua
Color(r, g, b)        -- packed 32-bit int, alpha=255
Color(r, g, b, a)     -- packed 32-bit int, alpha 0-255
```

Returns a packed integer usable anywhere a color argument is expected.

---

## Color Constants (predefined globals)

```
COLOR_BLACK    COLOR_WHITE    COLOR_RED      COLOR_GREEN
COLOR_BLUE     COLOR_YELLOW   COLOR_CYAN     COLOR_MAGENTA
COLOR_ORANGE   COLOR_PURPLE   COLOR_GREY
COLOR_PRIMARY  COLOR_TEXT_DIM COLOR_ACCENT
```

---

## Other Globals

| Function | Description |
|----------|-------------|
| `shutdown()` | Request clean game exit (calls game's `shutdown()`, returns to menu). |
| `quit()` | Alias for `shutdown()`. |
| `yield()` | Suspend Lua coroutine until next frame. **Must be called at end of each `loop()` iteration.** |
| `math`, `table`, `string` | Standard Lua 5.4 libraries. |
| `os` | Standard Lua 5.4 `os` library (`os.clock()`, `os.time()`, etc.). |
| `io` | Standard Lua 5.4 `io` library (use save system instead where possible). |
