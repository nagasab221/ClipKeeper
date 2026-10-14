#include "config.h"

#include <QDir>

namespace cfg {

// Dev builds isolate to %LOCALAPPDATA%\ClipKeeperQt so they can't disturb the
// Python app; the drop-in build (-DCLIPKEEPER_DROP_IN=ON) shares \ClipKeeper.
#ifdef CLIPKEEPER_DROP_IN
static constexpr const char *kDataFolder = "ClipKeeper";
#else
static constexpr const char *kDataFolder = "ClipKeeperQt";
#endif

QString dataDir() {
    // Mirror config.py's _data_dir(): machine-local, not roaming.
    QString base = qEnvironmentVariable("LOCALAPPDATA");
    if (base.isEmpty())
        base = QDir::homePath();
    return QDir(base).filePath(kDataFolder);
}

QString dbPath()             { return QDir(dataDir()).filePath("clipboard.db"); }
QString imageDir()           { return QDir(dataDir()).filePath("images"); }
QString settingsPath()       { return QDir(dataDir()).filePath("settings.json"); }
QString supabaseConfigPath() { return QDir(dataDir()).filePath("supabase.json"); }
QString sessionPath()        { return QDir(dataDir()).filePath("session.json"); }
QString logPath()            { return QDir(dataDir()).filePath("clipkeeper.log"); }

void ensureDirs() {
    QDir().mkpath(dataDir());
    QDir().mkpath(imageDir());
}

}  // namespace cfg
