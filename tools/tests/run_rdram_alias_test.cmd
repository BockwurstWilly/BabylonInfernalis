@echo off
rem Builds and runs rdram_alias_test.cpp (checks the Indy64 rdram view mapping used by librecomp).
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cd /d "%~dp0"
cl /nologo /std:c++20 /EHsc /O2 rdram_alias_test.cpp || exit /b 1
rdram_alias_test.exe
