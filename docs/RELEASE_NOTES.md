A from-scratch engine port of **Icy Tower 1.3.1** for the PS Vita, made by İbrahim Doğan. It plays with the
game's own graphics, sounds and music. They come from the original installer of the free PC game, and the VPK
contains none of them.

## New in 1.0.1

- **Setup is now one file:** copy the original `icytower13_install.exe` to `ux0:data/icytower/`. The game
  checks it (SHA-1) and unpacks the game files itself on the first start. No PC setup is needed anymore.
- Unpacking never overwrites files you already have. Copying `data/` and `characters/` by hand still works.
- The missing files screen and `log.txt` say what is wrong, e.g. when the .exe is not the 1.3.1 installer.

## Install

1. Install `icytower.vpk` (below) with VitaShell on a Vita running HENkaku / Ensō.
2. Download the free Icy Tower 1.3.1 installer
   [`icytower13_install.exe`](https://archive.org/download/Icy_Tower/icytower13.rar/icytower13_install.exe)
   (archive.org, 2,647,172 bytes, SHA-1 `a00aa6ebc4c37fac7c44de91671477ef7e32389a`).
3. Copy it to **`ux0:data/icytower/`**:

```
ux0:data/icytower/
└── icytower13_install.exe
```

4. Start Icy Tower. The first start unpacks `data/`, `characters/` and `replays/` next to the exe. After that you
   can delete the exe.

## Highlights

- The 1.3.1 engine frame for frame: physics, floors, combos and score. Logic runs at 50 Hz and is
  interpolated for 60 Hz.
- Replays compatible with the PC game (`.itr`): watch, save, and browse `replays/` with fast forward.
- Combos and the reward shouts, the hurry-up clock, stars, floor signs, the eleven floor types,
  unlockable start floors and the eye candy levels.
- The three highscore tables with initials entry.
- Custom characters in the original 1.3 format (`[datafile]` or `[frames]` + WAV/OGG), and custom
  `sfx/*.wav` sounds.
- Screen modes: Sharp, Smooth, Wide and 1:1.

## Controls

D-pad / stick to run, Cross / Square / Triangle to jump, Start to pause, Circle to quit.

## Problems?

The log file is `ux0:data/icytower/log.txt`, rewritten on every start. Please attach it when you open an issue.

Icy Tower belongs to Free Lunch Design. PlayStation and PS Vita are trademarks of Sony Interactive
Entertainment. This project is not affiliated with either.
