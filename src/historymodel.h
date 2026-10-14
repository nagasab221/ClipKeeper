#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QPixmap>

#include "db.h"

// List model backing the popup's QListView. Holds either history items or
// snippets (the two tabs) and exposes the fields the delegate paints via custom
// roles. Image/first-file thumbnails are decoded lazily and cached.
class HistoryModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Mode { History, Snippets };
    enum Roles {
        IdRole = Qt::UserRole + 1,
        TypeRole,       // "text" | "image" | "files" | "snippet"
        PreviewRole,
        ContentRole,
        PinnedRole,
        CreatedRole,    // ISO-8601
        ThumbnailRole,  // QPixmap, or invalid when the row has no thumbnail
        DimsRole,       // "W x H" for images
        KeywordRole,
        ExpansionRole,
    };

    explicit HistoryModel(QObject *parent = nullptr);

    void setHistory(const QList<db::Item> &items);
    void setSnippets(const QList<db::Snippet> &snips);
    Mode mode() const { return mode_; }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    db::Item itemAt(int row) const;
    db::Snippet snippetAt(int row) const;

private:
    QPixmap thumbFor(const db::Item &it) const;

    Mode mode_ = History;
    QList<db::Item> items_;
    QList<db::Snippet> snips_;
    mutable QHash<QString, QPixmap> thumbs_;
};
