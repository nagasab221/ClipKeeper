#pragma once

#include <QString>

// Launch ClipKeeper at logon via the per-user Run key (HKCU\...\CurrentVersion\
// Run) — port of autostart.py. Per-user, no admin, and removing the value fully
// undoes it. Off by default.
namespace autostart {
bool isEnabled();
QString launchCommand();
bool enable();
bool disable();
}  // namespace autostart
