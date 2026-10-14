@echo off
setlocal
REM Release, windowed (no-console) build of ClipKeeper.
REM   build_release.bat            -> dev identity (ClipKeeperQt data dir + autostart)
REM   build_release.bat ON         -> drop-in identity (shared "ClipKeeper" names)

set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
set "QT_PREFIX=C:/Qt/6.11.1/msvc2022_64"
set "DROPIN=%1"
if "%DROPIN%"=="" set "DROPIN=OFF"

call "%VCVARS%" >nul || ( echo [build] vcvars64 failed & exit /b 1 )
set "PATH=C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;%PATH%"

cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCLIPKEEPER_CONSOLE=OFF -DCLIPKEEPER_DROP_IN=%DROPIN% ^
      -DCMAKE_PREFIX_PATH=%QT_PREFIX% -S "%~dp0." -B "%~dp0build-release"
if errorlevel 1 ( echo [build] configure failed & exit /b 1 )

cmake --build "%~dp0build-release" -j
if errorlevel 1 ( echo [build] compile failed & exit /b 1 )

echo.
echo [build-release] OK (DROP_IN=%DROPIN%) -^> "%~dp0build-release\ClipKeeper.exe"
