@echo off
rem Builds BabylonInfernalis (indy64recomp\) with clang-cl + Ninja in the VS 2022 build tools environment.
rem Usage: tools\build_indy64.cmd [configure]   - "configure" only runs CMake.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
rem RT64 preprocesses shader headers with a plain "clang" call.
set PATH=C:\Program Files\LLVM\bin;%PATH%
set SRC=%~dp0..\indy64recomp
set BLD=%~dp0..\indy64recomp\build
cmake -S "%SRC%" -B "%BLD%" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
    -DCMAKE_C_COMPILER="C:/Program Files/LLVM/bin/clang-cl.exe" -DCMAKE_CXX_COMPILER="C:/Program Files/LLVM/bin/clang-cl.exe" ^
    -DCMAKE_LINKER="C:/Program Files/LLVM/bin/lld-link.exe" || exit /b 1
if "%1"=="configure" exit /b 0
cmake --build "%BLD%" --target BabylonInfernalis || exit /b 1
