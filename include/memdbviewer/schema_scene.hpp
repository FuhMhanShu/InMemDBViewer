#pragma once

#include <memdb/database.hpp>
#include <memdb/db_graph.hpp>
#include <memdb/references.hpp>

#include "memdbviewer/collection_item.hpp"

#include <QGraphicsScene>
#include <QString>

#include <vector>

class QPaintDevice;

namespace memdb::viewer {

class CollectionItem;

/// @brief View (MVC): QGraphicsScene mit einem CollectionItem pro Collection (FA-04,
///        rekursive Schema-Zeilen FA-06) und gerichteten Kanten fuer FA-18-Referenzen
///        (FA-05, inkl. Dangling-Markierung). Datenquelle: DbGraph (FA-22) + Collection-Schema.
class SchemaScene : public QGraphicsScene {
    Q_OBJECT
public:
    explicit SchemaScene(QObject* parent = nullptr);

    /// @brief Szene aus einer Database aufbauen (nicht-owning).
    /// @param refs optionaler ReferenceIndex fuer die Kanten (FA-18).
    void setDatabase(memdb::Database& db, const memdb::ReferenceIndex* refs = nullptr);
    /// @brief Szene aus der gemerkten Database neu aufbauen (z. B. nach Observer-Event, FA-08).
    void rebuild();

    int collectionItemCount() const { return static_cast<int>(items_.size()); }
    int edgeItemCount() const { return static_cast<int>(edges_.size()); }
    const std::vector<CollectionItem*>& collectionItems() const { return items_; }
    CollectionItem* itemForCollection(const QString& name) const;

    /// @brief Zuletzt aufgebauter DbGraph (FA-22) – z. B. fuer Mermaid-Export (FA-15).
    const memdb::DbGraph& graph() const { return graph_; }
    QString mermaid() const { return QString::fromStdString(graph_.toMermaid()); }
    QString json() const { return QString::fromStdString(graph_.toJson()); }
    QString markdown() const { return QString::fromStdString(graph_.toMarkdown()); }
    /// @brief Szene als PNG rendern (FA-15).
    bool exportPng(const QString& path);
    /// @brief Szene als SVG rendern (FA-15, QtSvg).
    bool exportSvg(const QString& path);

private:
    bool renderTo(QPaintDevice& device, const QRectF& sceneRect);
    std::vector<CollectionItem::FieldRow> buildFieldRows(memdb::Collection& coll) const;

    memdb::Database* db_ = nullptr;
    const memdb::ReferenceIndex* refs_ = nullptr;
    memdb::DbGraph graph_;
    std::vector<CollectionItem*> items_;
    std::vector<QGraphicsItem*> edges_;
};

} // namespace memdb::viewer
