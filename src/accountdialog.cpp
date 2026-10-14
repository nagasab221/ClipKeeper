#include "accountdialog.h"

#include <QCheckBox>
#include <QDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <memory>

#include "config.h"
#include "settings.h"
#include "supabaseclient.h"
#include "syncengine.h"

namespace {

const char *kHint = "#6ab7ff";
const char *kDanger = "#e57373";

const char *kQss = R"QSS(
    QDialog { background: #17212b; }
    QLabel { color: #e9edf1; }
    QLabel#Title { color: #ffffff; font-family: "Segoe UI"; font-size: 21px; font-weight: 600; }
    QLabel#Sub { color: #708499; font-size: 12px; }
    QLabel#Body { color: #708499; font-size: 12px; }
    QLabel#Status { color: #6ab7ff; font-size: 13px; }
    QFrame#Card { background: #1d2733; border-radius: 10px; }
    QLineEdit#Entry { background: #242f3d; border: 1px solid #2f3b47; border-radius: 6px; padding: 7px 8px; color: #e9edf1; }
    QCheckBox { color: #e9edf1; font-size: 12px; spacing: 8px; }
    QCheckBox::indicator { width: 18px; height: 18px; border: 1px solid #2f3b47; border-radius: 4px; background: #242f3d; }
    QCheckBox::indicator:checked { background: #5288c1; border: 1px solid #5288c1; }
    QPushButton#Primary { background: #5288c1; color: #ffffff; border: none; border-radius: 6px; padding: 7px 14px; font-weight: 600; }
    QPushButton#Primary:hover { background: #6ab7ff; }
    QPushButton#Ghost { background: transparent; color: #e9edf1; border: 1px solid #2f3b47; border-radius: 6px; padding: 7px 14px; }
    QPushButton#Ghost:hover { background: #202b36; }
)QSS";

void clearLayout(QLayout *lay) {
    while (QLayoutItem *item = lay->takeAt(0)) {
        if (QWidget *w = item->widget())
            w->deleteLater();
        else if (QLayout *child = item->layout()) {
            clearLayout(child);
            child->deleteLater();
        }
        delete item;
    }
}

}  // namespace

void showAccountDialog(QWidget *parent, SupabaseClient *client, SyncEngine *engine,
                       Settings *settings, std::function<void()> onChanged) {
    auto *dlg = new QDialog(parent, Qt::Window);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(QStringLiteral("ClipKeeper account"));
    dlg->setFixedWidth(520);
    dlg->setStyleSheet(QString::fromLatin1(kQss));

    auto *v = new QVBoxLayout(dlg);
    v->setContentsMargins(26, 22, 26, 18);

    auto *title = new QLabel(QStringLiteral("Cloud sync"));
    title->setObjectName(QStringLiteral("Title"));
    v->addWidget(title);
    auto *sub = new QLabel(QStringLiteral("Text history syncs across your devices. Images stay local."));
    sub->setObjectName(QStringLiteral("Sub"));
    v->addWidget(sub);
    v->addSpacing(12);

    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("Card"));
    auto *cardL = new QVBoxLayout(card);
    cardL->setContentsMargins(16, 16, 16, 16);
    auto *status = new QLabel(engine->status());
    status->setObjectName(QStringLiteral("Status"));
    status->setWordWrap(true);
    cardL->addWidget(status);
    auto *body = new QWidget;
    auto *bodyL = new QVBoxLayout(body);
    bodyL->setContentsMargins(0, 10, 0, 0);
    bodyL->setSpacing(8);
    cardL->addWidget(body);
    v->addWidget(card);

    auto *disc = new QLabel(QStringLiteral(
        "Clipboard text is stored unencrypted, locally and in the cloud. Don't copy anything "
        "through ClipKeeper you wouldn't put in a plain-text file."));
    disc->setObjectName(QStringLiteral("Body"));
    disc->setWordWrap(true);
    v->addSpacing(12);
    v->addWidget(disc);

    auto *closeRow = new QHBoxLayout;
    closeRow->addStretch(1);
    auto *closeBtn = new QPushButton(QStringLiteral("Close"));
    closeBtn->setObjectName(QStringLiteral("Ghost"));
    QObject::connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);
    closeRow->addWidget(closeBtn);
    v->addSpacing(12);
    v->addLayout(closeRow);

    auto setStatus = [status](const QString &text, const char *color = kHint) {
        status->setStyleSheet(QStringLiteral("color: %1;").arg(QString::fromLatin1(color)));
        status->setText(text);
    };

    // Recursive lambda: rebuilds the body for the current auth state.
    auto rebuild = std::make_shared<std::function<void()>>();
    *rebuild = [=]() {
        clearLayout(bodyL);
        setStatus(engine->status());

        if (!client->isConfigured()) {
            auto *l = new QLabel(
                QStringLiteral("No Supabase project connected yet.\n\nCreate %1 in\n%2\n"
                               "containing:  {\"url\": \"https://xxxx.supabase.co\", "
                               "\"anon_key\": \"sb_publishable_...\"}\n\nThen run "
                               "supabase_schema.sql in the project's SQL editor and reopen this window.")
                    .arg(QFileInfo(cfg::supabaseConfigPath()).fileName(), cfg::dataDir()));
            l->setObjectName(QStringLiteral("Body"));
            l->setWordWrap(true);
            bodyL->addWidget(l);
            return;
        }

        if (client->isSignedIn() && client->isAnonymous()) {
            auto *info = new QLabel(QStringLiteral(
                "You're synced anonymously — no account needed. History is backed up, but only for "
                "this PC.\n\nAdd an email to reach it from your other devices. Your existing history "
                "is kept."));
            info->setObjectName(QStringLiteral("Body"));
            info->setWordWrap(true);
            bodyL->addWidget(info);
            auto *email = new QLineEdit;
            email->setPlaceholderText(QStringLiteral("email"));
            email->setObjectName(QStringLiteral("Entry"));
            bodyL->addWidget(email);
            auto *pw = new QLineEdit;
            pw->setPlaceholderText(QStringLiteral("choose a password"));
            pw->setEchoMode(QLineEdit::Password);
            pw->setObjectName(QStringLiteral("Entry"));
            bodyL->addWidget(pw);
            auto *row = new QHBoxLayout;
            auto *up = new QPushButton(QStringLiteral("Sync across devices"));
            up->setObjectName(QStringLiteral("Primary"));
            auto *stop = new QPushButton(QStringLiteral("Stop syncing"));
            stop->setObjectName(QStringLiteral("Ghost"));
            row->addWidget(up);
            row->addWidget(stop);
            row->addStretch(1);
            bodyL->addLayout(row);
            QObject::connect(up, &QPushButton::clicked, dlg, [=] {
                const QString a = email->text().trimmed(), s = pw->text();
                if (a.isEmpty() || s.isEmpty()) {
                    setStatus(QStringLiteral("Enter an email and password."), kDanger);
                    return;
                }
                setStatus(QStringLiteral("Linking your account…"));
                client->upgradeToAccount(a, s, [=](bool ok, const QString &msg) {
                    setStatus(msg, ok ? kHint : kDanger);
                    if (ok) {
                        (*rebuild)();
                        if (onChanged)
                            onChanged();
                    }
                });
            });
            QObject::connect(stop, &QPushButton::clicked, dlg, [=] {
                settings->set(QStringLiteral("sync_enabled"), false);
                client->signOut();
                setStatus(QStringLiteral("Syncing off."));
                (*rebuild)();
                if (onChanged)
                    onChanged();
            });
            return;
        }

        if (client->isSignedIn()) {
            auto *l = new QLabel(QStringLiteral("Signed in as %1").arg(client->email()));
            l->setObjectName(QStringLiteral("Body"));
            bodyL->addWidget(l);
            auto *pause = new QCheckBox(QStringLiteral("Pause syncing"));
            pause->setChecked(!settings->getBool(QStringLiteral("sync_enabled")));
            bodyL->addWidget(pause);
            QObject::connect(pause, &QCheckBox::toggled, dlg, [=](bool checked) {
                settings->set(QStringLiteral("sync_enabled"), !checked);
                setStatus(engine->status());
                if (onChanged)
                    onChanged();
            });
            auto *out = new QPushButton(QStringLiteral("Sign out"));
            out->setObjectName(QStringLiteral("Ghost"));
            bodyL->addWidget(out, 0, Qt::AlignLeft);
            QObject::connect(out, &QPushButton::clicked, dlg, [=] {
                client->signOut();
                setStatus(QStringLiteral("Signed out."));
                (*rebuild)();
                if (onChanged)
                    onChanged();
            });
            return;
        }

        // Not signed in — offer sign in / create account.
        auto *email = new QLineEdit;
        email->setPlaceholderText(QStringLiteral("email"));
        email->setObjectName(QStringLiteral("Entry"));
        bodyL->addWidget(email);
        auto *pw = new QLineEdit;
        pw->setPlaceholderText(QStringLiteral("password"));
        pw->setEchoMode(QLineEdit::Password);
        pw->setObjectName(QStringLiteral("Entry"));
        bodyL->addWidget(pw);
        auto *row = new QHBoxLayout;
        auto *signin = new QPushButton(QStringLiteral("Sign in"));
        signin->setObjectName(QStringLiteral("Primary"));
        auto *create = new QPushButton(QStringLiteral("Create account"));
        create->setObjectName(QStringLiteral("Ghost"));
        row->addWidget(signin);
        row->addWidget(create);
        row->addStretch(1);
        bodyL->addLayout(row);
        QObject::connect(signin, &QPushButton::clicked, dlg, [=] {
            const QString a = email->text().trimmed(), s = pw->text();
            if (a.isEmpty() || s.isEmpty()) {
                setStatus(QStringLiteral("Enter an email and password."), kDanger);
                return;
            }
            setStatus(QStringLiteral("Signing in…"));
            client->signIn(a, s, [=](bool ok, const QString &err) {
                if (ok) {
                    setStatus(QStringLiteral("Signed in as %1").arg(a));
                    (*rebuild)();
                    if (onChanged)
                        onChanged();
                } else {
                    setStatus(err, kDanger);
                }
            });
        });
        QObject::connect(create, &QPushButton::clicked, dlg, [=] {
            const QString a = email->text().trimmed(), s = pw->text();
            if (a.isEmpty() || s.isEmpty()) {
                setStatus(QStringLiteral("Enter an email and password."), kDanger);
                return;
            }
            setStatus(QStringLiteral("Creating account…"));
            client->signUp(a, s, [=](bool ok, const QString &msg) {
                setStatus(msg, ok ? kHint : kDanger);
                if (ok) {
                    (*rebuild)();
                    if (onChanged)
                        onChanged();
                }
            });
        });
    };

    (*rebuild)();
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}
