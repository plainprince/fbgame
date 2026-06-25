# File Formats

## Sprite Format (.spr)

Plain text file with a header line followed by one line per pixel:

```
5 5
255 0 0 255
0 255 0 255
0 0 255 255
...
```

**Format:**
- Line 1: `width height` (space-separated integers)
- Following lines: `r g b a` (space-separated integers, 0–255)
- Total pixel lines must equal `width * height`
- Pixels are row-major (left-to-right, top-to-bottom)
- Alpha = 0 → transparent (skipped during rendering)

**Example (5×5 red block):**
```
5 5
255 0 0 255  255 0 0 255  255 0 0 255  255 0 0 255  255 0 0 255
255 0 0 255  200 0 0 255  200 0 0 255  200 0 0 255  255 0 0 255
255 0 0 255  200 0 0 255  255 0 0 255  200 0 0 255  255 0 0 255
255 0 0 255  200 0 0 255  200 0 0 255  200 0 0 255  255 0 0 255
255 0 0 255  255 0 0 255  255 0 0 255  255 0 0 255  255 0 0 255
```

Sprite files are conventionally placed in `games/<name>/sprites/` and loaded via `render.sprite()` (see [lua-api.md](lua-api.md) for the sprite path caveat).

---

## Font Format

Fonts consist of two files in a `fonts/<name>/` directory.

### font.properties

```properties
font_name = "Monogram"
font_license = "MIT"
font_chars = " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.,!?-+=:;/'()"
char_width = 5
char_height = 7
char_gap = 1           # Space between characters (optional, default 1)
top_gap = 1            # Space above text line (optional, default 1)
bottom_gap = 0         # Space below text line (optional, default 0)
wrap_mode = none       # Default wrap: "none", "word", or "char" (optional)
```

**Keys:**
| Key | Required | Default | Description |
|-----|----------|---------|-------------|
| `font_name` | no | — | Display name for the font |
| `font_license` | no | — | License information |
| `font_chars` | **yes** | — | String of characters in glyph order (must match `font.data`) |
| `char_width` | **yes** | — | Width of each character cell in pixels |
| `char_height` | **yes** | — | Height of each character cell in pixels |
| `char_gap` | no | 1 | Horizontal space between characters |
| `top_gap` | no | 1 | Vertical space above text line |
| `bottom_gap` | no | 0 | Vertical space below text line |
| `wrap_mode` | no | `none` | Default text wrapping mode |

### font.data

ASCII-art bitmap for each character in `font_chars` order. Each character occupies `char_height` rows. `#` and `@` are filled pixels; `.` and space are empty.

**Example — letter 'A' (5×7):**
```
.#####.
##...##
##...##
.#####.
....##.
....##.
....##.
```

For a font with `font_chars = " ABCDEFGHIJKLMNOPQRSTUVWXYZ"` and `char_height = 7`, the file contains `chars × 7 = 26 × 7 = 182` data rows. Characters appear in the same order as the `font_chars` string.

`actualWidth` is auto-calculated per glyph as the rightmost filled column, enabling tighter kerning without manual configuration.

---

## Theme Format (.theme)

Properties file with hex RGB/RGBA color values:

```properties
name = "Retro Terminal"
primary = "00FF41"
background = "0D0D0D"
text = "CCCCCC"
accent = "FF4488"
```

**Format:**
- Standard `key = value` properties (see config format below)
- Color values: 6 hex chars (RGB) or 8 hex chars (RGBA)
- Quotes optional: `primary = "00FF41"` ≡ `primary = 00FF41`
- Case-insensitive hex: `FF`, `ff`, `Ff` all valid

### Default Theme Colors

| Key | Default | Purpose |
|-----|---------|---------|
| `primary` | `#4488FF` | Headings, titles |
| `secondary` | `#8844FF` | Secondary headings |
| `background` | `#111111` | Canvas background |
| `surface` | `#222222` | Panel background |
| `text` | `#EEEEEE` | Body text |
| `text_dim` | `#888888` | Dim/muted text |
| `accent` | `#FF4488` | Highlights, selection |
| `success` | `#44FF88` | Positive states |
| `error` | `#FF4444` | Error states |
| `warning` | `#FFC844` | Warning states |
| `button` | `#333333` | Button background |
| `button_hover` | `#444444` | Button hover state |
| `border` | `#444444` | Borders |

Theme files live in `themes/` and are selectable from the Settings menu.

---

## Save System

### Storage

- Base path: `saves/`
- Per-game subdirectory: derived from `game_namespace` in `config.properties`
- File path: `saves/<namespace>/<filename>.txt`
- All files are plain text

### Behavior

- `save.read("filename")` → returns file contents as string, or `""` if file doesn't exist
- First read loads from disk and caches in memory; subsequent reads serve from cache
- `save.write("filename", "data")` → enqueues write to a background worker thread
- `save.flush()` → drains the write queue synchronously (called automatically on game exit)
- Save data persists across game sessions

### Settings Storage

Engine settings (orientation, font name, spacing, theme) are stored in `saves/app/settings.txt` using the properties format.

---

## Config Format (.properties)

Used by `config.properties` files (engine root and per-game), `font.properties`, and theme files.

```
key = value
key = "quoted value"
# This is a comment
flag = true
number = 42
```

**Rules:**
- One `key = value` per line
- Keys are case-sensitive
- Values with quotes have the quotes stripped during parsing
- Lines starting with `#` are comments
- Empty lines are ignored
- Leading/trailing whitespace is stripped from keys and values
- Values can be read as string, int, bool, or float

### Engine Defaults (`config.properties`)

```properties
orientation = h
fps = 30
```

### Game Config (`games/<name>/config.properties`)

```properties
game_name = "My Game"
game_namespace = "mygame"
fps = 15
orientation = v
```

| Key | Required | Default | Description |
|-----|----------|---------|-------------|
| `game_name` | no | directory name | Display name in the PLAY menu |
| `game_namespace` | no | directory name | Subdirectory under `saves/` for persistent data |
| `fps` | no | engine default | Target framerate for this game |
| `orientation` | no | engine default | `h` = 128×64 horizontal, `v` = 64×128 vertical |
