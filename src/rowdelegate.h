#pragma once

#include <QFont>
#include <QStyledItemDelegate>

// Draws one history/snippet row: text (elided), image (thumbnail + dims), or
// files (thumbnail of first + "N files"); a tan accent bar + star on pinned rows;
// a relative timestamp; and per-row pin/delete glyphs. Clicks on the glyphs are
// caught in editorEvent and reported via signals; clicks elsewhere fall through
// so the view emits clicked() (which the popup treats as "paste this").
class RowDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit RowDelegate(QObject *parent = nullptr);

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    bool editorEvent(QEvent *event, QAbstractItemModel *model,
                     const QStyleOptionViewItem &option,
                     const QModelIndex &index) override;

signals:
    void pinClicked(const QModelIndex &index);
    void deleteClicked(const QModelIndex &index);

private:
    int rowHeight(const QModelIndex &index) const;
    QRect deleteRect(const QRect &row) const;
    QRect pinRect(const QRect &row) const;

    QFont bodyFont_;
    QFont tinyFont_;
    QFont glyphFont_;
};
