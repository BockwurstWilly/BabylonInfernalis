@echo off
rem Builds N64RecompCLI and RSPRecomp from the N64Recomp submodule of indy64recomp's N64ModernRuntime (MSVC + Ninja),
rem so the generated code matches that runtime's recomp.h.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set SRC=%~dp0..\indy64recomp\lib\N64ModernRuntime\N64Recomp
set BLD=%~dp0..\third_party\N64Recomp-runtime-build
cmake -S "%SRC%" -B "%BLD%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl || exit /b 1
cmake --build "%BLD%" --target N64RecompCLI RSPRecomp || exit /b 1
