#include "hotkey.h"

#include <windows.h>

#include <QDebug>

namespace {

const wchar_t *kWndClass = L"ClipKeeperHotkeyWindow";
constexpr int kHotkeyId = 1;
constexpr UINT kNoRepeat = 0x4000;  // MOD_NOREPEAT
constexpr int kAlreadyRegistered = 1409;

struct Candidate {
    UINT mods;
    UINT vk;
    const char *label;
};

// Tried in order; first free one wins. Win+V is handled by the keyboard hook.
const Candidate kCandidates[] = {
    {MOD_WIN | MOD_SHIFT, 'V', "Win+Shift+V"},
    {MOD_CONTROL | MOD_ALT, 'V', "Ctrl+Alt+V"},
    {MOD_CONTROL | MOD_WIN, 'V', "Ctrl+Win+V"},
    {MOD_ALT | MOD_SHIFT, 'V', "Alt+Shift+V"},
};

LRESULT CALLBACK hotkeyWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_HOTKEY && wParam == kHotkeyId) {
        auto *self = reinterpret_cast<HotkeyListener *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (self)
            self->onHotkey();
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace

HotkeyListener::HotkeyListener(QObject *parent) : QObject(parent) {}
HotkeyListener::~HotkeyListener() { stop(); }

bool HotkeyListener::start() {
    HINSTANCE hinst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = hotkeyWndProc;
    wc.hInstance = hinst;
    wc.lpszClassName = kWndClass;
    atom_ = RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(0, kWndClass, L"ClipKeeperHotkey", 0, 0, 0, 0, 0,
                                HWND_MESSAGE, nullptr, hinst, nullptr);
    if (!hwnd) {
        qWarning() << "[hotkey] CreateWindow failed, err" << GetLastError();
        return false;
    }
    hwnd_ = hwnd;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    registerFirstFree();
    return registered_;
}

void HotkeyListener::registerFirstFree() {
    const auto hwnd = static_cast<HWND>(hwnd_);
    QStringList busy;
    for (const Candidate &c : kCandidates) {
        if (RegisterHotKey(hwnd, kHotkeyId, c.mods | kNoRepeat, c.vk)) {
            registered_ = true;
            activeLabel_ = QString::fromLatin1(c.label);
            if (!busy.isEmpty())
                qInfo() << "[hotkey]" << busy.join(QStringLiteral(", ")) << "in use; bound" << activeLabel_;
            return;
        }
        const DWORD err = GetLastError();
        if (int(err) == kAlreadyRegistered)
            busy << QString::fromLatin1(c.label);
        else
            qWarning() << "[hotkey]" << c.label << "failed, err" << err;
    }
    qWarning() << "[hotkey] could not bind any candidate; popup won't open via a fallback hotkey";
}

void HotkeyListener::stop() {
    if (hwnd_) {
        const auto hwnd = static_cast<HWND>(hwnd_);
        if (registered_)
            UnregisterHotKey(hwnd, kHotkeyId);
        DestroyWindow(hwnd);
        hwnd_ = nullptr;
    }
    if (atom_) {
        UnregisterClassW(kWndClass, GetModuleHandleW(nullptr));
        atom_ = 0;
    }
    registered_ = false;
}

void HotkeyListener::onHotkey() {
    emit triggered();
}
