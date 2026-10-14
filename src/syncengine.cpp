#include "syncengine.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>

#include "db.h"
#include "settings.h"
#include "supabaseclient.h"

namespace {
const QString kTableItems = QStringLiteral("clipboard_sync");
const QString kTableHeartbeat = QStringLiteral("device_heartbeat");
constexpr int kDebounceMs = 500;
constexpr int kBatchLimit = 200;

QString utcNowIso() {
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}
}  // namespace

SyncEngine::SyncEngine(db::Database *db, SupabaseClient *client, Settings *settings, QObject *parent)
    : QObject(parent), db_(db), client_(client), settings_(settings), debounce_(new QTimer(this)) {
    debounce_->setSingleShot(true);
    debounce_->setInterval(kDebounceMs);
    connect(debounce_, &QTimer::timeout, this, &SyncEngine::flush);
}

bool SyncEngine::active() const {
    return settings_->getBool(QStringLiteral("sync_enabled")) && client_->isConfigured() &&
           client_->isSignedIn();
}

void SyncEngine::start() { fullPass(); }
void SyncEngine::kick() { fullPass(); }

void SyncEngine::notify() {
    // Debounce: a burst of rapid copies becomes one upload.
    debounce_->start();
}

void SyncEngine::fullPass() {
    ensureSignedIn([this] {
        if (active()) {
            heartbeat();
            prune();
        }
        flush();
    });
}

void SyncEngine::ensureSignedIn(std::function<void()> then) {
    // First run with no account: take an anonymous identity so sync works with no
    // signup step. Nothing happens if syncing is off or we're already signed in.
    if (!settings_->getBool(QStringLiteral("sync_enabled")) || !client_->isConfigured() ||
        client_->isSignedIn()) {
        then();
        return;
    }
    client_->signInAnonymously([this, then](bool ok, const QString &err) {
        if (!ok)
            lastError_ = err;
        emit statusChanged();
        then();
    });
}

void SyncEngine::flush() {
    if (!active())
        return;
    const QList<db::Item> pending = db_->unsyncedTextItems(kBatchLimit);
    if (pending.isEmpty())
        return;
    QJsonArray rows;
    QList<qint64> ids;
    for (const db::Item &it : pending) {
        rows.append(QJsonObject{
            {QStringLiteral("user_id"), client_->userId()},
            {QStringLiteral("content_type"), QStringLiteral("text")},
            {QStringLiteral("content"), it.content},
            {QStringLiteral("created_at"), it.createdAt},
        });
        ids.append(it.id);
    }
    client_->insertRows(kTableItems, rows, [this, ids](bool ok, const QString &err) {
        if (ok) {
            db_->markSynced(ids);
            lastError_.clear();
        } else {
            lastError_ = err;
        }
        emit statusChanged();
    });
}

void SyncEngine::heartbeat() {
    if (!active())
        return;
    client_->upsertRow(kTableHeartbeat,
                       QJsonObject{{QStringLiteral("user_id"), client_->userId()},
                                   {QStringLiteral("last_seen"), utcNowIso()}},
                       QStringLiteral("user_id"), [this](bool ok, const QString &err) {
                           if (!ok) {
                               lastError_ = err;
                               emit statusChanged();
                           }
                       });
}

void SyncEngine::prune() {
    const int days = settings_->getInt(QStringLiteral("retention_days"));
    if (days <= 0)
        return;
    db_->pruneOlderThan(days);
    if (active()) {
        const QString cutoff =
            QDateTime::currentDateTimeUtc().addDays(-days).toString(Qt::ISODateWithMs);
        client_->deleteOlderThan(kTableItems, cutoff, [](bool, const QString &) {});
    }
}

QString SyncEngine::status() const {
    if (!client_->isConfigured())
        return QStringLiteral("Not configured");
    if (!settings_->getBool(QStringLiteral("sync_enabled")))
        return QStringLiteral("Paused");
    if (!client_->isSignedIn())
        return lastError_.isEmpty() ? QStringLiteral("Connecting…")
                                    : QStringLiteral("Error: %1").arg(lastError_);
    if (!lastError_.isEmpty())
        return QStringLiteral("Error: %1").arg(lastError_);
    if (client_->isAnonymous())
        return QStringLiteral("Syncing (this device only)");
    return QStringLiteral("Syncing as %1").arg(client_->email());
}
