#pragma once

#include <QColor>
#include <QString>

// ClipKeeper design system — the Qt port of theme.py. A refined dark palette:
// near-black background, warm tan/gold accent, thin-bordered surfaces.
namespace theme {

// Telegram-style "Night" dark palette: desaturated navy surfaces, soft blue accent.
inline const QColor BG(0x17, 0x21, 0x2b);          // window background
inline const QColor SURFACE(0x1d, 0x27, 0x33);     // list / panels
inline const QColor SURFACE_TOP(0x24, 0x2f, 0x3d); // raised surface
inline const QColor HOVER(0x20, 0x2b, 0x36);       // hover fill
inline const QColor SELECT(0x2b, 0x52, 0x78);      // selected row (Telegram blue)
inline const QColor INPUT_BG(0x24, 0x2f, 0x3d);
inline const QColor INPUT_BORDER(0x2f, 0x3b, 0x47);
inline const QColor CARD_BORDER(0x10, 0x19, 0x21);
inline const QColor OUTLINE(0x0e, 0x16, 0x21);     // hairline window border
inline const QColor DIVIDER(0x10, 0x19, 0x21);

inline const QColor ACCENT(0x52, 0x88, 0xc1);      // Telegram blue
inline const QColor ACCENT_HOVER(0x6a, 0xb7, 0xff);
inline const QColor ACCENT_TEXT(0xff, 0xff, 0xff);

inline const QColor HEADING(0xff, 0xff, 0xff);     // heading white
inline const QColor FG(0xe9, 0xed, 0xf1);          // body text (named FG, not TEXT: windows.h has a TEXT() macro)
inline const QColor MUTED(0x70, 0x84, 0x99);       // helper text (muted blue-gray)
inline const QColor HINT(0x6a, 0xb7, 0xff);        // blue hint text
inline const QColor DANGER(0xe5, 0x73, 0x73);

inline constexpr const char *SERIF = "Palatino Linotype";
inline constexpr const char *SANS = "Segoe UI";
inline constexpr const char *MONO = "Consolas";

// Global stylesheet for the popup and its child widgets.
QString styleSheet();

}  // namespace theme
