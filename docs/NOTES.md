# Technical notes

What was learned while porting Icy Tower 1.3.1 and why the code is shaped the way it is.

## 1. The original's files

The installer `icytower13_install.exe` (Inno Setup, SHA-1 `a00aa6eb…389a`) unpacks with
`innoextract` to `app/`. The exe is UPX packed and was built with **Allegro 4.0.3 +
AllegroOGG, MinGW32**. Its strings (menu labels, messages, file names) are readable
after `upx -d`.

| file | content |
|---|---|
| `data/data.dat` | Allegro datafile, **packed + encrypted**, password `gostflor`: 90 `BMP ` objects, `AAAPAL`, fonts `FONT1..3` |
| `data/sfx13.dat` | Allegro datafile, encrypted (not packed): 22 `OGG ` objects (`S_GOOD`, `S_BG_BEAT`, ...) |
| `characters/<name>/<name>.txt` | character script (`[datafile]` or `[frames]` + sound keys) |
| `characters/*/harold.dat` | plain datafile: `000_PAL`, `001_BMP`..`015_BMP` frames, `016`..`023` OGG sounds |
| `replays/*.itr` | ITR130 replays |

### Allegro 4 datafile

- The new style encryption XORs **every byte of the file, the magic number included**,
  with the password repeated. After decrypting, the first long is `slh!` (LZSS packed)
  or `slh.` (not packed), then `ALL.` and the object count.
- Objects: optional `prop` chunks (`NAME`, `ORIG`, `DATE`...), a type id, then
  `file size` and `data size`. A negative data size means the object is LZSS packed.
- LZSS: 4096 byte window, 18 byte matches, the window starts at `N-F`
  (`src/datafile.c`).
- `BMP `: `bits, w, h` as big-endian shorts, then w×h bytes for 8 bit.
- `PAL `: 256 × 4 bytes, 6-bit VGA values.
- `FONT`: a leading short 0 means the "new" format: range count, then per range
  `depth (0 = 8 bit colour, 1 = mono), first, last`, then per glyph `w, h` and pixels
  (colour) or `((w+7)/8)*h` bits (mono). FONT1/FONT2 are colour fonts, FONT3 is mono.
- Character datafile sounds `016..023` are in this order: jumplo, jumpmed, jumphi,
  greeting, pause, death, edge, bgmusic.

### Palette

The game runs in 256 colours, and **the palette of the selected character is the game
palette** (log: `getting palette from <(1) harold_the_homeboy>`). `AAAPAL` from
data.dat differs from Harold's palette in indices 1–15, 44–47, 88–98 and 142–151.
Because of that, FONT2 looks like a rainbow font with `AAAPAL` but like **white text
with a dark outline** with the game palette. So no colour map is needed for the white
texts.

### `[frames]` character sheets

The sheet has the 15 frames in datafile order (IDLE1, WALK1–4, JUMP1–3, JUMP, IDLE2,
IDLE3, CHOCK, ROTATE, EDGE2, EDGE1) on a background of colour **255**. Take the first
row that has a non-255 pixel, and every run of non-255 pixels on that row is a frame. It
extends downwards until it hits 255. This reproduces the datafile frame sizes of Harold
exactly (30×52, 29×52 …, 44×60 for ROTATE). Index 0 is transparent inside the frames.

## 2. The engine and replays

`src/core.c` is RaMMicHaeL's engine from replay_checker, operation for operation.
It covers physics, scrolling speed, the floor generator (MSVC `rand`), combos and score.
The floor ring is 16 deep and indexed by level instead of the original 7-slot ring. The
two are equivalent, but the deeper ring also keeps the floor below the screen for
drawing.

- 50 ticks per second. Every 1500 ticks of scrolling the speed goes up (max 5) → HURRY UP.
- Score = 10 × floor + Σ combo², where a combo needs jumps of ≥ 2 floors within the
  100-tick combo timer.
- ITR130: 98 byte header (name/date 31+1 chars, score, floor, combo, rejump, seed, hash),
  then `{int frames; uchar keys}` macros. A macro holds `frames+1` ticks. Keys: left 1,
  right 2, jump 0x10. The last macro has 0x80 set. The hash uses **signed** chars
  (x86), so build with explicit `signed char` (ARM chars are unsigned).
- Build with `-ffp-contract=off`: fused multiply-add would round differently from the
  original, and replays would drift.
- 11 of the 15 example replays reproduce score/floor/combo exactly on ARM64 and x86-64.
  The other 4 fail the same way in the reference checker (probably recorded with a beta
  build).

Tests: `tests/replay_test.c`, plus `ICYTOWER_AUTOPLAY=file.itr`, which feeds a replay's
inputs into a *normal* game. The scene code then has to reach the same result, and the
screens after a good game can be driven headless.

## 3. Things measured on the original (Wine screenshots)

Title screen (640×480 coordinates):

- `TITLE_BG` at 0,0. `TITLE` at 250,20. The menu items use FONT1 at x=40,
  y=270+28·i. The bullet is at 40−w, 262+28·i.
- "HIGHSCORES" is FONT2 centred on x=510, y=240. `HISCTOP` is at 370,264.
- Highscore scroller: BEST SCORES / BEST FLOORS / BEST COMBOS. Each section header is
  centred on 510, the first row is 31 px below it, rows are 21 px apart, and the next
  header follows 44 px after the last row. Columns: name at x=370, floor right-aligned
  at 480, combo at 540, score at 630. It scrolls 1 px every 2 ticks and is clipped to
  y 280..448.
- The default tables are `FLD 58/18/904, 47/15/695, 35/11/471, 23/7/279, 11/3/119`
  (score = 10·floor + combo²).
- Bottom strip: rows 448, 449 and 450+ of the title are darkened with Allegro light
  tables (levels 220, 148 and 92 towards rgb 0,2,2). The marquee is FONT2 at y=450 and
  moves 2 px/tick.
- "v1.3.1": FONT3 at 605,2 in colour (109,81,85), with a black shadow at +1,+1.

Found by matching sprites and glyphs against the screenshots (fontfind-style search:
render the text with the real font and palette, then minimise the pixel error).

## 4. Vita specific

- Rendering is an 8-bit software renderer (like Allegro). On the Vita the frame goes
  unchanged into a **P8 GXM texture** (vita2d), and the GPU does the palette lookup.
  Fades just scale the 256 palette entries. Three textures are used in turn because
  the texture memory is written while earlier frames may still be in flight.
- "Sharp" mode: the CPU doubles every pixel into a 1280×960 P8 texture, and the GPU
  filters that down to 725×544. Do not use a vita2d render target for this: vita2d
  keeps the 960×544 screen projection while drawing into it, so a 2× image runs off
  the target's edges, and only one corner ends up on screen (the bug in the first
  vita2d build).
- 50 Hz logic on a 60 Hz screen: fixed step accumulator. Drawing interpolates the
  scroll position, Harold, the stars, the spin angle and the hurry-up banner between
  the last two ticks.
- On the host, a frame of drawing + conversion takes about 0.19 ms on average (0.6 ms
  at worst) in a combo-heavy replay (`ICYTOWER_BENCH=1`). On the Vita the conversion
  step is gone (P8).
- Gameplay uses the physical buttons (cross/square/triangle jump, circle escapes), so
  consoles where circle confirms still play the same. The menus follow the system
  X/O setting.
