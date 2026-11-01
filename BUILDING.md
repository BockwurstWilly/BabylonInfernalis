# Building Babylon Infernalis

Windows 10/11 x64 only so far. The build turns your own ROM into C code on your machine; nothing derived from the game
is stored in this repository, so please do not commit or share the generated files or the finished program.

## 1. Requirements

| Tool | Notes |
|---|---|
| [Visual Studio 2022 Build Tools](https://visualstudio.microsoft.com/downloads/) | Workload "Desktop development with C++". The scripts expect the default path `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools`. |
| [LLVM](https://github.com/llvm/llvm-project/releases) (clang-cl, lld-link) | Installed to `C:\Program Files\LLVM`. |
| [CMake](https://cmake.org/download/) 3.20+ and [Ninja](https://github.com/ninja-build/ninja/releases) | On `PATH`. |
| [Python 3](https://www.python.org/downloads/) | On `PATH` as `python`. |
| [Git](https://git-scm.com/) | Used by `setup_deps.py` to apply our patches (GitHub Desktop's bundled git works too, pass it with `--git`). |
| [SDL2 2.30.9 development package for VC](https://github.com/libsdl-org/SDL/releases/tag/release-2.30.9) | `SDL2-devel-2.30.9-VC.zip`, extract so that `third_party\SDL2\include` and `third_party\SDL2\lib\x64` exist. |
| Your ROM | Indiana Jones and the Infernal Machine (USA), SHA-1 `93300d63412d6c7432109b5ad2e4a8b9348e9538`. |

## 2. Libraries

The libraries (N64ModernRuntime with N64Recomp, RT64, RecompFrontend, and their submodules) are downloaded at the
pinned commits from `deps/deps.json`, and our changes from `deps/patches/` are applied:

```
python tools/setup_deps.py
```

This needs about 450 MB and makes about 40 requests to GitHub's API (limit 60 per hour without a token; set
`GITHUB_TOKEN` if you hit it).

## 3. Recompilers

Builds `N64Recomp.exe` and `RSPRecomp.exe` from the N64Recomp copy inside N64ModernRuntime into
`third_party\N64Recomp-runtime-build`:

```
tools\build_n64recomp_runtime.cmd
```

## 4. Game code from your ROM

Copy your ROM to `rom\indy.z64` (a `.v64` or `.n64` dump is converted automatically), then:

```
tools\generate_code.cmd
```

This checks the ROM, writes the recompiled game code to `indy64recomp\RecompiledFuncs` and the recompiled audio
microcode to `indy64recomp\rsp\f5_audio_ucode.cpp`. RSPRecomp prints a few `Unhandled instruction: INVALID` lines;
these are data words inside the microcode and harmless.

## 5. Build

```
tools\build_indy64.cmd
```

The program is `indy64recomp\build\BabylonInfernalis.exe` (with `SDL2.dll`, `dxil.dll` and `dxcompiler.dll` next to
it). It finds `assets\` next to the exe or one folder up, so it can be started from the build folder. On the first
start it asks for the ROM once and keeps its own copy in `%LOCALAPPDATA%\Indy64Recompiled`.

## Changing the libraries

Edit the files in `indy64recomp\lib`, then rebuild the patch files:

```
python tools/make_dep_patches.py <path to git.exe>
```

It compares each library with its pinned original (the zips cached in `third_party\_dl` by `setup_deps.py`).

## Debugging aids

The program reads several environment variables meant for development, for example `INDY64_AUTOSTART=1` (start the
game without the launcher), `INDY64_AUTOINPUT="<seconds>:<button>,..."` (scripted input, also `REPORT` and `QUIT`),
`INDY64_AUDIO_RAW=<file>` (record the audio output) and `INDY64_AUDIO_HLE=1` (use mupen64plus-rsp-hle's MusyX code
instead of the recompiled microcode). They are documented where they are read (`indy64recomp/src/main`) and in
`NOTES.md`. `tools/test_bg.ps1` runs the game in the background with scripted input and takes screenshots.

The function names and addresses in `recomp/indy64.syms.toml` come from a Ghidra analysis of the game
(`tools/run_recomp.ps1`, `tools/ghidra/`); you only need that to add or rename functions.
