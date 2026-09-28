A from-scratch engine port of **Icy Tower 1.3.1** for the PS Vita, made by İbrahim Doğan. It plays with the
game's own graphics, sounds and music, read from your copy of the free PC game. The VPK contains none of them.

## Install

1. Install `icytower.vpk` (below) with VitaShell on a Vita running HENkaku / Ensō.
2. Get the free **Icy Tower 1.3.1** installer `icytower13_install.exe`
   ([archive.org](https://archive.org/details/Icy_Tower)). Install it on Windows, or run
   `tools/prepare_data.sh icytower13_install.exe` from the repository on macOS / Linux.
3. Copy `data/`, `characters/` and (optionally) `replays/` into **`ux0:data/icytower/`**:

```
ux0:data/icytower/
├── data/data.dat
├── data/sfx13.dat
├── characters/            the whole folder (harold_the_homeboy/, disco_dave/, template/)
└── replays/               optional
```

`ux0:data/icytower/data/data.dat` must exist. The files must come from version 1.3.1.

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
