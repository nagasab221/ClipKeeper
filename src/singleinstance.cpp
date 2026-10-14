#include "singleinstance.h"

#include <QLocalServer>
#include <QLocalSocket>

namespace {
constexpr char kToken[] = "CLIPKEEPER_SHOW";
}

SingleInstance::SingleInstance(QObject *parent) : QObject(parent) {}

bool SingleInstance::acquire() {
    // Is a primary already listening?
    {
        QLocalSocket probe;
        probe.connectToServer(name_);
        if (probe.waitForConnected(150)) {
            probe.disconnectFromServer();
            return false;  // someone's already home
        }
    }
    // No primary answered — clear any stale pipe left by a crash, then listen.
    QLocalServer::removeServer(name_);
    server_ = new QLocalServer(this);
    if (!server_->listen(name_)) {
        delete server_;
        server_ = nullptr;
        return false;
    }
    connect(server_, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *conn = server_->nextPendingConnection()) {
            connect(conn, &QLocalSocket::readyRead, conn, [this, conn] {
                if (conn->readAll().contains(kToken))
                    emit showRequested();
                conn->disconnectFromServer();
                conn->deleteLater();
            });
        }
    });
    return true;
}

void SingleInstance::notifyExisting() {
    QLocalSocket sock;
    sock.connectToServer(name_);
    if (sock.waitForConnected(500)) {
        sock.write(kToken);
        sock.flush();
        sock.waitForBytesWritten(500);
        sock.disconnectFromServer();
    }
}
