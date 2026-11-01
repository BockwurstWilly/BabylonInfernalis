# Third-party components and licenses

Babylon Infernalis itself is licensed under the GNU General Public License v3.0 ([LICENSE](LICENSE)). It is built
from the components below. Libraries marked *fetched* are not stored in this repository; `tools/setup_deps.py`
downloads them at the commits in `deps/deps.json` and applies our changes from `deps/patches/`, which are published
under the license of the respective library.

| Component | Where | License |
|---|---|---|
| [N64Recomp](https://github.com/N64Recomp/N64Recomp) (recompiler, `recomp.h`) | fetched, patched | MIT |
| [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime) (librecomp, ultramodern) | fetched, patched | GPL-3.0 |
| [RecompFrontend](https://github.com/N64Recomp/RecompFrontend) (menus, input) | fetched, patched | GPL-3.0 |
| [RT64](https://github.com/rt64/rt64) (renderer) and its submodules | fetched, patched | MIT (submodules: see their repositories) |
| Factor 5 microcode port in RT64 (`src/gbi/rt64_gbi_f5indi.cpp`, in `deps/patches/rt64.patch`) | patch | GPL-2.0, derived from [GLideN64](https://github.com/gonetz/GLideN64) `src/uCodes/F5Indi_Naboo.cpp` (Sergey Lipskiy and contributors; microcode decoding by olivieryuyu) |
| N64: Recompiled project template (window and audio setup in `indy64recomp/src/main/main.cpp`, CMake setup, `indy64recomp/patches/*.h`) | in repository | GPL-3.0 ([Zelda64Recomp](https://github.com/Zelda64Recomp/Zelda64Recomp), [Harvest Moon 64: Recompiled](https://github.com/HarvestMoon64Recomp/HarvestMoon64Recomp)) |
| [mupen64plus-rsp-hle](https://github.com/mupen64plus/mupen64plus-rsp-hle) MusyX code (`indy64recomp/src/audio_hle/`) | in repository | GPL-2.0-or-later ([LICENSES](indy64recomp/src/audio_hle/LICENSES)) |
| [SlotMap](https://github.com/SergeyMakeev/SlotMap) (`indy64recomp/lib/SlotMap`) | in repository | MIT |
| [SDL_GameControllerDB](https://github.com/gabomdq/SDL_GameControllerDB) (`indy64recomp/recompcontrollerdb.txt`) | in repository | zlib |
| [Inter](https://github.com/rsms/inter) font (`indy64recomp/assets/InterVariable.ttf`) | in repository | SIL Open Font License 1.1 |
| [Noto Emoji](https://github.com/googlefonts/noto-emoji) font (`indy64recomp/assets/NotoEmoji-Regular.ttf`) | in repository | SIL Open Font License 1.1 |
| [PromptFont](https://shinmera.github.io/promptfont/) (`indy64recomp/assets/promptfont/`) | in repository | SIL Open Font License 1.1 ([LICENSE](indy64recomp/assets/promptfont/LICENSE.txt)) |
| [SDL2](https://www.libsdl.org/) 2.30.9 | downloaded by the user | zlib |

The launcher picture, title lettering and app icon are original work made by `tools/make_branding.py` for this
project (GPL-3.0).

## Note on GLideN64

GLideN64 states that it is licensed under "the GNU General Public License version 2" without "or later". A program
that combines this GPL-2.0 code with the GPL-3.0 parts above may therefore not be distributable as a binary. This
repository only publishes source code and does not distribute binaries. Binaries would require permission from the
GLideN64 authors to use the Factor 5 microcode port under GPL-2.0-or-later (or GPL-3.0); that has not been asked for
yet.

## Game

Indiana Jones and the Infernal Machine is © Lucasfilm Ltd. / LucasArts, developed for the Nintendo 64 by Factor 5.
No part of the game (code, graphics, audio, ROM) is included in this repository.
