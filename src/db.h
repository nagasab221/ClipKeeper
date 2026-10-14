#pragma once

#include <QList>
#include <QSqlDatabase>
#include <QString>
#include <optional>

// SQLite storage with an FTS5 full-text index — the Qt port of the Python app's
// db.py. Text lives in clipboard_items.content; images store a PNG *path* there
// (the binary is on disk under imageDir); files store a newline-joined path list.
// clipboard_fts is an external-content FTS5 table kept in sync by three triggers.
namespace db {

struct Item {
    qint64 id = 0;
    QString contentType;   // "text" | "image" | "files"
    QString content;       // text body, PNG path, or newline-joined file paths
    QString preview;       // truncated single-line preview for the list
    QString contentHash;   // dedup key (images: hash of the pixels)
    bool pinned = false;
    QString createdAt;     // ISO-8601 UTC
    bool synced = false;
};

struct Snippet {
    qint64 id = 0;
    QString triggerKeyword;
    QString expansionText;
    QString createdAt;
};

// Single-line, truncated preview for list display (mirrors db.make_preview).
QString makePreview(const QString &contentType, const QString &content);

class Database {
public:
    explicit Database(const QString &path, const QString &connectionName = QStringLiteral("clipkeeper"));
    ~Database();

    bool isOpen() const { return open_; }

    qint64 addItem(const QString &contentType, const QString &content,
                   const QString &preview = QString(),
                   const QString &contentHash = QString());

    // Has this exact content just been stored? Checks the shared DB rather than
    // memory, so it also catches a stray second instance capturing the same event.
    bool hashSeenRecently(const QString &contentHash, int seconds = 5);

    std::optional<Item> lastItem();

    // Empty query -> recent items (pinned first, newest first). Otherwise FTS5
    // prefix match ranked by bm25, pinned first.
    QList<Item> search(const QString &query, int limit = 200);

    int count();
    void setPinned(qint64 id, bool pinned);

    // Deletes the row. Only *image* rows unlink their backing PNG — a "files"
    // row must NEVER delete the user's real files.
    void deleteItem(qint64 id);

    // Delete unpinned items older than `days`; returns how many went. Pinned
    // items never expire. Backing PNGs go with their (image) rows.
    int pruneOlderThan(int days);

    // --- snippets ---
    qint64 addSnippet(const QString &keyword, const QString &text);
    void deleteSnippet(qint64 id);
    QList<Snippet> allSnippets();

    // --- sync bookkeeping (Phase 5) ---
    QList<Item> unsyncedTextItems(int limit = 200);
    void markSynced(const QList<qint64> &ids);

private:
    void initSchema();

    QString conn_;
    QSqlDatabase sdb_;
    bool open_ = false;
};

}  // namespace db
