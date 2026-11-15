#pragma once

#include <memdb/database.hpp>
#include <memdb/references.hpp>

#include <QGraphicsScene>

#include <vector>

namespace memdb::viewer {

class CollectionItem;

/// @brief View (MVC): QGraphicsScene mit einem CollectionItem pro Collection (FA-04)
///        und gerichteten Kanten fuer FA-18-Referenzen (FA-05, inkl. Dangling-Markierung).
///        Datenquelle ist DbGraph (FA-22); Raster-Layout, Kanten als Linien mit Label.
class SchemaScene : public QGraphicsScene {
    Q_OBJECT
public:
    explicit SchemaScene(QObject* parent = nullptr);

    /// @brief Szene aus einer Database aufbauen (nicht-owning).
    /// @param refs opcionaler ReferenceIndex fuer die Kanten (FA-18).
    void setDatabase(memdb::Database& db, const memdb::ReferenceIndex* refs = nullptr);
    /// @brief Szene aus der gemerkten Database neu aufbauen (z. B. nach Observer-Event, FA-08).
    void rebuild();

    int collectionItemCount() const { return static_cast<int>(items_.size()); }
    int edgeItemCount() const { return static_cast<int>(edges_.size()); }
    const std::vector<CollectionItem*>& collectionItems() const { return items_; }

private:
    memdb::Database* db_ = nullptr;
    const memdb::ReferenceIndex* refs_ = nullptr;
    std::vector<CollectionItem*> items_;
    std::vector<QGraphicsItem*> edges_;
};

} // namespace memdb::viewer
