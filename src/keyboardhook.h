#pragma once

#include <QObject>

// Low-level keyboard hook to claim Win+V — the port of keyboard_hook.py. The shell
// reserves Win+key, so RegisterHotKey(Win+V) fails with 1409 regardless of the
// clipboard-history setting; a WH_KEYBOARD_LL hook runs ahead of the shell and
// swallows the chord. Scope is deliberately minimal, because this is the same API
// AV heuristics associate with keyloggers: the callback reads only the virtual-key
// code, acts on V-with-Win-held alone, records nothing, and is removed on exit.
// Installed on the GUI thread and driven by Qt's message loop.
class WinVHook : public QObject {
    Q_OBJECT
public:
    explicit WinVHook(QObject *parent = nullptr);
    ~WinVHook() override;

    bool install();
    void uninstall();
    bool installed() const { return installed_; }

    // Called by the hook proc; return true to swallow the keystroke.
    bool onVDown();
    bool onVUp();

signals:
    void triggered();

private:
    void *hook_ = nullptr;  // HHOOK
    bool installed_ = false;
    bool vHeld_ = false;
};
