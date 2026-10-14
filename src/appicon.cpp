#include "appicon.h"

#include <QColor>
#include <QPainter>
#include <QPixmap>
#include <QRectF>

namespace ui {

QIcon appIcon() {
    constexpr int kSize = 256;  // draw large; QIcon downscales smoothly for the tray
    QPixmap pm(kSize, kSize);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);

    // Same geometry as icon.py: design on a 64-unit grid, rendered at 256.
    const double s = kSize / 64.0;
    auto box = [&](double x0, double y0, double x1, double y1) {
        return QRectF(x0 * s, y0 * s, (x1 - x0) * s, (y1 - y0) * s);
    };
    const QColor accent(0x6a, 0xb7, 0xff);  // bright Telegram blue (reads well at tray size)
    const QColor bg(0x17, 0x21, 0x2b);      // dark navy page

    p.setBrush(accent); p.drawRoundedRect(box(7, 11, 57, 61), 9 * s, 9 * s);   // board
    p.setBrush(bg);     p.drawRoundedRect(box(17, 21, 47, 53), 5 * s, 5 * s);  // page inset
    p.setBrush(accent); p.drawRoundedRect(box(23, 3, 41, 17), 5 * s, 5 * s);   // clip
    p.setBrush(accent); p.drawRoundedRect(box(23, 29, 41, 35), 3 * s, 3 * s);  // bar 1
    p.setBrush(accent); p.drawRoundedRect(box(23, 40, 36, 46), 3 * s, 3 * s);  // bar 2
    p.end();

    return QIcon(pm);
}

}  // namespace ui
