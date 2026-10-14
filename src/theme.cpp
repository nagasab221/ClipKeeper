#include "theme.h"

namespace theme {

QString styleSheet() {
    // Telegram-style dark palette. Colours are inlined here (they're constants)
    // to keep the sheet readable; they mirror theme.h.
    return QStringLiteral(R"QSS(
        QWidget { color: #e9edf1; font-family: "Segoe UI"; font-size: 13px; }

        #Card { background: #17212b; }

        QLineEdit#Search {
            background: #242f3d; border: 1px solid #2f3b47; border-radius: 6px;
            padding: 6px 8px; color: #e9edf1; selection-background-color: #2b5278;
        }

        QListView#List {
            background: #1d2733; border: 1px solid #101921; border-radius: 8px;
            outline: 0; padding: 2px;
        }
        QListView#List::item { border: none; }

        QScrollArea#MoreArea { background: #1d2733; border: 1px solid #101921; border-radius: 8px; }
        QScrollArea#MoreArea > QWidget > QWidget { background: #1d2733; }

        QLabel#Section { color: #6ab7ff; font-size: 11px; font-weight: 600; }

        QPushButton#SnipItem {
            background: #242f3d; border: none; border-radius: 5px; color: #e9edf1;
            text-align: left; padding: 5px 8px; font-size: 12px;
        }
        QPushButton#SnipItem:hover { background: #202b36; }

        QLineEdit#SnipKw, QLineEdit#SnipText {
            background: #242f3d; border: 1px solid #2f3b47; border-radius: 6px;
            padding: 5px 7px; color: #e9edf1;
        }
        QFrame#SnipForm { background: #242f3d; border-radius: 7px; }

        QCheckBox { color: #e9edf1; font-size: 12px; spacing: 8px; }
        QCheckBox::indicator { width: 17px; height: 17px; border: 1px solid #2f3b47; border-radius: 4px; background: #242f3d; }
        QCheckBox::indicator:checked { background: #5288c1; border: 1px solid #5288c1; }

        QPushButton#Primary {
            background: #5288c1; border: none; border-radius: 6px; color: #ffffff;
            padding: 5px 12px; font-weight: 600;
        }
        QPushButton#Primary:hover { background: #6ab7ff; }

        QPushButton#Ghost {
            background: transparent; border: 1px solid #2f3b47; border-radius: 6px;
            color: #e9edf1; padding: 5px 12px;
        }
        QPushButton#Ghost:hover { background: #202b36; }

        /* Compact bottom-right view toggle (History <-> More). */
        QPushButton#More {
            background: transparent; border: none; border-radius: 6px;
            color: #6ab7ff; font-size: 11px; padding: 3px 8px;
        }
        QPushButton#More:hover { background: #202b36; }

        QLabel#Hint, QLabel#Count { color: #708499; font-size: 10px; }

        QFrame#Notice { background: #242f3d; border-radius: 7px; }
        QLabel#NoticeText { color: #6ab7ff; font-size: 10px; }

        QScrollBar:vertical { background: transparent; width: 8px; margin: 2px; }
        QScrollBar::handle:vertical { background: #2f3b47; border-radius: 4px; min-height: 24px; }
        QScrollBar::handle:vertical:hover { background: #3a4756; }
        QScrollBar::add-line, QScrollBar::sub-line { height: 0; }
        QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
    )QSS");
}

}  // namespace theme
