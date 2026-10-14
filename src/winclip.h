#pragma once

#include <QImage>
#include <QString>
#include <QStringList>

// Raw Win32 clipboard I/O — the port of clipboard_watcher.py's reader and
// clipboard_io.py's writers. Kept free of <windows.h> in the header (HWND is
// passed as void*) so the rest of the app doesn't inherit the Windows macros.
namespace winclip {

enum class Kind { None, Text, Image, Files };

struct Read {
    Kind kind = Kind::None;
    QString text;        // Kind::Text
    QImage image;        // Kind::Image  (decoded from CF_DIB)
    QStringList files;   // Kind::Files  (CF_HDROP path list)
};

// Read the current clipboard, retrying OpenClipboard on contention so a lost
// race never costs the user an item. Priority: files > text > image.
Read readClipboard();

// --- writers (paste-back), mirroring clipboard_io.py --------------------------
bool setText(const QString &text);
bool setImageFromPng(const QString &pngPath);   // PNG on disk -> CF_DIB
bool setFiles(const QStringList &paths);         // -> CF_HDROP (DROPFILES)

// Restore focus to the previously active window and synthesize Ctrl+V.
void pasteInto(void *hwnd);
void *foregroundWindow();

}  // namespace winclip
