#include "memdbviewer/schema_view.hpp"

#include "memdbviewer/collection_item.hpp"
#include "memdbviewer/schema_scene.hpp"

#include <QMouseEvent>
#include <QPainter>

namespace memdb::viewer {

SchemaView::SchemaView(QWidget* parent) : QGraphicsView(parent) {
    setRenderHint(QPainter::Antialiasing, true);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
}

void SchemaView::setSchemaScene(SchemaScene* scene) {
    setScene(scene);
    fitContent();
}

void SchemaView::fitContent() {
    if (scene() && !scene()->itemsBoundingRect().isNull())
        fitInView(scene()->itemsBoundingRect().adjusted(-30, -30, 30, 30), Qt::KeepAspectRatio);
}

void SchemaView::mousePressEvent(QMouseEvent* event) {
    QGraphicsView::mousePressEvent(event);
    if (const auto* item = itemAt(event->pos()))
        if (const auto* ci = dynamic_cast<const CollectionItem*>(item))
            emit collectionClicked(ci->title());
}

void SchemaView::mouseDoubleClickEvent(QMouseEvent* event) {
    if (auto* item = itemAt(event->pos()))
        if (auto* ci = dynamic_cast<CollectionItem*>(item)) {
            ci->toggleCollapseAt(mapToScene(event->pos()) - ci->scenePos());
            return;
        }
    QGraphicsView::mouseDoubleClickEvent(event);
}

} // namespace memdb::viewer
