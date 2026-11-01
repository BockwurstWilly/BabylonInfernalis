@echo off
rem Builds RT64 (third_party\rt64, with the Factor 5 GBI port) as a static library with MSVC + Ninja.
rem Usage: tools\build_rt64.cmd [configure]   - "configure" only runs CMake.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set SRC=%~dp0..\third_party\rt64
set BLD=%~dp0..\third_party\rt64-build
cmake -S "%SRC%" -B "%BLD%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DRT64_STATIC=ON -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl || exit /b 1
if "%1"=="configure" exit /b 0
cmake --build "%BLD%" --target rt64 || exit /b 1
