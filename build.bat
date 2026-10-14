@echo off
setlocal
REM Build ClipKeeper (Qt Widgets / MSVC). Loads the MSVC 2022 environment, then
REM configures + builds with Ninja against Qt 6.11.1 for MSVC.

set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
set "QT_PREFIX=C:/Qt/6.11.1/msvc2022_64"

call "%VCVARS%" >nul
if errorlevel 1 ( echo [build] vcvars64 failed & exit /b 1 )

REM Prefer Qt's bundled CMake + Ninja (matched to this Qt).
set "PATH=C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;%PATH%"

cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=%QT_PREFIX% -S "%~dp0." -B "%~dp0build"
if errorlevel 1 ( echo [build] configure failed & exit /b 1 )

cmake --build "%~dp0build" -j
if errorlevel 1 ( echo [build] compile failed & exit /b 1 )

echo.
echo [build] OK -^> "%~dp0build\ClipKeeper.exe"
