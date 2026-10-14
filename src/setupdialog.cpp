#include "setupdialog.h"

#include <QCheckBox>
#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "settings.h"
#include "winhistory.h"

namespace {
const char *kQss = R"QSS(
    QDialog { background: #17212b; }
    QLabel { color: #e9edf1; }
    QLabel#Title { color: #ffffff; font-family: "Segoe UI"; font-size: 22px; font-weight: 600; }
    QLabel#Sub { color: #708499; font-size: 13px; }
    QLabel#Body { color: #708499; font-size: 12px; }
    QLabel#Status { color: #6ab7ff; font-size: 12px; }
    QFrame#Card { background: #1d2733; border-radius: 10px; }
    QCheckBox { color: #e9edf1; font-size: 13px; spacing: 8px; }
    QCheckBox::indicator { width: 18px; height: 18px; border: 1px solid #2f3b47; border-radius: 4px; background: #242f3d; }
    QCheckBox::indicator:checked { background: #5288c1; border: 1px solid #5288c1; }
    QPushButton#Primary { background: #5288c1; color: #ffffff; border: none; border-radius: 6px; padding: 8px 18px; font-weight: 600; }
    QPushButton#Primary:hover { background: #6ab7ff; }
    QPushButton#Ghost { background: transparent; color: #e9edf1; border: 1px solid #2f3b47; border-radius: 6px; padding: 8px 18px; }
    QPushButton#Ghost:hover { background: #202b36; }
)QSS";
}  // namespace

void showSetupDialog(QWidget *parent, Settings *settings, std::function<void()> onApplied) {
    auto *dlg = new QDialog(parent, Qt::Window);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(QStringLiteral("ClipKeeper setup"));
    dlg->setFixedWidth(580);
    dlg->setStyleSheet(QString::fromLatin1(kQss));

    auto *v = new QVBoxLayout(dlg);
    v->setContentsMargins(28, 22, 28, 20);
    v->setSpacing(6);

    auto *title = new QLabel(QStringLiteral("ClipKeeper"));
    title->setObjectName(QStringLiteral("Title"));
    v->addWidget(title);
    auto *sub = new QLabel(QStringLiteral("Two optional choices. Both are off by default."));
    sub->setObjectName(QStringLiteral("Sub"));
    v->addWidget(sub);
    v->addSpacing(10);

    // Card 1 — Win+V via the keyboard hook.
    auto *card1 = new QFrame;
    card1->setObjectName(QStringLiteral("Card"));
    auto *c1 = new QVBoxLayout(card1);
    c1->setContentsMargins(18, 16, 18, 16);
    auto *cbWinV = new QCheckBox(QStringLiteral("Use Win+V for ClipKeeper"));
    cbWinV->setChecked(settings->getBool(QStringLiteral("use_win_v")));
    c1->addWidget(cbWinV);
    auto *b1 = new QLabel(QStringLiteral(
        "Windows reserves Win+V and won't give it up, so ClipKeeper claims it with a "
        "low-level keyboard hook that intercepts the combo before the shell sees it.\n\n"
        "The hook reads only the key code, acts on Win+V alone, and passes every other key "
        "straight through — nothing is recorded. Worth knowing: this is the same Windows API "
        "antivirus tools associate with keyloggers, so a scanner may flag it one day."));
    b1->setObjectName(QStringLiteral("Body"));
    b1->setWordWrap(true);
    c1->addWidget(b1);
    v->addWidget(card1);
    v->addSpacing(10);

    // Card 2 — Windows' own clipboard history.
    auto *card2 = new QFrame;
    card2->setObjectName(QStringLiteral("Card"));
    auto *c2 = new QVBoxLayout(card2);
    c2->setContentsMargins(18, 16, 18, 16);
    auto *cbKill = new QCheckBox(QStringLiteral("Also turn off Windows' own clipboard history"));
    cbKill->setChecked(winhistory::state() == 0);
    c2->addWidget(cbKill);
    auto *b2 = new QLabel(QStringLiteral(
        "Optional, and not needed for Win+V. It only stops Windows keeping its own second copy "
        "of everything you copy. Same per-user switch as Settings › System › Clipboard — no admin "
        "rights, fully reversible. Note it also disables Windows' cloud clipboard sync."));
    b2->setObjectName(QStringLiteral("Body"));
    b2->setWordWrap(true);
    c2->addWidget(b2);
    v->addWidget(card2);
    v->addSpacing(8);

    auto *status = new QLabel(
        QStringLiteral("Windows Clipboard History is currently %1.").arg(winhistory::describe()));
    status->setObjectName(QStringLiteral("Status"));
    v->addWidget(status);
    v->addSpacing(6);

    auto *btns = new QHBoxLayout;
    btns->addStretch(1);
    auto *skip = new QPushButton(QStringLiteral("Skip"));
    skip->setObjectName(QStringLiteral("Ghost"));
    auto *save = new QPushButton(QStringLiteral("Save"));
    save->setObjectName(QStringLiteral("Primary"));
    btns->addWidget(skip);
    btns->addWidget(save);
    v->addLayout(btns);

    QObject::connect(save, &QPushButton::clicked, dlg, [=]() {
        const bool wantsOff = cbKill->isChecked();
        const bool currentlyOff = winhistory::state() == 0;
        if (wantsOff != currentlyOff && !winhistory::setEnabled(!wantsOff)) {
            status->setText(QStringLiteral("Couldn't write the registry setting — nothing changed."));
            return;
        }
        settings->set(QStringLiteral("use_win_v"), cbWinV->isChecked(), false);
        settings->set(QStringLiteral("disable_windows_history"), wantsOff, false);
        settings->set(QStringLiteral("setup_completed"), true);
        if (onApplied)
            onApplied();
        dlg->accept();
    });
    QObject::connect(skip, &QPushButton::clicked, dlg, [=]() {
        settings->set(QStringLiteral("setup_completed"), true);
        dlg->accept();
    });

    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}
