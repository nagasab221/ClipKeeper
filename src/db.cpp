#include "db.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QVariant>

namespace db {

namespace {

constexpr int kPreviewLen = 120;

QString utcNowIso() {
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

QString utcIsoOffset(qint64 secs) {
    return QDateTime::currentDateTimeUtc().addSecs(secs).toString(Qt::ISODateWithMs);
}

// Turn a raw user string into a safe FTS5 prefix MATCH expression: each \w+ run
// becomes a quoted prefix term ("foo"*), ANDed by the implicit space operator.
// Empty when there's nothing searchable, so the caller falls back to recent items.
QString ftsQuery(const QString &text) {
    static const QRegularExpression re(
        QStringLiteral("\\w+"), QRegularExpression::UseUnicodePropertiesOption);
    QStringList terms;
    auto it = re.globalMatch(text);
    while (it.hasNext())
        terms << QStringLiteral("\"%1\"*").arg(it.next().captured(0));
    return terms.join(QLatin1Char(' '));
}

Item rowToItem(const QSqlQuery &q) {
    const QSqlRecord r = q.record();
    Item it;
    it.id = q.value(r.indexOf(QStringLiteral("id"))).toLongLong();
    it.contentType = q.value(r.indexOf(QStringLiteral("content_type"))).toString();
    it.content = q.value(r.indexOf(QStringLiteral("content"))).toString();
    it.preview = q.value(r.indexOf(QStringLiteral("preview"))).toString();
    it.contentHash = q.value(r.indexOf(QStringLiteral("content_hash"))).toString();
    it.pinned = q.value(r.indexOf(QStringLiteral("pinned"))).toInt() != 0;
    it.createdAt = q.value(r.indexOf(QStringLiteral("created_at"))).toString();
    it.synced = q.value(r.indexOf(QStringLiteral("synced"))).toInt() != 0;
    return it;
}

}  // namespace

QString makePreview(const QString &contentType, const QString &content) {
    if (contentType != QLatin1String("text"))
        return content;
    static const QRegularExpression ws(
        QStringLiteral("\\s+"), QRegularExpression::UseUnicodePropertiesOption);
    const QString oneLine =
        content.split(ws, Qt::SkipEmptyParts).join(QLatin1Char(' '));
    if (oneLine.size() > kPreviewLen)
        return oneLine.left(kPreviewLen) + QChar(0x2026);  // …
    return oneLine;
}

Database::Database(const QString &path, const QString &connectionName)
    : conn_(connectionName) {
    sdb_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn_);
    sdb_.setDatabaseName(path);
    if (!sdb_.open()) {
        qWarning() << "[db] could not open" << path << ":" << sdb_.lastError().text();
        return;
    }
    open_ = true;
    QSqlQuery(sdb_).exec(QStringLiteral("PRAGMA journal_mode=WAL;"));
    initSchema();
}

Database::~Database() {
    if (sdb_.isOpen())
        sdb_.close();
    sdb_ = QSqlDatabase();
    QSqlDatabase::removeDatabase(conn_);
}

void Database::initSchema() {
    static const char *stmts[] = {
        "CREATE TABLE IF NOT EXISTS clipboard_items ("
        "  id           INTEGER PRIMARY KEY,"
        "  content_type TEXT    NOT NULL,"
        "  content      TEXT    NOT NULL,"
        "  preview      TEXT,"
        "  content_hash TEXT,"
        "  source_app   TEXT,"
        "  pinned       INTEGER NOT NULL DEFAULT 0,"
        "  created_at   TEXT    NOT NULL,"
        "  synced       INTEGER NOT NULL DEFAULT 0)",

        "CREATE VIRTUAL TABLE IF NOT EXISTS clipboard_fts USING fts5("
        "  content, content='clipboard_items', content_rowid='id')",

        "CREATE TRIGGER IF NOT EXISTS clipboard_ai AFTER INSERT ON clipboard_items BEGIN"
        "  INSERT INTO clipboard_fts(rowid, content) VALUES (new.id, new.content);"
        "END",

        "CREATE TRIGGER IF NOT EXISTS clipboard_ad AFTER DELETE ON clipboard_items BEGIN"
        "  INSERT INTO clipboard_fts(clipboard_fts, rowid, content) VALUES ('delete', old.id, old.content);"
        "END",

        "CREATE TRIGGER IF NOT EXISTS clipboard_au AFTER UPDATE ON clipboard_items BEGIN"
        "  INSERT INTO clipboard_fts(clipboard_fts, rowid, content) VALUES ('delete', old.id, old.content);"
        "  INSERT INTO clipboard_fts(rowid, content) VALUES (new.id, new.content);"
        "END",

        "CREATE TABLE IF NOT EXISTS snippets ("
        "  id              INTEGER PRIMARY KEY,"
        "  trigger_keyword TEXT UNIQUE,"
        "  expansion_text  TEXT NOT NULL,"
        "  created_at      TEXT NOT NULL)",
    };
    for (const char *sql : stmts) {
        QSqlQuery q(sdb_);
        if (!q.exec(QLatin1String(sql)))
            qWarning() << "[db] schema stmt failed:" << q.lastError().text();
    }
}

qint64 Database::addItem(const QString &contentType, const QString &content,
                         const QString &preview, const QString &contentHash) {
    const QString pv = preview.isNull() ? makePreview(contentType, content) : preview;
    QSqlQuery q(sdb_);
    q.prepare(QStringLiteral(
        "INSERT INTO clipboard_items "
        "(content_type, content, preview, source_app, created_at, content_hash) "
        "VALUES (?, ?, ?, NULL, ?, ?)"));
    q.addBindValue(contentType);
    q.addBindValue(content);
    q.addBindValue(pv);
    q.addBindValue(utcNowIso());
    q.addBindValue(contentHash.isNull() ? QVariant(QMetaType(QMetaType::QString))
                                        : QVariant(contentHash));
    if (!q.exec()) {
        qWarning() << "[db] addItem failed:" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toLongLong();
}

bool Database::hashSeenRecently(const QString &contentHash, int seconds) {
    if (contentHash.isEmpty())
        return false;
    QSqlQuery q(sdb_);
    q.prepare(QStringLiteral(
        "SELECT 1 FROM clipboard_items "
        "WHERE content_hash = ? AND created_at >= ? LIMIT 1"));
    q.addBindValue(contentHash);
    q.addBindValue(utcIsoOffset(-seconds));
    if (!q.exec()) {
        qWarning() << "[db] hashSeenRecently failed:" << q.lastError().text();
        return false;
    }
    return q.next();
}

std::optional<Item> Database::lastItem() {
    QSqlQuery q(sdb_);
    if (!q.exec(QStringLiteral("SELECT * FROM clipboard_items ORDER BY id DESC LIMIT 1")))
        return std::nullopt;
    if (!q.next())
        return std::nullopt;
    return rowToItem(q);
}

QList<Item> Database::search(const QString &query, int limit) {
    const QString match = ftsQuery(query);
    QSqlQuery q(sdb_);
    if (match.isEmpty()) {
        q.prepare(QStringLiteral(
            "SELECT * FROM clipboard_items "
            "ORDER BY pinned DESC, id DESC LIMIT ?"));
        q.addBindValue(limit);
    } else {
        q.prepare(QStringLiteral(
            "SELECT ci.* FROM clipboard_items ci "
            "JOIN clipboard_fts ON clipboard_fts.rowid = ci.id "
            "WHERE clipboard_fts MATCH ? "
            "ORDER BY ci.pinned DESC, bm25(clipboard_fts) LIMIT ?"));
        q.addBindValue(match);
        q.addBindValue(limit);
    }
    QList<Item> out;
    if (!q.exec()) {
        qWarning() << "[db] search failed:" << q.lastError().text();
        return out;
    }
    while (q.next())
        out << rowToItem(q);
    return out;
}

int Database::count() {
    QSqlQuery q(sdb_);
    if (q.exec(QStringLiteral("SELECT COUNT(*) FROM clipboard_items")) && q.next())
        return q.value(0).toInt();
    return 0;
}

void Database::setPinned(qint64 id, bool pinned) {
    QSqlQuery q(sdb_);
    q.prepare(QStringLiteral("UPDATE clipboard_items SET pinned = ? WHERE id = ?"));
    q.addBindValue(pinned ? 1 : 0);
    q.addBindValue(id);
    if (!q.exec())
        qWarning() << "[db] setPinned failed:" << q.lastError().text();
}

void Database::deleteItem(qint64 id) {
    QSqlQuery sel(sdb_);
    sel.prepare(QStringLiteral(
        "SELECT content_type, content FROM clipboard_items WHERE id = ?"));
    sel.addBindValue(id);
    QString type, content;
    if (sel.exec() && sel.next()) {
        type = sel.value(0).toString();
        content = sel.value(1).toString();
    }
    QSqlQuery del(sdb_);
    del.prepare(QStringLiteral("DELETE FROM clipboard_items WHERE id = ?"));
    del.addBindValue(id);
    if (!del.exec()) {
        qWarning() << "[db] deleteItem failed:" << del.lastError().text();
        return;
    }
    // Only image entries own a backing PNG. A "files" row points at the user's
    // real files and must never delete them.
    if (type == QLatin1String("image") && !content.isEmpty())
        QFile::remove(content);
}

int Database::pruneOlderThan(int days) {
    if (days <= 0)
        return 0;
    const QString cutoff =
        QDateTime::currentDateTimeUtc().addDays(-days).toString(Qt::ISODateWithMs);

    QSqlQuery sel(sdb_);
    sel.prepare(QStringLiteral(
        "SELECT content_type, content FROM clipboard_items "
        "WHERE pinned = 0 AND created_at < ?"));
    sel.addBindValue(cutoff);
    QStringList doomedPngs;
    int n = 0;
    if (sel.exec()) {
        while (sel.next()) {
            ++n;
            if (sel.value(0).toString() == QLatin1String("image"))
                doomedPngs << sel.value(1).toString();
        }
    }
    if (n == 0)
        return 0;

    QSqlQuery del(sdb_);
    del.prepare(QStringLiteral(
        "DELETE FROM clipboard_items WHERE pinned = 0 AND created_at < ?"));
    del.addBindValue(cutoff);
    if (!del.exec()) {
        qWarning() << "[db] prune failed:" << del.lastError().text();
        return 0;
    }
    for (const QString &png : doomedPngs)
        if (!png.isEmpty())
            QFile::remove(png);
    return n;
}

qint64 Database::addSnippet(const QString &keyword, const QString &text) {
    QSqlQuery q(sdb_);
    q.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO snippets "
        "(trigger_keyword, expansion_text, created_at) VALUES (?, ?, ?)"));
    q.addBindValue(keyword);
    q.addBindValue(text);
    q.addBindValue(utcNowIso());
    if (!q.exec()) {
        qWarning() << "[db] addSnippet failed:" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toLongLong();
}

void Database::deleteSnippet(qint64 id) {
    QSqlQuery q(sdb_);
    q.prepare(QStringLiteral("DELETE FROM snippets WHERE id = ?"));
    q.addBindValue(id);
    q.exec();
}

QList<Snippet> Database::allSnippets() {
    QList<Snippet> out;
    QSqlQuery q(sdb_);
    if (!q.exec(QStringLiteral(
            "SELECT * FROM snippets ORDER BY trigger_keyword COLLATE NOCASE")))
        return out;
    while (q.next()) {
        const QSqlRecord r = q.record();
        Snippet s;
        s.id = q.value(r.indexOf(QStringLiteral("id"))).toLongLong();
        s.triggerKeyword = q.value(r.indexOf(QStringLiteral("trigger_keyword"))).toString();
        s.expansionText = q.value(r.indexOf(QStringLiteral("expansion_text"))).toString();
        s.createdAt = q.value(r.indexOf(QStringLiteral("created_at"))).toString();
        out << s;
    }
    return out;
}

QList<Item> Database::unsyncedTextItems(int limit) {
    QList<Item> out;
    QSqlQuery q(sdb_);
    q.prepare(QStringLiteral(
        "SELECT * FROM clipboard_items "
        "WHERE synced = 0 AND content_type = 'text' ORDER BY id LIMIT ?"));
    q.addBindValue(limit);
    if (!q.exec())
        return out;
    while (q.next())
        out << rowToItem(q);
    return out;
}

void Database::markSynced(const QList<qint64> &ids) {
    if (ids.isEmpty())
        return;
    QStringList placeholders;
    for (int i = 0; i < ids.size(); ++i)
        placeholders << QStringLiteral("?");
    QSqlQuery q(sdb_);
    q.prepare(QStringLiteral("UPDATE clipboard_items SET synced = 1 WHERE id IN (%1)")
                  .arg(placeholders.join(QLatin1Char(','))));
    for (qint64 id : ids)
        q.addBindValue(id);
    if (!q.exec())
        qWarning() << "[db] markSynced failed:" << q.lastError().text();
}

}  // namespace db
