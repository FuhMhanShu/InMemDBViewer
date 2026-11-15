#include "memdbviewer/schema_scene.hpp"

#include "memdbviewer/collection_item.hpp"

#include <memdb/collection.hpp>
#include <memdb/schema.hpp>

#include <QGraphicsLineItem>
#include <QGraphicsSimpleTextItem>
#include <QImage>
#include <QPainter>
#include <QPen>

#include <algorithm>
#include <cmath>
#include <set>
#include <unordered_map>

namespace memdb::viewer {

namespace {

/// @brief Schema rekursiv in FieldRows umwandeln (FA-06: Unter-Schemas eingerueckt).
void collectRows(const memdb::Schema& schema, const std::vector<memdb::Index>& indexes,
                 const std::string& prefix, int depth,
                 std::vector<CollectionItem::FieldRow>& rows, std::set<std::string>& schemaPaths) {
    for (const auto& [path, field] : schema.fields()) {
        const std::string full = prefix.empty() ? path : prefix + "." + path;
        schemaPaths.insert(full);
        QString flags;
        for (const memdb::Index& idx : indexes)
            if (idx.field() == full)
                flags = idx.isUnique() ? QStringLiteral("unique") : QStringLiteral("index");
        if (!field.nullable())
            flags += flags.isEmpty() ? QStringLiteral("notNull") : QStringLiteral(", notNull");
        QString type = QString::fromStdString(memdb::typeName(field.type()));
        if (field.type() == memdb::Value::Type::Array && field.elementType())
            type += QStringLiteral("<") + QString::fromStdString(memdb::typeName(*field.elementType())) +
                    QStringLiteral(">");
        const auto dot = path.find('.');
        const std::string leaf = dot == std::string::npos ? path : path.substr(dot + 1);
        rows.push_back({QString::fromStdString(leaf), QString::fromStdString(full), type, flags, depth});
        if (const memdb::Schema* sub = field.sub())
            collectRows(*sub, indexes, full, depth + 1, rows, schemaPaths);
    }
}

} // namespace

SchemaScene::SchemaScene(QObject* parent) : QGraphicsScene(parent) {}

void SchemaScene::setDatabase(memdb::Database& db, const memdb::ReferenceIndex* refs) {
    db_ = &db;
    refs_ = refs;
    rebuild();
}

std::vector<CollectionItem::FieldRow> SchemaScene::buildFieldRows(memdb::Collection& coll) const {
    std::vector<CollectionItem::FieldRow> rows;
    const std::vector<memdb::Index> indexes = coll.indexes();
    const std::optional<memdb::Schema> schema = coll.schema();
    if (schema) {
        std::set<std::string> schemaPaths;
        collectRows(*schema, indexes, "", 0, rows, schemaPaths);
        // index-only Felder (Index auf Pfad ohne Schema-Eintrag)
        for (const memdb::Index& idx : indexes)
            if (!schemaPaths.count(idx.field()))
                rows.push_back({QString::fromStdString(idx.field()), QString::fromStdString(idx.field()),
                                QStringLiteral("?"),
                                idx.isUnique() ? QStringLiteral("unique") : QStringLiteral("index"), 0});
    } else {
        // schema-los: nur index-only Felder bekannt (aus dem DbGraph-Knoten)
        if (db_) {
            for (const memdb::GraphNode& node : graph_.nodes) {
                if (node.collection != coll.name())
                    continue;
                for (const memdb::GraphField& f : node.fields)
                    rows.push_back({QString::fromStdString(f.name), QString::fromStdString(f.name),
                                    QString::fromStdString(f.type),
                                    f.index == "unique" ? QStringLiteral("unique")
                                    : f.index == "index"  ? QStringLiteral("index")
                                                          : QString(),
                                    0});
            }
        }
    }
    return rows;
}

void SchemaScene::rebuild() {
    clear();
    items_.clear();
    edges_.clear();
    if (!db_)
        return;

    graph_ = memdb::DbGraph::fromDatabase(*db_, refs_);

    // Raster-Layout ueber die Collection-Knoten
    const int count = static_cast<int>(graph_.nodes.size());
    const int cols = std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(count)))));
    std::unordered_map<std::string, CollectionItem*> byCollection;

    int col = 0;
    qreal x = 0, y = 0, rowHeight = 0;
    for (const memdb::GraphNode& node : graph_.nodes) {
        memdb::Collection coll = db_->collection(node.collection);
        const QString subtitle = QStringLiteral("%1 Dokument(e) | Schema: %2")
                                     .arg(node.documents)
                                     .arg(node.hasSchema ? QStringLiteral("ja") : QStringLiteral("nein"));
        auto* item = new CollectionItem(QString::fromStdString(node.collection), subtitle,
                                        buildFieldRows(coll));
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
    for (const memdb::GraphEdge& edge : graph_.edges) {
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

CollectionItem* SchemaScene::itemForCollection(const QString& name) const {
    for (CollectionItem* item : items_)
        if (item->title() == name)
            return item;
    return nullptr;
}

bool SchemaScene::exportPng(const QString& path) const {
    const QRectF r = sceneRect();
    if (r.isNull() || r.width() <= 0 || r.height() <= 0)
        return false;
    QImage img(r.size().toSize(), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    QPainter painter(&img);
    painter.setRenderHint(QPainter::Antialiasing, true);
    render(&painter, QRectF(QPointF(0, 0), r.size()), r);
    return img.save(path);
}

} // namespace memdb::viewer
