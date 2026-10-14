#include "capture.h"

#include <windows.h>

#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QSet>
#include <algorithm>

#include "config.h"
#include "db.h"
#include "guard.h"
#include "winclip.h"

#ifndef WM_CLIPBOARDUPDATE
#define WM_CLIPBOARDUPDATE 0x031D
#endif

namespace {

const wchar_t *kWndClass = L"ClipKeeperWatcherWindow";

// Extensions that make a copied-file set read as "N images" (matches main.py).
const QSet<QString> &imageExts() {
    static const QSet<QString> exts = {
        QStringLiteral("jpg"),  QStringLiteral("jpeg"), QStringLiteral("png"),
        QStringLiteral("gif"),  QStringLiteral("bmp"),  QStringLiteral("webp"),
        QStringLiteral("tif"),  QStringLiteral("tiff"), QStringLiteral("heic"),
        QStringLiteral("heif"), QStringLiteral("avif"), QStringLiteral("ico"),
    };
    return exts;
}

// Content hash of image pixels for dedup. A fixed pixel format makes two decodes
// of the same clipboard bitmap hash identically.
QString imageHash(const QImage &image) {
    const QImage canonical = image.convertToFormat(QImage::Format_RGBA8888);
    QCryptographicHash h(QCryptographicHash::Blake2b_256);
    h.addData(reinterpret_cast<const char *>(canonical.constBits()),
              canonical.sizeInBytes());
    return QString::fromLatin1(h.result().toHex());
}

// Persist a clipboard image as a timestamped PNG; returns (path, size) or empty.
QPair<QString, QSize> saveImage(const QImage &image) {
    cfg::ensureDirs();
    const QDir dir(cfg::imageDir());
    const QString ts =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    QString path = dir.filePath(QStringLiteral("img_%1.png").arg(ts));
    for (int n = 1; QFileInfo::exists(path); ++n)
        path = dir.filePath(QStringLiteral("img_%1_%2.png").arg(ts).arg(n));
    if (!image.save(path, "PNG"))
        return {QString(), QSize()};
    return {QDir::toNativeSeparators(path), image.size()};
}

// One-line summary of a copied file set, e.g. "20 images" or "report.pdf".
QString filesPreview(const QStringList &paths) {
    if (paths.size() == 1) {
        const QString base = QFileInfo(paths.first()).fileName();
        return base.isEmpty() ? paths.first() : base;
    }
    const bool allImages = std::all_of(paths.cbegin(), paths.cend(), [](const QString &p) {
        return imageExts().contains(QFileInfo(p).suffix().toLower());
    });
    return QStringLiteral("%1 %2").arg(paths.size()).arg(allImages ? QStringLiteral("images")
                                                                   : QStringLiteral("files"));
}

LRESULT CALLBACK watcherWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_CLIPBOARDUPDATE) {
        auto *self = reinterpret_cast<ClipboardWatcher *>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (self)
            self->onClipboardUpdate();
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace

ClipboardWatcher::ClipboardWatcher(db::Database *db, QObject *parent)
    : QObject(parent), db_(db) {}

ClipboardWatcher::~ClipboardWatcher() { stop(); }

bool ClipboardWatcher::start() {
    HINSTANCE hinst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = watcherWndProc;
    wc.hInstance = hinst;
    wc.lpszClassName = kWndClass;
    atom_ = RegisterClassExW(&wc);  // 0 if already registered — CreateWindow by name still works

    HWND hwnd = CreateWindowExW(0, kWndClass, L"ClipKeeperWatcher", 0, 0, 0, 0, 0,
                                HWND_MESSAGE, nullptr, hinst, nullptr);
    if (!hwnd) {
        qWarning() << "[watcher] CreateWindow failed, err" << GetLastError();
        return false;
    }
    hwnd_ = hwnd;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    if (!AddClipboardFormatListener(hwnd))
        qWarning() << "[watcher] AddClipboardFormatListener failed, err" << GetLastError();
    return true;
}

void ClipboardWatcher::stop() {
    if (hwnd_) {
        HWND h = static_cast<HWND>(hwnd_);
        RemoveClipboardFormatListener(h);
        DestroyWindow(h);
        hwnd_ = nullptr;
    }
    if (atom_) {
        UnregisterClassW(kWndClass, GetModuleHandleW(nullptr));
        atom_ = 0;
    }
}

void ClipboardWatcher::onClipboardUpdate() {
    const winclip::Read r = winclip::readClipboard();
    switch (r.kind) {
    case winclip::Kind::Text:  handleText(r.text);   break;
    case winclip::Kind::Image: handleImage(r.image); break;
    case winclip::Kind::Files: handleFiles(r.files); break;
    case winclip::Kind::None:  break;
    }
}

void ClipboardWatcher::handleText(const QString &text) {
    if (guard::shouldSuppressText(text))
        return;
    const auto last = db_->lastItem();
    if (last && last->contentType == QLatin1String("text") && last->content == text)
        return;  // consecutive duplicate
    const qint64 id = db_->addItem(QStringLiteral("text"), text);
    if (id > 0)
        emit itemStored(id, QStringLiteral("text"), db::makePreview(QStringLiteral("text"), text));
}

void ClipboardWatcher::handleImage(const QImage &image) {
    if (guard::shouldSuppressImage())
        return;
    const QString hash = imageHash(image);
    if (db_->hashSeenRecently(hash))
        return;  // multiple updates per screenshot / stray 2nd instance
    const auto [path, size] = saveImage(image);
    if (path.isEmpty())
        return;
    const QString preview =
        QStringLiteral("[Image %1x%2]").arg(size.width()).arg(size.height());
    const qint64 id = db_->addItem(QStringLiteral("image"), path, preview, hash);
    if (id > 0)
        emit itemStored(id, QStringLiteral("image"), preview);
}

void ClipboardWatcher::handleFiles(const QStringList &paths) {
    const QString content = paths.join(QLatin1Char('\n'));
    if (guard::shouldSuppressFiles(content))
        return;
    const auto last = db_->lastItem();
    if (last && last->contentType == QLatin1String("files") && last->content == content)
        return;  // consecutive duplicate
    const QString preview = filesPreview(paths);
    const qint64 id = db_->addItem(QStringLiteral("files"), content, preview);
    if (id > 0)
        emit itemStored(id, QStringLiteral("files"), preview);
}
