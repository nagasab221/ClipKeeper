#pragma once

#include <QString>

// Read and (opt-in only) change Windows' built-in Clipboard History setting —
// port of windows_history.py. HKCU\Software\Microsoft\Clipboard\
// EnableClipboardHistory is exactly what Settings › System › Clipboard writes:
// per-user, no admin, fully reversible.
namespace winhistory {
int state();          // 1 = on, 0 = off, -1 = unset (Windows defaults to on)
bool setEnabled(bool enabled);
bool ensureDisabled();
QString describe();
}  // namespace winhistory
