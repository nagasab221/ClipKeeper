#include "historymodel.h"

#include <QFileInfo>
#include <QSet>

namespace {

const QSet<QString> &thumbExts() {
    static const QSet<QString> exts = {
        QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
        QStringLiteral("gif"), QStringLiteral("bmp"),  QStringLiteral("webp"),
        QStringLiteral("ico"),
    };
    return exts;
}

// "[Image 137x91]" -> "137 x 91"
QString dimsFromPreview(const QString &preview) {
    QString s = preview;
    s.remove(QLatin1Char('[')).remove(QLatin1Char(']'));
    s.replace(QStringLiteral("Image "), QString());
    s.replace(QLatin1Char('x'), QStringLiteral(" × "));  // ×
    return s.trimmed();
}

}  // namespace

HistoryModel::HistoryModel(QObject *parent) : QAbstractListModel(parent) {}

void HistoryModel::setHistory(const QList<db::Item> &items) {
    beginResetModel();
    mode_ = History;
    items_ = items;
    endResetModel();
}

void HistoryModel::setSnippets(const QList<db::Snippet> &snips) {
    beginResetModel();
    mode_ = Snippets;
    snips_ = snips;
    endResetModel();
}

int HistoryModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;
    return mode_ == History ? items_.size() : snips_.size();
}

db::Item HistoryModel::itemAt(int row) const {
    return (row >= 0 && row < items_.size()) ? items_.at(row) : db::Item{};
}

db::Snippet HistoryModel::snippetAt(int row) const {
    return (row >= 0 && row < snips_.size()) ? snips_.at(row) : db::Snippet{};
}

QPixmap HistoryModel::thumbFor(const db::Item &it) const {
    QString path;
    if (it.contentType == QLatin1String("image")) {
        path = it.content;
    } else if (it.contentType == QLatin1String("files")) {
        const QString first = it.content.split(QLatin1Char('\n')).value(0);
        if (thumbExts().contains(QFileInfo(first).suffix().toLower()))
            path = first;
    }
    if (path.isEmpty())
        return QPixmap();
    if (auto it2 = thumbs_.constFind(path); it2 != thumbs_.constEnd())
        return it2.value();

    QPixmap pm(path);
    if (!pm.isNull())
        pm = pm.scaled(QSize(300, 84), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (thumbs_.size() > 250)
        thumbs_.clear();
    thumbs_.insert(path, pm);
    return pm;
}

QVariant HistoryModel::data(const QModelIndex &index, int role) const {
    const int row = index.row();
    if (row < 0)
        return {};

    if (mode_ == Snippets) {
        if (row >= snips_.size())
            return {};
        const db::Snippet &s = snips_.at(row);
        switch (role) {
        case IdRole:         return QVariant::fromValue<qlonglong>(s.id);
        case TypeRole:       return QStringLiteral("snippet");
        case KeywordRole:    return s.triggerKeyword;
        case ExpansionRole:  return s.expansionText;
        case PreviewRole:    return QStringLiteral("%1  —  %2").arg(s.triggerKeyword, s.expansionText);
        case PinnedRole:     return false;
        default:             return {};
        }
    }

    if (row >= items_.size())
        return {};
    const db::Item &it = items_.at(row);
    switch (role) {
    case IdRole:       return QVariant::fromValue<qlonglong>(it.id);
    case TypeRole:     return it.contentType;
    case PreviewRole:  return it.preview.isEmpty() ? it.content : it.preview;
    case ContentRole:  return it.content;
    case PinnedRole:   return it.pinned;
    case CreatedRole:  return it.createdAt;
    case DimsRole:     return it.contentType == QLatin1String("image") ? dimsFromPreview(it.preview) : QString();
    case ThumbnailRole: {
        const QPixmap pm = thumbFor(it);
        return pm.isNull() ? QVariant() : QVariant(pm);
    }
    default:           return {};
    }
}
