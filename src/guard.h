#pragma once

#include <QString>

// Self-write suppression — the Qt port of clipboard_io.py's guard. When the app
// pastes an item back it sets the clipboard, which trips our own listener; a
// short (1.5s) time-boxed guard lets the capture side skip that echo so history
// isn't polluted by ClipKeeper's own pastes. Wired by paste-back in Phase 2;
// harmless (always "not suppressed") until then.
namespace guard {

void markSelfText(const QString &content);
void markSelfImage();
void markSelfFiles(const QString &content);

bool shouldSuppressText(const QString &content);
bool shouldSuppressImage();
bool shouldSuppressFiles(const QString &content);

}  // namespace guard
