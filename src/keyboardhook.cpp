#include "keyboardhook.h"

#include <windows.h>

#include <QDebug>
#include <QMetaObject>

namespace {

WinVHook *g_hook = nullptr;

// We ate the V, so the shell would otherwise see a bare Win tap and open Start.
// A synthetic Ctrl tap turns it into a chord, which Start ignores.
void breakStartMenu() {
    keybd_event(VK_CONTROL, 0, 0, 0);
    keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
}

LRESULT CALLBACK llKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    // Keep this fast: Windows silently drops low-level hooks that block past
    // LowLevelHooksTimeout (300ms default).
    if (nCode == HC_ACTION && g_hook) {
        auto *kb = reinterpret_cast<KBDLLHOOKSTRUCT *>(lParam);
        if (kb->vkCode == 'V' && !(kb->flags & LLKHF_INJECTED)) {
            const bool winDown = (GetAsyncKeyState(VK_LWIN) & 0x8000) ||
                                 (GetAsyncKeyState(VK_RWIN) & 0x8000);
            if ((wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) && winDown) {
                if (g_hook->onVDown()) {
                    breakStartMenu();
                    return 1;  // swallow: the shell never sees it
                }
            } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
                if (g_hook->onVUp())
                    return 1;  // swallow the matching key-up too
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

}  // namespace

WinVHook::WinVHook(QObject *parent) : QObject(parent) {}
WinVHook::~WinVHook() { uninstall(); }

bool WinVHook::install() {
    if (installed_)
        return true;
    g_hook = this;
    HHOOK h = SetWindowsHookExW(WH_KEYBOARD_LL, llKeyboardProc, GetModuleHandleW(nullptr), 0);
    if (!h) {
        qWarning() << "[win+v] SetWindowsHookEx failed, err" << GetLastError();
        g_hook = nullptr;
        return false;
    }
    hook_ = h;
    installed_ = true;
    return true;
}

void WinVHook::uninstall() {
    if (hook_) {
        UnhookWindowsHookEx(static_cast<HHOOK>(hook_));
        hook_ = nullptr;
    }
    installed_ = false;
    if (g_hook == this)
        g_hook = nullptr;
}

bool WinVHook::onVDown() {
    if (vHeld_)
        return true;  // ignore auto-repeat while held; still swallow
    vHeld_ = true;
    // Defer the popup toggle so the hook callback returns immediately.
    QMetaObject::invokeMethod(this, [this] { emit triggered(); }, Qt::QueuedConnection);
    return true;
}

bool WinVHook::onVUp() {
    if (!vHeld_)
        return false;
    vHeld_ = false;
    return true;
}
