#include "winclip.h"

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>   // DROPFILES

#include <cstring>
#include <optional>
#include <string>

#include <QBuffer>
#include <QByteArray>
#include <QtEndian>

namespace winclip {
namespace {

// OpenClipboard races with any other app reading it, so retry a few times
// rather than dropping the read (mirrors clipboard_watcher._read_once).
bool openClipboardRetry() {
    for (int i = 0; i < 10; ++i) {
        if (OpenClipboard(nullptr))
            return true;
        Sleep(20);
    }
    return false;
}

// Rebuild a loadable BMP from a packed CF_DIB by prepending a 14-byte
// BITMAPFILEHEADER, then let Qt decode it. bfOffBits must clear the header plus
// any colour table / channel masks, or Qt reads pixels from the wrong offset.
QImage dibToQImage(const QByteArray &dib) {
    if (dib.size() < int(sizeof(BITMAPINFOHEADER)))
        return QImage();
    const auto *bih = reinterpret_cast<const BITMAPINFOHEADER *>(dib.constData());
    const quint32 biSize = bih->biSize;
    const quint16 bitCount = bih->biBitCount;
    const quint32 compression = bih->biCompression;
    const quint32 clrUsed = bih->biClrUsed;

    quint32 tableBytes = 0;
    if (bitCount <= 8) {
        const quint32 colors = clrUsed ? clrUsed : (1u << bitCount);
        tableBytes = colors * 4;  // RGBQUAD palette
    } else if (compression == BI_BITFIELDS && biSize < 108 /* pre-V4 header */) {
        tableBytes = 12;          // three DWORD channel masks follow the header
    }
    const quint32 offBits = 14 + biSize + tableBytes;

    QByteArray bmp;
    bmp.reserve(14 + dib.size());
    bmp.append('B').append('M');
    auto put32 = [&bmp](quint32 v) {
        char b[4];
        qToLittleEndian(v, b);
        bmp.append(b, 4);
    };
    put32(14 + quint32(dib.size()));  // bfSize
    put32(0);                         // bfReserved1 + bfReserved2
    put32(offBits);                   // bfOffBits
    bmp.append(dib);
    return QImage::fromData(reinterpret_cast<const uchar *>(bmp.constData()),
                            int(bmp.size()), "BMP");
}

std::optional<Read> readOnce() {
    if (!openClipboardRetry())
        return std::nullopt;  // busy -> caller retries

    QString text;
    QStringList files;
    QByteArray dib;
    bool hasDib = false;

    // Files copied in Explorer (even image files) arrive as CF_HDROP.
    if (IsClipboardFormatAvailable(CF_HDROP)) {
        if (HANDLE h = GetClipboardData(CF_HDROP)) {
            auto drop = static_cast<HDROP>(h);
            const UINT n = DragQueryFileW(drop, 0xFFFFFFFFu, nullptr, 0);
            for (UINT i = 0; i < n; ++i) {
                wchar_t buf[1024];
                const UINT len =
                    DragQueryFileW(drop, i, buf, UINT(sizeof(buf) / sizeof(buf[0])));
                if (len > 0)
                    files << QString::fromWCharArray(buf, int(len));
            }
        }
    }
    if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        if (HANDLE h = GetClipboardData(CF_UNICODETEXT)) {
            if (auto *p = static_cast<const wchar_t *>(GlobalLock(h))) {
                text = QString::fromWCharArray(p);
                GlobalUnlock(h);
            }
        }
    }
    if (IsClipboardFormatAvailable(CF_DIB)) {
        hasDib = true;
        if (HANDLE h = GetClipboardData(CF_DIB)) {
            const SIZE_T sz = GlobalSize(h);
            if (const void *p = GlobalLock(h)) {
                if (sz > 0)
                    dib = QByteArray(static_cast<const char *>(p), int(sz));
                GlobalUnlock(h);
            }
        }
    }
    CloseClipboard();  // losing the handle here must not discard what we read

    Read out;
    if (!files.isEmpty()) {  // file-copy intent wins over any text/image alias
        out.kind = Kind::Files;
        out.files = files;
        return out;
    }
    if (!text.isEmpty()) {  // text wins over image (e.g. a copied spreadsheet cell)
        out.kind = Kind::Text;
        out.text = text;
        return out;
    }
    if (hasDib && !dib.isEmpty()) {
        QImage img = dibToQImage(dib);
        if (!img.isNull()) {
            out.kind = Kind::Image;
            out.image = img;
            return out;
        }
    }
    return out;  // Kind::None
}

}  // namespace

Read readClipboard() {
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (auto r = readOnce())
            return *r;
        Sleep(50);
    }
    return Read{};
}

bool setText(const QString &text) {
    if (!openClipboardRetry())
        return false;
    EmptyClipboard();
    bool ok = false;
    const int chars = text.size() + 1;
    if (HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, SIZE_T(chars) * sizeof(wchar_t))) {
        if (auto *dst = static_cast<wchar_t *>(GlobalLock(h))) {
            const int n = text.toWCharArray(dst);
            dst[n] = L'\0';
            GlobalUnlock(h);
            ok = SetClipboardData(CF_UNICODETEXT, h) != nullptr;
        }
        if (!ok)
            GlobalFree(h);
    }
    CloseClipboard();
    return ok;
}

bool setImageFromPng(const QString &pngPath) {
    QImage img(pngPath);
    if (img.isNull())
        return false;
    const QImage rgb = img.convertToFormat(QImage::Format_RGB888);
    QByteArray bmp;
    {
        QBuffer buf(&bmp);
        buf.open(QIODevice::WriteOnly);
        rgb.save(&buf, "BMP");
    }
    if (bmp.size() <= 14)
        return false;
    const QByteArray dib = bmp.mid(14);  // drop the BITMAPFILEHEADER -> raw DIB

    if (!openClipboardRetry())
        return false;
    EmptyClipboard();
    bool ok = false;
    if (HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, SIZE_T(dib.size()))) {
        if (void *dst = GlobalLock(h)) {
            std::memcpy(dst, dib.constData(), size_t(dib.size()));
            GlobalUnlock(h);
            ok = SetClipboardData(CF_DIB, h) != nullptr;
        }
        if (!ok)
            GlobalFree(h);
    }
    CloseClipboard();
    return ok;
}

bool setFiles(const QStringList &paths) {
    QStringList clean;
    for (const QString &p : paths)
        if (!p.isEmpty())
            clean << p;
    if (clean.isEmpty())
        return false;

    // DROPFILES header, then each path NUL-terminated and one extra NUL to close.
    std::wstring list;
    for (const QString &p : clean) {
        list += p.toStdWString();
        list.push_back(L'\0');
    }
    list.push_back(L'\0');

    const SIZE_T bytes = sizeof(DROPFILES) + list.size() * sizeof(wchar_t);
    if (!openClipboardRetry())
        return false;
    EmptyClipboard();
    bool ok = false;
    if (HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes)) {
        if (auto *df = static_cast<DROPFILES *>(GlobalLock(h))) {
            ZeroMemory(df, sizeof(DROPFILES));
            df->pFiles = sizeof(DROPFILES);  // list starts right after the header
            df->fWide = TRUE;
            std::memcpy(reinterpret_cast<char *>(df) + sizeof(DROPFILES), list.data(),
                        list.size() * sizeof(wchar_t));
            GlobalUnlock(h);
            ok = SetClipboardData(CF_HDROP, h) != nullptr;
        }
        if (!ok)
            GlobalFree(h);
    }
    CloseClipboard();
    return ok;
}

void pasteInto(void *hwnd) {
    if (hwnd)
        SetForegroundWindow(static_cast<HWND>(hwnd));
    Sleep(60);
    keybd_event(VK_CONTROL, 0, 0, 0);
    keybd_event('V', 0, 0, 0);
    keybd_event('V', 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
}

void *foregroundWindow() {
    return GetForegroundWindow();
}

}  // namespace winclip
