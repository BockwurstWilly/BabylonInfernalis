# Babylon Infernalis

This project was made with extensive help from Claude Code, an AI assistant by Anthropic.

A native Windows port of **Indiana Jones and the Infernal Machine** for the Nintendo 64, made with
[N64: Recompiled](https://github.com/N64Recomp/N64Recomp). The game's MIPS code is statically recompiled to C and runs
natively; graphics are drawn by [RT64](https://github.com/rt64/rt64).

*Deutsche Fassung: [README.de.md](README.de.md)*

> **No game data is included.** This repository contains only the port's own code, configuration and tools. To build
> and play it you need your own ROM of the USA version (see [BUILDING.md](BUILDING.md)). Please do not ask for ROMs
> or prebuilt binaries.

This is an unofficial fan project, not affiliated with or endorsed by Lucasfilm, LucasArts, Disney, Factor 5 or
Nintendo. "Indiana Jones" is a trademark of Lucasfilm Ltd.

## Status: v0.1.0 (preview)

The game starts and plays with music, sound effects and speech. The first level (Canyonlands) was played by hand at
length; all 16 levels were loaded in an automated test without crashing. The game has **not** been played through
from start to finish yet. See [CHANGELOG.md](CHANGELOG.md) for what works, what
was done and the known issues.

- Windows 10/11, x64, Direct3D 12 or Vulkan.
- Tested with the **Nintendo Switch Online N64 controller** (Bluetooth), which has its own button mapping built in.
  Other controllers and the keyboard use the default mapping of the frontend and can be remapped in *Controls*.
- 4:3 and the original frame rate only (see known issues).

## What makes this game special

Factor 5 wrote their own RSP microcode for graphics and audio, which is why the game was long considered hard to
port or emulate. This port needed:

- **Graphics:** an RT64 implementation of the Factor 5 graphics microcode, ported from
  [GLideN64](https://github.com/gonetz/GLideN64)'s `F5Indi_Naboo` (decoding by olivieryuyu).
- **Audio:** the MusyX audio microcode, recompiled with RSPRecomp (including its runtime overlays).
- **Runtime:** support for the game's virtual memory window at `0x41000000`, which it normally pages in through the
  TLB, plus several fixes to the N64ModernRuntime (thread scheduling, PI DMA messages, audio interrupt pacing).

The research notes (German) are in [NOTES.md](NOTES.md): memory layout, microcode analysis, every bug and its cause.

## Bug reports

Press **F9** (or the *Capture* button on the Switch Online N64 controller) while playing. The program saves a report
in `bugreports\<date_time>\` next to the exe: screenshot, RAM dump, the next display list, session log and a
`notiz.txt` for a short description. Each session also writes a log to `logs\`.

## Repository layout

| Path | Contents |
|---|---|
| `indy64recomp/` | The port: host program (`src/main`), game setup (`src/game`), launcher assets, CMake project |
| `recomp/` | N64Recomp configuration and symbol list for the game, RSPRecomp configuration for the audio microcode |
| `deps/` | Pinned versions of the libraries used and our changes to them as patches |
| `tools/` | Build and setup scripts, plus the analysis and test tools used during development |
| `NOTES.md` | Development notes (German) |

## Credits

- [N64: Recompiled](https://github.com/N64Recomp/N64Recomp), N64ModernRuntime and RecompFrontend by Wiseguy and
  contributors; the project structure follows the N64: Recompiled project template (Zelda64Recomp, Harvest Moon 64:
  Recompiled).
- [RT64](https://github.com/rt64/rt64) by Dario and contributors.
- [GLideN64](https://github.com/gonetz/GLideN64) by Sergey Lipskiy and contributors; Factor 5 microcode decoding by
  olivieryuyu.
- [mupen64plus-rsp-hle](https://github.com/mupen64plus/mupen64plus-rsp-hle) (MusyX v2 reference, optional).
- [BizHawk](https://github.com/TASEmulators/BizHawk) / ares, used as reference during development.

Full list of components and licenses: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## License

The port's own code is licensed under the [GNU GPL v3.0](LICENSE). Parts come from projects under other licenses,
see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
