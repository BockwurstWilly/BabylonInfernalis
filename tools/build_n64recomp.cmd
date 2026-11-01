@echo off
rem Builds N64RecompCLI and RSPRecomp from third_party\N64Recomp with MSVC + Ninja (Release).
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set SRC=%~dp0..\third_party\N64Recomp
set BLD=%~dp0..\third_party\N64Recomp-build
cmake -S "%SRC%" -B "%BLD%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl || exit /b 1
cmake --build "%BLD%" --target N64RecompCLI RSPRecomp || exit /b 1
