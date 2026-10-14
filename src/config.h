#pragma once

#include <QString>

// Filesystem paths and app-wide constants — the Qt port of the Python app's
// config.py. During the rewrite the data folder is "ClipKeeperQt" so buggy early
// builds can't corrupt the live %LOCALAPPDATA%\ClipKeeper history the Python app
// is still using. Flip kDataFolder to "ClipKeeper" for the final drop-in.
namespace cfg {

inline constexpr const char *kAppName = "ClipKeeper";

QString dataDir();            // %LOCALAPPDATA%\ClipKeeperQt
QString dbPath();             // ...\clipboard.db
QString imageDir();           // ...\images
QString settingsPath();       // ...\settings.json
QString supabaseConfigPath(); // ...\supabase.json  (URL + key override)
QString sessionPath();        // ...\session.json (auth tokens)
QString logPath();            // ...\clipkeeper.log
void ensureDirs();            // create dataDir + imageDir if missing

}  // namespace cfg
