# ClipKeeper (Qt)

Native **C++20 / Qt 6 (Widgets)** rewrite of the Python + CustomTkinter ClipKeeper,
a local-first Windows clipboard manager. The Python app in `../ClipKeeper` is the
behavioral spec and stays intact as reference. The rewrite exists to fix
CustomTkinter's rendering limits (O(n²) list, scroll stutter) with a natively
virtualized `QListView` + custom delegate.

## Stack
- C++20, Qt 6.11.1 (Widgets, Sql, Network), CMake + Ninja
- MSVC 2022 (Visual Studio 2026, toolset 14.51) + Windows 11 SDK 10.0.26100
- Native Win32 for OS integration (clipboard listener, hotkey, keyboard hook, DWM)

## Toolchain locations (this machine)
- Visual Studio 2026 Community — `C:\Program Files\Microsoft Visual Studio\18\Community`
- Qt for MSVC — `C:\Qt\6.11.1\msvc2022_64` (qtbase + qtsvg + qtimageformats)

> Qt-for-MSVC was fetched directly from download.qt.io (only qtbase is required
> for a Widgets app) because aqtinstall 3.3.0 can't parse Qt 6.11.1's nested repo
> layout. QML/docs/examples/debug-symbols were skipped → ~41 MB, not 1.5 GB.

## Build & run
```
build.bat            # configure + compile (Debug, Ninja, MSVC)
run.bat              # launch (tray-resident)
run.bat --selftest   # headless smoke check: comes up, quits, exit 0
```

## Data directory
Dev builds use `%LOCALAPPDATA%\ClipKeeperQt`, deliberately separate from the
Python app's `%LOCALAPPDATA%\ClipKeeper` so early builds can't corrupt live
history. Change `kDataFolder` in `src/config.cpp` for the final drop-in.

## Status
- [x] Skeleton — tray icon + menu + clean quit
- [ ] Phase 1 — capture + SQLite/FTS5 storage
- [ ] Phase 2 — popup (QListView + custom delegate, virtualized)
- [ ] Phase 3 — hotkey + Win+V low-level hook
- [ ] Phase 4 — tray + autostart + single-instance
- [ ] Phase 5 — Supabase sync
- [ ] Phase 6 — installer (windeployqt + Inno Setup)
