@echo off
rem Generates the game code from your own ROM (rom\indy.z64): N64Recomp writes indy64recomp\RecompiledFuncs,
rem RSPRecomp the audio microcode indy64recomp\rsp\f5_audio_ucode.cpp. Both are derived from the game and must not be
rem shared. Needs N64Recomp.exe and RSPRecomp.exe from tools\build_n64recomp_runtime.cmd. See BUILDING.md.
setlocal
set ROOT=%~dp0..
set BIN=%ROOT%\third_party\N64Recomp-runtime-build
python -I "%ROOT%\tools\check_rom.py" || exit /b 1
pushd "%ROOT%\recomp" || exit /b 1
"%BIN%\N64Recomp.exe" indy64.recomp.toml || (popd & exit /b 1)
popd
if not exist "%ROOT%\indy64recomp\rsp" mkdir "%ROOT%\indy64recomp\rsp"
rem RSPRecomp needs the absolute path of its configuration.
pushd "%ROOT%\recomp\rsp" || exit /b 1
"%BIN%\RSPRecomp.exe" "%CD%\f5_audio.rsp.toml" || (popd & exit /b 1)
popd
python -I "%ROOT%\tools\fix_rsp_output.py" "%ROOT%\indy64recomp\rsp\f5_audio_ucode.cpp" || exit /b 1
echo Game code generated.
