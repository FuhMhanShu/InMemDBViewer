#include "memdbviewer/collection_item.hpp"

#include <QPainter>

#include <algorithm>

namespace memdb::viewer {

CollectionItem::CollectionItem(QString title, QString subtitle, std::vector<FieldRow> fields,
                               QGraphicsItem* parent)
    : QGraphicsItem(parent),
      title_(std::move(title)),
      subtitle_(std::move(subtitle)),
      fields_(std::move(fields)) {
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    rebuildVisible();
}

bool CollectionItem::hasChildren(const QString& path) const {
    const QString prefix = path + QStringLiteral(".");
    for (const FieldRow& f : fields_)
        if (f.path.startsWith(prefix))
            return true;
    return false;
}

void CollectionItem::rebuildVisible() {
    prepareGeometryChange();
    visible_.clear();
    QString skipPrefix;
    for (int i = 0; i < static_cast<int>(fields_.size()); ++i) {
        if (!skipPrefix.isEmpty() && fields_[i].path.startsWith(skipPrefix))
            continue; // Kind einer kollabierten Zeile
        visible_.push_back(i);
        skipPrefix.clear();
        if (collapsed_.contains(fields_[i].path) && hasChildren(fields_[i].path))
            skipPrefix = fields_[i].path + QStringLiteral(".");
    }
    const qreal h = kHeaderHeight + kPadding + visible_.size() * kRowHeight + kPadding;
    rect_ = QRectF(0, 0, kWidth, h);
}

int CollectionItem::rowAt(const QPointF& itemPos) const {
    if (itemPos.x() < 0 || itemPos.x() > kWidth)
        return -1;
    const qreal y = itemPos.y() - kHeaderHeight - kPadding / 2;
    if (y < 0)
        return -1;
    const int row = static_cast<int>(y / kRowHeight);
    return row >= 0 && row < static_cast<int>(visible_.size()) ? row : -1;
}

void CollectionItem::toggleCollapseAt(const QPointF& itemPos) {
    const int row = rowAt(itemPos);
    if (row < 0)
        return;
    const FieldRow& f = fields_[static_cast<std::size_t>(visible_[static_cast<std::size_t>(row)])];
    if (!hasChildren(f.path))
        return;
    if (collapsed_.contains(f.path))
        collapsed_.remove(f.path);
    else
        collapsed_.insert(f.path);
    rebuildVisible();
    update();
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

    // Feld-Zeilen (sichtbare, ggf. eingerueckte Unter-Schema-Zeilen, FA-06)
    painter->setFont(small);
    for (int row = 0; row < static_cast<int>(visible_.size()); ++row) {
        const FieldRow& f = fields_[static_cast<std::size_t>(visible_[static_cast<std::size_t>(row)])];
        const qreal y = kHeaderHeight + kPadding / 2 + static_cast<qreal>(row) * kRowHeight;
        const qreal indent = static_cast<qreal>(f.depth) * kIndent;
        const QRectF rowRect(kPadding + indent, y,
                             std::max(10.0, kWidth - 2 * kPadding - indent), kRowHeight);
        if (hasChildren(f.path)) {
            painter->setPen(QColor(60, 60, 60));
            painter->drawText(QRectF(2, y, kPadding - 2, kRowHeight), Qt::AlignLeft | Qt::AlignVCenter,
                              collapsed_.contains(f.path) ? QStringLiteral("+") : QStringLiteral("-"));
        }
        painter->setPen(QColor(30, 30, 30));
        painter->drawText(rowRect, Qt::AlignLeft | Qt::AlignVCenter, f.name);
        QString right = f.type;
        if (!f.flags.isEmpty())
            right += right.isEmpty() ? f.flags : QStringLiteral("  [") + f.flags + QStringLiteral("]");
        painter->setPen(f.flags.contains(QStringLiteral("unique")) ? QColor(180, 40, 40)
                                                                    : QColor(90, 90, 90));
        painter->drawText(rowRect, Qt::AlignRight | Qt::AlignVCenter, right);
    }
}

} // namespace memdb::viewer
