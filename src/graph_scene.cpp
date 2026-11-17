#include "graph_scene.hpp"

#include <QFile>
#include <QPainter>
#include <QPainterPath>
#include <QGraphicsPathItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsTextItem>
#include <QSvgGenerator>
#include <QWheelEvent>

#include <cmath>

namespace memdb::viewer::detail {

CollectionNodeItem::CollectionNodeItem(QString collection, QGraphicsItem* parent)
    : QGraphicsObject(parent), collection_(std::move(collection)) {
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIsMovable, false);
    setCursor(Qt::PointingHandCursor);
    setToolTip(collection_);
}

void CollectionNodeItem::setNode(const memdb::GraphNode& node) {
    node_ = node;
    prepareGeometryChange();
    update();
}

void CollectionNodeItem::setExpanded(bool on) {
    if (expanded_ == on)
        return;
    expanded_ = on;
    prepareGeometryChange();
    update();
}

void CollectionNodeItem::setHighlighted(bool on) {
    highlighted_ = on;
    update();
}

QRectF CollectionNodeItem::boundingRect() const {
    const int fieldRows = expanded_ ? static_cast<int>(node_.fields.size()) : 0;
    const qreal h = 46.0 + fieldRows * 16.0 + 8.0;
    return QRectF(0, 0, kWidth, h);
}

void CollectionNodeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*) {
    painter->setRenderHint(QPainter::Antialiasing, true);
    const QRectF r = boundingRect();
    QColor fill = highlighted_ ? QColor(70, 130, 200, 60) : QColor(245, 247, 250);
    QColor border = isSelected() ? QColor(30, 100, 200) : QColor(120, 130, 140);
    if (highlighted_)
        border = QColor(30, 100, 200);
    painter->setPen(QPen(border, highlighted_ || isSelected() ? 2.0 : 1.0));
    painter->setBrush(fill);
    painter->drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);

    painter->setPen(QPen(Qt::black));
    QFont f = painter->font();
    f.setBold(true);
    painter->setFont(f);
    painter->drawText(QRectF(8, 6, kWidth - 16, 20), Qt::AlignLeft | Qt::AlignVCenter,
                      QStringLiteral("%1  (%2)").arg(collection_).arg(node_.documents));
    f.setBold(false);
    f.setPointSize(f.pointSize() - 1);
    painter->setFont(f);
    painter->setPen(QColor(90, 90, 90));
    const QString meta = QStringLiteral("Schema: %1").arg(node_.hasSchema ? QStringLiteral("ja") : QStringLiteral("nein"));
    painter->drawText(QRectF(8, 26, kWidth - 16, 16), Qt::AlignLeft | Qt::AlignVCenter, meta);
    if (expanded_) {
        qreal y = 44.0;
        for (const auto& field : node_.fields) {
            const QString mark = QString::fromStdString(field.index).isEmpty()
                                     ? QString()
                                     : QStringLiteral("  [%1]").arg(QString::fromStdString(field.index));
            QString line = QStringLiteral("%1: %2%3")
                               .arg(QString::fromStdString(field.name),
                                    QString::fromStdString(field.type), mark);
            if (!field.nullable)
                line += QStringLiteral(" !");
            painter->drawText(QRectF(8, y, kWidth - 16, 16), Qt::AlignLeft | Qt::AlignVCenter, line);
            y += 16.0;
        }
    }
    Q_UNUSED(option);
}

void CollectionNodeItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() == Qt::LeftButton)
        emit clicked(collection_);
    QGraphicsObject::mousePressEvent(event);
}

void CollectionNodeItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        setExpanded(!expanded_);
        emit doubleClicked(collection_);
    }
    QGraphicsObject::mouseDoubleClickEvent(event);
}

// ---------------------------------------------------------------- GraphScene

void GraphScene::rebuild(memdb::Database& db, memdb::ReferenceIndex* refs) {
    setGraph(memdb::DbGraph::fromDatabase(db, refs));
}

void GraphScene::setGraph(const memdb::DbGraph& graph) {
    graph_ = graph;
    clearAll();
    for (const auto& n : graph_.nodes) {
        auto* item = new CollectionNodeItem(QString::fromStdString(n.collection));
        item->setNode(n);
        addItem(item);
        nodes_[item->collectionName()] = item;
        connect(item, &CollectionNodeItem::clicked, this, [this](const QString& c) { emit collectionClicked(c); });
    }
    layoutGrid();
    drawEdges();
    setSceneRect(itemsBoundingRect().adjusted(-40, -40, 40, 40));
}

void GraphScene::clearAll() {
    clear();
    nodes_.clear();
    edgeCount_ = 0;
}

void GraphScene::layoutGrid() {
    const int n = static_cast<int>(nodes_.size());
    if (n == 0)
        return;
    const int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(n))));
    int i = 0;
    for (auto& [name, item] : nodes_) {
        const int col = i % cols;
        const int row = i / cols;
        item->setPos(col * 300.0, row * 260.0);
        ++i;
    }
}

void GraphScene::drawEdges() {
    edgeCount_ = 0;
    for (const auto& e : graph_.edges) {
        const QString from = QString::fromStdString(e.from).section(QLatin1Char('#'), 0, 0);
        const QString to = QString::fromStdString(e.to).section(QLatin1Char('#'), 0, 0);
        auto* a = nodeItem(from);
        auto* b = nodeItem(to);
        if (!a || !b)
            continue;
        const QPointF p1 = a->pos() + QPointF(CollectionNodeItem::kWidth / 2, 30);
        const QPointF p2 = b->pos() + QPointF(CollectionNodeItem::kWidth / 2, 30);
        QPainterPath path(p1);
        const qreal midY = (p1.y() + p2.y()) / 2 + 40;
        path.cubicTo(QPointF(p1.x(), midY), QPointF(p2.x(), midY), p2);
        auto* edge = new QGraphicsPathItem(path);
        QPen pen(e.dangling ? QColor(200, 40, 40) : QColor(80, 90, 100), 1.5);
        if (e.dangling)
            pen.setStyle(Qt::DashLine);
        edge->setPen(pen);
        edge->setZValue(-1);
        addItem(edge);
        // Pfeilspitze
        const QPointF dir = p2 - path.pointAtPercent(0.98);
        const qreal len = std::sqrt(dir.x() * dir.x() + dir.y() * dir.y()) ?: 1.0;
        const QPointF u = dir / len;
        const QPointF tip = p2 - u * 10.0;
        const QPointF left = tip - QPointF(-u.y(), u.x()) * 5.0;
        const QPointF right = tip + QPointF(-u.y(), u.x()) * 5.0;
        QPainterPath head;
        head.moveTo(p2);
        head.lineTo(left);
        head.lineTo(right);
        head.closeSubpath();
        auto* arrow = new QGraphicsPathItem(head);
        arrow->setPen(Qt::NoPen);
        arrow->setBrush(pen.color());
        arrow->setZValue(-1);
        addItem(arrow);
        // Label (Relationsname)
        if (!e.relation.empty()) {
            auto* label = new QGraphicsTextItem(QString::fromStdString(e.relation));
            label->setDefaultTextColor(pen.color());
            label->setPos((p1.x() + p2.x()) / 2 - 20, midY - 26);
            label->setZValue(-1);
            addItem(label);
        }
        ++edgeCount_;
    }
}

CollectionNodeItem* GraphScene::nodeItem(const QString& collection) const {
    auto it = nodes_.find(collection);
    return it != nodes_.end() ? it->second : nullptr;
}

void GraphScene::setExpanded(const QString& collection, bool on) {
    if (auto* item = nodeItem(collection))
        item->setExpanded(on);
}

void GraphScene::setHighlighted(const QString& collection) {
    for (auto& [name, item] : nodes_)
        item->setHighlighted(name == collection);
}

bool GraphScene::exportPng(const QString& path) {
    QImage img(itemsBoundingRect().size().toSize(), QImage::Format_ARGB32);
    img.fill(Qt::white);
    QPainter p(&img);
    render(&p, QRectF(), itemsBoundingRect());
    return img.save(path);
}

bool GraphScene::exportSvg(const QString& path) {
    QSvgGenerator gen;
    gen.setFileName(path);
    gen.setSize(itemsBoundingRect().size().toSize());
    gen.setViewBox(itemsBoundingRect());
    gen.setTitle(QStringLiteral("InMemDBViewer – Struktur"));
    QPainter p(&gen);
    render(&p, QRectF(), itemsBoundingRect());
    return true;
}

// ----------------------------------------------------------------- GraphView

GraphView::GraphView(QWidget* parent) : QGraphicsView(parent) {
    setRenderHint(QPainter::Antialiasing, true);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setBackgroundBrush(QColor(252, 252, 253));
}

void GraphView::setGraphScene(GraphScene* scene) { setScene(scene); }

void GraphView::wheelEvent(QWheelEvent* event) {
    const qreal factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    scale(factor, factor);
}

} // namespace memdb::viewer::detail
