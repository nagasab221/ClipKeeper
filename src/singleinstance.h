#pragma once

#include <QObject>
#include <QString>

class QLocalServer;

// Single-instance guard via a named QLocalServer (a named pipe on Windows) — the
// Qt equivalent of single_instance.py's loopback socket. The first instance
// listens; a later launch fails to bind, pings the primary to surface its popup,
// and exits. This matters once "start with Windows" is on: otherwise the logon
// copy and a manual launch both capture into the same DB and grab different
// hotkeys.
class SingleInstance : public QObject {
    Q_OBJECT
public:
    explicit SingleInstance(QObject *parent = nullptr);

    bool acquire();        // true if we became the primary instance
    void notifyExisting(); // ask the running instance to show its popup

signals:
    void showRequested();

private:
    QLocalServer *server_ = nullptr;
    QString name_ = QStringLiteral("ClipKeeperQt.singleton");
};
