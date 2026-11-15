#include "memdbviewer/schema_scene.hpp"

#include "memdbviewer/collection_item.hpp"

#include <memdb/db_graph.hpp>

#include <QGraphicsLineItem>
#include <QGraphicsSimpleTextItem>
#include <QPen>

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace memdb::viewer {

SchemaScene::SchemaScene(QObject* parent) : QGraphicsScene(parent) {}

void SchemaScene::setDatabase(memdb::Database& db, const memdb::ReferenceIndex* refs) {
    db_ = &db;
    refs_ = refs;
    rebuild();
}

void SchemaScene::rebuild() {
    clear();
    items_.clear();
    edges_.clear();
    if (!db_)
        return;

    const memdb::DbGraph graph = memdb::DbGraph::fromDatabase(*db_, refs_);

    // Raster-Layout ueber die Collection-Knoten
    const int count = static_cast<int>(graph.nodes.size());
    const int cols = std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(count)))));
    std::unordered_map<std::string, CollectionItem*> byCollection;

    int col = 0;
    qreal x = 0, y = 0, rowHeight = 0;
    for (const memdb::GraphNode& node : graph.nodes) {
        std::vector<CollectionItem::FieldRow> fields;
        fields.reserve(node.fields.size());
        for (const memdb::GraphField& f : node.fields) {
            QString flags;
            if (f.index == "unique")
                flags = QStringLiteral("unique");
            else if (f.index == "index")
                flags = QStringLiteral("index");
            if (!f.nullable)
                flags += flags.isEmpty() ? QStringLiteral("notNull") : QStringLiteral(", notNull");
            fields.push_back({QString::fromStdString(f.name), QString::fromStdString(f.type), flags});
        }
        const QString subtitle = QStringLiteral("%1 Dokument(e) | Schema: %2")
                                     .arg(node.documents)
                                     .arg(node.hasSchema ? QStringLiteral("ja") : QStringLiteral("nein"));
        auto* item = new CollectionItem(QString::fromStdString(node.collection), subtitle,
                                        std::move(fields));
        item->setPos(x, y);
        addItem(item);
        byCollection[node.collection] = item;
        items_.push_back(item);
        rowHeight = std::max(rowHeight, item->boundingRect().height());
        ++col;
        if (col == cols) {
            col = 0;
            x = 0;
            y += rowHeight + 40;
            rowHeight = 0;
        } else {
            x += CollectionItem::kWidth + 40;
        }
    }

    // Referenz-Kanten (FA-05): Endpunkte "collection#id" -> Collection-Teil vor '#'
    const auto collectionOf = [](const std::string& endpoint) {
        const auto pos = endpoint.find('#');
        return pos == std::string::npos ? endpoint : endpoint.substr(0, pos);
    };
    for (const memdb::GraphEdge& edge : graph.edges) {
        const auto fromIt = byCollection.find(collectionOf(edge.from));
        const auto toIt = byCollection.find(collectionOf(edge.to));
        if (fromIt == byCollection.end() || toIt == byCollection.end())
            continue;
        const QPointF p1 = fromIt->second->sceneBoundingRect().center();
        const QPointF p2 = toIt->second->sceneBoundingRect().center();
        QPen pen(edge.dangling ? QColor(200, 40, 40) : QColor(100, 100, 100), 1.5);
        if (edge.dangling)
            pen.setStyle(Qt::DashLine);
        auto* line = new QGraphicsLineItem(QLineF(p1, p2));
        line->setPen(pen);
        line->setZValue(-1);
        addItem(line);
        edges_.push_back(line);
        if (!edge.relation.empty()) {
            auto* label = new QGraphicsSimpleTextItem(QString::fromStdString(edge.relation));
            label->setPos((p1 + p2) / 2 + QPointF(4, -10));
            label->setBrush(edge.dangling ? QColor(200, 40, 40) : QColor(80, 80, 80));
            addItem(label);
        }
    }

    setSceneRect(itemsBoundingRect().adjusted(-30, -30, 30, 30));
}

} // namespace memdb::viewer
