#pragma once

#include <QObject>
#include <QString>
#include <functional>

namespace db { class Database; }
class SupabaseClient;
class Settings;
class QTimer;

// Background sync to Supabase — the Qt port of sync.py, but async on the GUI
// thread (QNetworkAccessManager) instead of a worker thread + queue. New text
// items are debounced 500ms then uploaded; anything unsynced in the DB is a
// backlog picked up on start; images are never uploaded; a heartbeat keeps the
// free tier awake; cloud retention mirrors the local setting. Nothing here can
// block a paste — failures are recorded and retried next pass.
class SyncEngine : public QObject {
    Q_OBJECT
public:
    SyncEngine(db::Database *db, SupabaseClient *client, Settings *settings,
               QObject *parent = nullptr);

    void start();   // ensure signed in, then heartbeat + prune + backlog flush
    void notify();  // a new text item landed — debounce, then flush
    void kick();    // full pass after the account state changes

    bool active() const;
    QString status() const;
    QString lastError() const { return lastError_; }

signals:
    void statusChanged();

private:
    void fullPass();
    void ensureSignedIn(std::function<void()> then);
    void flush();
    void heartbeat();
    void prune();

    db::Database *db_;
    SupabaseClient *client_;
    Settings *settings_;
    QTimer *debounce_;
    QString lastError_;
};
