#pragma once

#include <QObject>
#include <QString>

// Event-driven global hotkey via RegisterHotKey — the port of hotkey_listener.py.
// The preferred combo is Win+Shift+V, but that's often already taken (PowerToys
// Advanced Paste, etc.), so an ordered candidate list is tried and the first free
// one bound. A message-only window receives WM_HOTKEY, dispatched by Qt's own
// event loop on the GUI thread. Win+V is deliberately absent: the shell reserves
// Win+key, so RegisterHotKey(Win+V) fails with 1409 — that's the keyboard hook's
// job (see keyboardhook.h).
class HotkeyListener : public QObject {
    Q_OBJECT
public:
    explicit HotkeyListener(QObject *parent = nullptr);
    ~HotkeyListener() override;

    bool start();
    void stop();

    bool registered() const { return registered_; }
    QString activeLabel() const { return activeLabel_; }

    void onHotkey();  // called by the window proc on WM_HOTKEY

signals:
    void triggered();

private:
    void registerFirstFree();

    void *hwnd_ = nullptr;
    unsigned short atom_ = 0;
    bool registered_ = false;
    QString activeLabel_;
};
