#include "autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>

namespace autostart {
namespace {
const QString kRunKey =
    QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
// Dev uses a distinct Run-key value so it can't clobber the Python ClipKeeper's
// "ClipKeeper" entry; the drop-in build shares "ClipKeeper".
#ifdef CLIPKEEPER_DROP_IN
const QString kValue = QStringLiteral("ClipKeeper");
#else
const QString kValue = QStringLiteral("ClipKeeperQt");
#endif
}  // namespace

QString launchCommand() {
    return QLatin1Char('"') +
           QDir::toNativeSeparators(QCoreApplication::applicationFilePath()) +
           QLatin1Char('"');
}

bool isEnabled() {
    QSettings s(kRunKey, QSettings::NativeFormat);
    return s.contains(kValue);
}

bool enable() {
    QSettings s(kRunKey, QSettings::NativeFormat);
    s.setValue(kValue, launchCommand());
    s.sync();
    return s.status() == QSettings::NoError;
}

bool disable() {
    QSettings s(kRunKey, QSettings::NativeFormat);
    s.remove(kValue);
    s.sync();
    return s.status() == QSettings::NoError;
}

}  // namespace autostart
