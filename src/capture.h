#pragma once

#include <QObject>
#include <QStringList>

namespace db { class Database; }

// Clipboard capture — the raw-Win32 port of clipboard_watcher.py + main.py's
// on_change. A message-only window (HWND_MESSAGE) registers
// AddClipboardFormatListener; its WM_CLIPBOARDUPDATE messages are dispatched by
// Qt's own event loop on the GUI thread (the window lives on that thread), so
// there's no separate message pump and no cross-thread marshalling. Reads,
// dedups, and stores each change.
class ClipboardWatcher : public QObject {
    Q_OBJECT
public:
    explicit ClipboardWatcher(db::Database *db, QObject *parent = nullptr);
    ~ClipboardWatcher() override;

    bool start();
    void stop();

    // Called by the window proc on WM_CLIPBOARDUPDATE (public so a free-function
    // WndProc can reach it without pulling <windows.h> into this header).
    void onClipboardUpdate();

signals:
    void itemStored(qint64 id, const QString &contentType, const QString &preview);

private:
    void handleText(const QString &text);
    void handleImage(const class QImage &image);
    void handleFiles(const QStringList &paths);

    void *hwnd_ = nullptr;      // HWND
    unsigned short atom_ = 0;   // registered window-class ATOM
    db::Database *db_;
};
