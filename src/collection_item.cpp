#include "memdbviewer/collection_item.hpp"

#include <QPainter>

namespace memdb::viewer {

CollectionItem::CollectionItem(QString title, QString subtitle, std::vector<FieldRow> fields,
                               QGraphicsItem* parent)
    : QGraphicsItem(parent),
      title_(std::move(title)),
      subtitle_(std::move(subtitle)),
      fields_(std::move(fields)) {
    const qreal h = kHeaderHeight + kPadding + fields_.size() * kRowHeight + kPadding;
    rect_ = QRectF(0, 0, kWidth, h);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
}

void CollectionItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                           QWidget* widget) {
    Q_UNUSED(option);
    Q_UNUSED(widget);
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QRectF body = rect_.adjusted(0.5, 0.5, -0.5, -0.5);
    painter->setPen(QPen(QColor(80, 80, 80), 1));
    painter->setBrush(isSelected() ? QColor(227, 240, 253) : QColor(250, 250, 250));
    painter->drawRoundedRect(body, 6, 6);

    // Header
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(70, 130, 180));
    painter->drawRect(QRectF(1, 1, kWidth - 2, kHeaderHeight - 1));

    QFont bold = painter->font();
    bold.setBold(true);
    painter->setFont(bold);
    painter->setPen(Qt::white);
    painter->drawText(QRectF(kPadding, 2, kWidth - 2 * kPadding, kHeaderHeight / 2),
                      Qt::AlignLeft | Qt::AlignVCenter, title_);

    QFont small = painter->font();
    small.setBold(false);
    if (small.pointSize() > 8)
        small.setPointSize(small.pointSize() - 1);
    painter->setFont(small);
    painter->setPen(QColor(230, 230, 230));
    painter->drawText(QRectF(kPadding, kHeaderHeight / 2, kWidth - 2 * kPadding, kHeaderHeight / 2),
                      Qt::AlignLeft | Qt::AlignVCenter, subtitle_);

    painter->setPen(QPen(QColor(80, 80, 80), 1));
    painter->drawLine(QPointF(0, kHeaderHeight), QPointF(kWidth, kHeaderHeight));

    // Feld-Zeilen
    painter->setFont(small);
    for (std::size_t i = 0; i < fields_.size(); ++i) {
        const qreal y = kHeaderHeight + kPadding / 2 + static_cast<qreal>(i) * kRowHeight;
        const QRectF rowRect(kPadding, y, kWidth - 2 * kPadding, kRowHeight);
        painter->setPen(QColor(30, 30, 30));
        painter->drawText(rowRect, Qt::AlignLeft | Qt::AlignVCenter, fields_[i].name);
        QString right = fields_[i].type;
        if (!fields_[i].flags.isEmpty())
            right += right.isEmpty() ? fields_[i].flags
                                     : QStringLiteral("  [") + fields_[i].flags + QStringLiteral("]");
        painter->setPen(fields_[i].flags.contains(QStringLiteral("unique")) ? QColor(180, 40, 40)
                                                                            : QColor(90, 90, 90));
        painter->drawText(rowRect, Qt::AlignRight | Qt::AlignVCenter, right);
    }
}

} // namespace memdb::viewer
