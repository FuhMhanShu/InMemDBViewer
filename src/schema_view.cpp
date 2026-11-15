#include "memdbviewer/schema_view.hpp"

#include "memdbviewer/schema_scene.hpp"

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

} // namespace memdb::viewer
