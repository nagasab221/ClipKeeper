#include "rowdelegate.h"

#include <QDateTime>
#include <QFileInfo>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QSet>

#include "historymodel.h"
#include "theme.h"

namespace {

constexpr int kTextRowH = 40;
constexpr int kImageRowH = 116;
constexpr int kGlyph = 24;

bool isImageExt(const QString &path) {
    static const QSet<QString> exts = {
        QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
        QStringLiteral("gif"), QStringLiteral("bmp"),  QStringLiteral("webp"),
        QStringLiteral("ico"),
    };
    return exts.contains(QFileInfo(path).suffix().toLower());
}

// "now" / "5m" / "3h" / "2d" / "14 Jul"
QString relativeTime(const QString &iso) {
    if (iso.isEmpty())
        return QString();
    QDateTime when = QDateTime::fromString(iso, Qt::ISODateWithMs);
    if (!when.isValid())
        when = QDateTime::fromString(iso, Qt::ISODate);
    if (!when.isValid())
        return QString();
    when.setTimeSpec(Qt::UTC);
    const qint64 secs = when.secsTo(QDateTime::currentDateTimeUtc());
    if (secs < 45)
        return QStringLiteral("now");
    if (secs < 3600)
        return QStringLiteral("%1m").arg(secs / 60);
    if (secs < 86400)
        return QStringLiteral("%1h").arg(secs / 3600);
    if (secs < 7 * 86400)
        return QStringLiteral("%1d").arg(secs / 86400);
    return when.toLocalTime().toString(QStringLiteral("d MMM"));
}

}  // namespace

RowDelegate::RowDelegate(QObject *parent) : QStyledItemDelegate(parent) {
    bodyFont_ = QFont(QString::fromLatin1(theme::SANS));
    bodyFont_.setPixelSize(13);
    tinyFont_ = QFont(QString::fromLatin1(theme::SANS));
    tinyFont_.setPixelSize(10);
    glyphFont_ = QFont(QString::fromLatin1(theme::SANS));
    glyphFont_.setPixelSize(14);
}

int RowDelegate::rowHeight(const QModelIndex &index) const {
    const QString type = index.data(HistoryModel::TypeRole).toString();
    if (type == QLatin1String("image"))
        return kImageRowH;
    if (type == QLatin1String("files")) {
        const QString first =
            index.data(HistoryModel::ContentRole).toString().split(QLatin1Char('\n')).value(0);
        return isImageExt(first) ? kImageRowH : kTextRowH;
    }
    return kTextRowH;
}

QSize RowDelegate::sizeHint(const QStyleOptionViewItem &option,
                            const QModelIndex &index) const {
    const int w = option.rect.width() > 0 ? option.rect.width() : 340;
    return QSize(w, rowHeight(index));
}

QRect RowDelegate::deleteRect(const QRect &row) const {
    const int cy = row.center().y();
    return QRect(row.right() - 4 - kGlyph, cy - kGlyph / 2, kGlyph, kGlyph);
}

QRect RowDelegate::pinRect(const QRect &row) const {
    const QRect del = deleteRect(row);
    return QRect(del.left() - 2 - kGlyph, del.top(), kGlyph, kGlyph);
}

void RowDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                        const QModelIndex &index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    const QRect r = option.rect;
    const bool selected = option.state & QStyle::State_Selected;
    const bool hover = option.state & QStyle::State_MouseOver;

    if (selected)
        painter->fillRect(r, theme::SELECT);
    else if (hover)
        painter->fillRect(r, theme::HOVER);

    const QString type = index.data(HistoryModel::TypeRole).toString();
    const bool pinned = index.data(HistoryModel::PinnedRole).toBool();
    const bool isSnippet = type == QLatin1String("snippet");

    if (pinned) {
        painter->fillRect(QRect(r.left() + 5, r.top() + 6, 3, r.height() - 12), theme::ACCENT);
    }

    const QRect del = deleteRect(r);
    const QRect pin = pinRect(r);
    const QRect timeR(pin.left() - 44, r.top(), 42, r.height());
    const int bodyLeft = r.left() + 16;
    const QRect body(bodyLeft, r.top(), timeR.left() - bodyLeft - 6, r.height());

    const QColor textColor = selected ? theme::HEADING : theme::FG;
    const QPixmap thumb = index.data(HistoryModel::ThumbnailRole).value<QPixmap>();

    if (!thumb.isNull()) {
        const int tx = body.left();
        const int ty = body.top() + 8;
        painter->drawPixmap(tx, ty, thumb);
        const QString caption = (type == QLatin1String("image"))
                                    ? index.data(HistoryModel::DimsRole).toString()
                                    : index.data(HistoryModel::PreviewRole).toString();
        painter->setFont(tinyFont_);
        painter->setPen(selected ? theme::HEADING : theme::MUTED);
        const QRect capR(body.left(), ty + thumb.height() + 3, body.width(), 16);
        painter->drawText(capR, Qt::AlignLeft | Qt::AlignVCenter,
                          painter->fontMetrics().elidedText(caption, Qt::ElideRight, body.width()));
    } else {
        painter->setFont(bodyFont_);
        painter->setPen(textColor);
        const QString txt = index.data(HistoryModel::PreviewRole).toString();
        painter->drawText(body, Qt::AlignLeft | Qt::AlignVCenter,
                          painter->fontMetrics().elidedText(txt, Qt::ElideRight, body.width()));
    }

    const QString created = index.data(HistoryModel::CreatedRole).toString();
    if (!created.isEmpty()) {
        painter->setFont(tinyFont_);
        painter->setPen(theme::MUTED);
        painter->drawText(timeR, Qt::AlignRight | Qt::AlignVCenter, relativeTime(created));
    }

    painter->setFont(glyphFont_);
    if (!isSnippet && (selected || hover || pinned)) {
        painter->setPen(pinned ? theme::ACCENT : theme::HINT);
        painter->drawText(pin, Qt::AlignCenter,
                          pinned ? QString(QChar(0x2605)) : QString(QChar(0x2606)));  // ★ / ☆
    }
    if (selected || hover) {
        painter->setPen(theme::MUTED);
        painter->drawText(del, Qt::AlignCenter, QString(QChar(0x2715)));  // ✕
    }

    painter->restore();
}

bool RowDelegate::editorEvent(QEvent *event, QAbstractItemModel *,
                              const QStyleOptionViewItem &option,
                              const QModelIndex &index) {
    if (event->type() == QEvent::MouseButtonRelease) {
        auto *me = static_cast<QMouseEvent *>(event);
        const QString type = index.data(HistoryModel::TypeRole).toString();
        if (type != QLatin1String("snippet") && pinRect(option.rect).contains(me->pos())) {
            emit pinClicked(index);
            return true;
        }
        if (deleteRect(option.rect).contains(me->pos())) {
            emit deleteClicked(index);
            return true;
        }
    }
    return false;  // fall through: the view selects + emits clicked() (= paste)
}
