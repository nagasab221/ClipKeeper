#include "winhistory.h"

#include <QSettings>

namespace winhistory {
namespace {
const QString kKey = QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Clipboard");
const QString kValue = QStringLiteral("EnableClipboardHistory");
}  // namespace

int state() {
    QSettings s(kKey, QSettings::NativeFormat);
    if (!s.contains(kValue))
        return -1;
    return s.value(kValue).toInt() != 0 ? 1 : 0;
}

bool setEnabled(bool enabled) {
    QSettings s(kKey, QSettings::NativeFormat);
    s.setValue(kValue, enabled ? 1 : 0);
    s.sync();
    return s.status() == QSettings::NoError;
}

bool ensureDisabled() {
    if (state() == 0)
        return true;
    return setEnabled(false);
}

QString describe() {
    switch (state()) {
    case 1:  return QStringLiteral("on");
    case 0:  return QStringLiteral("off");
    default: return QStringLiteral("on (Windows default, not set)");
    }
}

}  // namespace winhistory
