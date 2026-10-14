// ClipKeeper (Qt rewrite) — entry point.
//
//   ClipKeeper.exe                 run the app: tray + clipboard capture
//   ClipKeeper.exe --capture-cli   headless capture printout (debug)
//   ClipKeeper.exe --list          list recent history items (no GUI)
//   ClipKeeper.exe --search TERM    full-text search history (no GUI)
//   ClipKeeper.exe --selftest      headless smoke check (comes up, exits 0)
//
// The popup, hotkey, tray actions, and sync arrive in later phases.

#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QDataStream>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QTextStream>
#include <QTimer>

#include "accountdialog.h"
#include "appicon.h"
#include "autostart.h"
#include "capture.h"
#include "config.h"
#include "db.h"
#include "hotkey.h"
#include "keyboardhook.h"
#include "popup.h"
#include "settings.h"
#include "setupdialog.h"
#include "singleinstance.h"
#include "supabaseclient.h"
#include "syncengine.h"
#include "winclip.h"
#include "winhistory.h"

namespace {

int runList() {
    db::Database database(cfg::dbPath());
    if (!database.isOpen())
        return 1;
    const QList<db::Item> items = database.search(QString(), 200);
    QTextStream out(stdout);
    out << database.count() << " item(s) total — showing " << items.size()
        << " most recent:\n";
    for (const db::Item &it : items)
        out << (it.pinned ? QStringLiteral(" *") : QStringLiteral("  ")) << "#" << it.id
            << " [" << it.contentType << "] " << it.preview << "\n";
    return 0;
}

int runSearch(const QString &term) {
    db::Database database(cfg::dbPath());
    if (!database.isOpen())
        return 1;
    const QList<db::Item> items = database.search(term, 200);
    QTextStream out(stdout);
    out << items.size() << " result(s) for \"" << term << "\":\n";
    for (const db::Item &it : items)
        out << "  #" << it.id << " [" << it.contentType << "] " << it.preview << "\n";
    return 0;
}

int runCaptureCli() {
    db::Database database(cfg::dbPath());
    if (!database.isOpen())
        return 1;
    ClipboardWatcher capture(&database);
    QTextStream out(stdout);
    QObject::connect(&capture, &ClipboardWatcher::itemStored,
                     [&out](qint64 id, const QString &type, const QString &preview) {
                         out << "[" << QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))
                             << "] #" << id << "  " << type.rightJustified(5) << "  " << preview << "\n";
                         out.flush();
                     });
    capture.start();
    out << cfg::kAppName << " — headless capture (Ctrl+C to stop). Watching clipboard…\n";
    out << "  DB: " << cfg::dbPath() << "\n";
    out.flush();
    return qApp->exec();
}

// Seeds a few representative rows and shows the popup, for eyeballing / screenshots.
int runPopupDemo() {
    db::Database database(cfg::dbPath());
    if (!database.isOpen())
        return 1;
    database.addItem(QStringLiteral("text"), QStringLiteral("Welcome to ClipKeeper — the Qt rewrite"));
    database.addItem(QStringLiteral("text"), QStringLiteral("https://github.com/telegramdesktop/tdesktop"));
    const qint64 pid = database.addItem(
        QStringLiteral("text"), QStringLiteral("PINNED: sign the build before release"));
    database.setPinned(pid, true);
    database.addItem(QStringLiteral("text"),
                     QStringLiteral("The quick brown fox jumps over the lazy dog, then keeps running "
                                    "far past the visible width of the row so the delegate has to "
                                    "elide it cleanly with an ellipsis."));
    database.addItem(QStringLiteral("files"),
                     QStringLiteral("C:\\Users\\fabri\\alpha.txt\nC:\\Users\\fabri\\beta.txt\n"
                                    "C:\\Users\\fabri\\gamma.txt"),
                     QStringLiteral("3 files"));

    auto *popup = new Popup(&database);
    popup->showPopup(nullptr);
    return qApp->exec();
}

// Write a multi-size .ico of the app icon (PNG-compressed entries), so the exe /
// installer icon matches the drawn tray glyph. Usage: --export-icon <path>
int runExportIcon(const QString &path) {
    const QIcon icon = ui::appIcon();
    const int sizes[] = {16, 24, 32, 48, 64, 128, 256};
    const int n = int(sizeof(sizes) / sizeof(sizes[0]));
    QByteArray png[7];
    for (int i = 0; i < n; ++i) {
        QImage img = icon.pixmap(QSize(sizes[i], sizes[i])).toImage();
        if (img.width() != sizes[i] || img.height() != sizes[i])
            img = img.scaled(sizes[i], sizes[i], Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        img = img.convertToFormat(QImage::Format_ARGB32);
        QBuffer buf(&png[i]);
        buf.open(QIODevice::WriteOnly);
        img.save(&buf, "PNG");
    }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        QTextStream(stdout) << "could not write " << path << "\n";
        return 1;
    }
    QDataStream ds(&f);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds << quint16(0) << quint16(1) << quint16(n);  // ICONDIR: reserved, type=icon, count
    quint32 offset = 6 + 16 * quint32(n);
    for (int i = 0; i < n; ++i) {
        const quint8 wh = sizes[i] >= 256 ? 0 : quint8(sizes[i]);
        ds << wh << wh << quint8(0) << quint8(0);            // width, height, colours, reserved
        ds << quint16(1) << quint16(32);                     // planes, bit count
        ds << quint32(png[i].size()) << offset;              // bytes in resource, offset
        offset += quint32(png[i].size());
    }
    for (int i = 0; i < n; ++i)
        f.write(png[i]);
    QTextStream(stdout) << "wrote " << path << "\n";
    return 0;
}

// Opens the Account dialog for eyeballing / screenshots.
int runAccountDemo() {
    static Settings settings;
    static db::Database database(cfg::dbPath());
    SupabaseClient *client = SupabaseClient::fromConfig(qApp);
    static SyncEngine sync(&database, client, &settings);
    showAccountDialog(nullptr, client, &sync, &settings, [] {});
    return qApp->exec();
}

int runApp(bool selftest, bool forceWinV, bool forceSetup) {
    // Single-instance guard (skipped for --selftest so automated smoke runs don't
    // collide with a running copy). A second launch surfaces the primary.
    SingleInstance instance;
    if (!selftest && !instance.acquire()) {
        instance.notifyExisting();
        qInfo() << "ClipKeeper is already running — surfaced the existing window.";
        return 0;
    }
    Settings settings;

    const bool trayAvailable = QSystemTrayIcon::isSystemTrayAvailable();
    if (!trayAvailable) {
        qWarning() << "System tray is not available on this session.";
        if (selftest)
            return 2;
    }

    db::Database database(cfg::dbPath());
    if (!database.isOpen()) {
        qWarning() << "Database failed to open at" << cfg::dbPath();
        if (selftest)
            return 3;
    }

    ClipboardWatcher capture(&database);
    capture.start();

    // Supabase sync (text only). Anonymous sign-in on first launch; new text items
    // are debounced then uploaded. Never blocks capture or paste.
    SupabaseClient *client = SupabaseClient::fromConfig(qApp);
    SyncEngine sync(&database, client, &settings);
    QObject::connect(&capture, &ClipboardWatcher::itemStored, &sync,
                     [&sync](qint64, const QString &type, const QString &) {
                         if (type == QLatin1String("text"))
                             sync.notify();
                     });
    QObject::connect(&sync, &SyncEngine::statusChanged, qApp,
                     [&sync] { qInfo().noquote() << "  [sync]" << sync.status(); });

    Popup popup(&database, &settings);
    QObject::connect(&instance, &SingleInstance::showRequested, &popup,
                     [&popup] { popup.showPopup(winclip::foregroundWindow()); });

    // Global hotkey (fallback chain) + Win+V low-level hook (gated on the setting).
    // Both capture the previously active window before the popup takes focus, so
    // paste-back returns to the right app.
    auto onHotkey = [&popup] { popup.toggle(winclip::foregroundWindow()); };
    HotkeyListener hotkey;
    QObject::connect(&hotkey, &HotkeyListener::triggered, &popup, [onHotkey] { onHotkey(); });
    hotkey.start();

    WinVHook winv;
    QObject::connect(&winv, &WinVHook::triggered, &popup, [onHotkey] { onHotkey(); });
    auto applyWinV = [&] {
        const bool want = settings.getBool(QStringLiteral("use_win_v")) || forceWinV;
        if (want && !winv.installed())
            winv.install();
        else if (!want && winv.installed())
            winv.uninstall();
    };
    applyWinV();
    QObject::connect(&popup, &Popup::applyWinVRequested, &popup, [applyWinV] { applyWinV(); });

    if (settings.getBool(QStringLiteral("disable_windows_history")))
        winhistory::ensureDisabled();

    auto *tray = new QSystemTrayIcon(ui::appIcon(), qApp);
    tray->setToolTip(cfg::kAppName);
    auto *menu = new QMenu();
    QAction *openAct = menu->addAction(QStringLiteral("Open ClipKeeper"));
    menu->addSeparator();
    QAction *accountAct = menu->addAction(QStringLiteral("Account…"));
    QAction *settingsAct = menu->addAction(QStringLiteral("Settings…"));
    QAction *autostartAct = menu->addAction(QStringLiteral("Start with Windows"));
    autostartAct->setCheckable(true);
    autostartAct->setChecked(autostart::isEnabled());
    menu->addSeparator();
    QAction *quitAct = menu->addAction(QStringLiteral("Quit"));
    tray->setContextMenu(menu);

    QObject::connect(quitAct, &QAction::triggered, qApp, &QApplication::quit);
    QObject::connect(openAct, &QAction::triggered, &popup, [onHotkey] { onHotkey(); });
    QObject::connect(accountAct, &QAction::triggered, &popup, [&] {
        showAccountDialog(nullptr, client, &sync, &settings, [&sync] { sync.kick(); });
    });
    QObject::connect(settingsAct, &QAction::triggered, &popup, [&settings, applyWinV] {
        showSetupDialog(nullptr, &settings, [applyWinV] { applyWinV(); });
    });
    QObject::connect(autostartAct, &QAction::triggered, [autostartAct] {
        if (autostart::isEnabled())
            autostart::disable();
        else
            autostart::enable();
        autostartAct->setChecked(autostart::isEnabled());
    });
    QObject::connect(&popup, &Popup::hideTrayChanged, tray,
                     [tray](bool hide) { tray->setVisible(!hide); });
    QObject::connect(&popup, &Popup::accountRequested, &popup, [&] {
        showAccountDialog(nullptr, client, &sync, &settings, [&sync] { sync.kick(); });
    });

    if (!settings.getBool(QStringLiteral("hide_tray_icon")))
        tray->show();
    qInfo().noquote() << cfg::kAppName << "running.";
    qInfo().noquote() << "  Data dir:" << cfg::dataDir();
    qInfo().noquote() << "  Items:" << database.count();
    {
        QStringList keys;
        if (winv.installed())
            keys << QStringLiteral("Win+V");
        if (hotkey.registered())
            keys << hotkey.activeLabel();
        qInfo().noquote() << "  Hotkey:" << (keys.isEmpty() ? QStringLiteral("none bound")
                                                            : keys.join(QStringLiteral(" or ")));
    }
    qInfo().noquote() << "  Start with Windows:" << (autostart::isEnabled() ? "on" : "off");
    qInfo().noquote() << "  Sync:" << sync.status();

    sync.start();  // anonymous sign-in + heartbeat + backlog upload (async)

    if (!selftest && (forceSetup || !settings.getBool(QStringLiteral("setup_completed")))) {
        QTimer::singleShot(400, qApp, [&settings, applyWinV] {
            showSetupDialog(nullptr, &settings, [applyWinV] { applyWinV(); });
        });
    }

    if (selftest) {
        qInfo() << "[selftest] tray + capture initialized; quitting in 1.2s";
        QTimer::singleShot(1200, qApp, [] {
            qInfo() << "[selftest] clean shutdown";
            QCoreApplication::quit();
        });
    } else if (trayAvailable) {
        tray->showMessage(cfg::kAppName, QStringLiteral("Running in the tray."),
                          ui::appIcon(), 3000);
    }

    return qApp->exec();
}

int runRestoreHistory() {
    Settings settings;
    const bool ok = winhistory::setEnabled(true);
    settings.set(QStringLiteral("use_win_v"), false);
    QTextStream out(stdout);
    out << (ok ? "Windows Clipboard History re-enabled — Win+V belongs to Windows again."
               : "Could not change the registry setting.")
        << "\n";
    return ok ? 0 : 1;
}

int runAutostart(bool enable) {
    const bool ok = enable ? autostart::enable() : autostart::disable();
    QTextStream out(stdout);
    if (!ok) {
        out << "Could not update the Run key.\n";
        return 1;
    }
    out << "Start with Windows is now " << (autostart::isEnabled() ? "on" : "off") << ".\n";
    return 0;
}

// End-to-end check that sync is really working (port of run_check_cloud).
int runCheckCloud() {
    Settings settings;
    db::Database database(cfg::dbPath());
    SupabaseClient *client = SupabaseClient::fromConfig(qApp);
    QTextStream out(stdout);
    out << "Config\n";
    out << "  project      : " << client->url() << "\n";
    out << "  key          : " << (client->hasKey() ? "set" : "MISSING") << "\n";
    out << "  sync enabled : " << (settings.getBool(QStringLiteral("sync_enabled")) ? "true" : "false") << "\n";
    out << "\nAccount\n";
    if (!client->isSignedIn()) {
        out << "  signed in    : no  (open tray -> Account, once anon sign-ins are enabled)\n";
        out.flush();
        return 0;
    }
    out << "  signed in    : yes - "
        << (client->isAnonymous() ? "anonymous (this device only)" : client->email()) << "\n";
    out << "  user id      : " << client->userId() << "\n";
    out << "\nLocal\n";
    out << "  items        : " << database.count() << "\n";
    out << "  text pending : " << database.unsyncedTextItems(100000).size() << "\n";
    out.flush();

    client->count(QStringLiteral("clipboard_sync"), [client](bool ok, int total) {
        QTextStream o(stdout);
        o << "\nCloud\n";
        if (!ok) {
            o << "  FAILED to reach the cloud\n";
            o.flush();
            qApp->quit();
            return;
        }
        o << "  rows         : " << total << "\n";
        o.flush();
        client->select(QStringLiteral("clipboard_sync"),
                       {{QStringLiteral("select"), QStringLiteral("content,created_at")},
                        {QStringLiteral("order"), QStringLiteral("created_at.desc")},
                        {QStringLiteral("limit"), QStringLiteral("3")}},
                       [client](bool, const QJsonArray &rows) {
                           QTextStream o(stdout);
                           for (const QJsonValue &v : rows) {
                               const QJsonObject r = v.toObject();
                               o << "    " << r.value(QStringLiteral("created_at")).toString().left(19)
                                 << "  " << r.value(QStringLiteral("content")).toString().simplified().left(50)
                                 << "\n";
                           }
                           o.flush();
                           client->select(QStringLiteral("device_heartbeat"),
                                          {{QStringLiteral("select"), QStringLiteral("last_seen")},
                                           {QStringLiteral("limit"), QStringLiteral("1")}},
                                          [](bool, const QJsonArray &beats) {
                                              QTextStream o(stdout);
                                              o << "  heartbeat    : "
                                                << (beats.isEmpty()
                                                        ? QStringLiteral("none yet")
                                                        : beats.first().toObject().value(QStringLiteral("last_seen")).toString())
                                                << "\n\n  Sync is working.\n";
                                              o.flush();
                                              qApp->quit();
                                          });
                       });
    });
    return qApp->exec();
}

}  // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QString::fromLatin1(cfg::kAppName));
    app.setOrganizationName(QString::fromLatin1(cfg::kAppName));
    QApplication::setQuitOnLastWindowClosed(false);
    cfg::ensureDirs();

    const QStringList args = app.arguments();
    if (args.contains(QStringLiteral("--list")))
        return runList();
    if (const int i = args.indexOf(QStringLiteral("--search")); i >= 0)
        return runSearch(i + 1 < args.size() ? args.at(i + 1) : QString());
    if (args.contains(QStringLiteral("--capture-cli")))
        return runCaptureCli();
    if (args.contains(QStringLiteral("--popup-demo")))
        return runPopupDemo();
    if (args.contains(QStringLiteral("--account-demo")))
        return runAccountDemo();
    if (const int i = args.indexOf(QStringLiteral("--export-icon")); i >= 0 && i + 1 < args.size())
        return runExportIcon(args.at(i + 1));
    if (args.contains(QStringLiteral("--restore-clipboard-history")))
        return runRestoreHistory();
    if (args.contains(QStringLiteral("--enable-autostart")))
        return runAutostart(true);
    if (args.contains(QStringLiteral("--disable-autostart")))
        return runAutostart(false);
    if (args.contains(QStringLiteral("--check-cloud")))
        return runCheckCloud();
    return runApp(args.contains(QStringLiteral("--selftest")),
                  args.contains(QStringLiteral("--win-v")),
                  args.contains(QStringLiteral("--setup")));
}
