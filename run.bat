@echo off
REM Run the built app with Qt's DLLs on PATH.
REM   run.bat              -> launch normally (tray-resident)
REM   run.bat --selftest   -> headless smoke check (comes up, then exits 0)
set "PATH=C:\Qt\6.11.1\msvc2022_64\bin;%PATH%"
REM Qt suppresses stderr logs when stdio is piped (no console window); force them.
set "QT_FORCE_STDERR_LOGGING=1"
"%~dp0build\ClipKeeper.exe" %*
