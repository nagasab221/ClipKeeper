#pragma once

#include <QIcon>

// The clipboard glyph, drawn to match the theme (warm tan on near-black) — the
// Qt port of the Python app's icon.py. Drawn with QPainter so there's no asset
// to ship and the silhouette stays crisp at any tray size.
namespace ui {
QIcon appIcon();
}
