#pragma once

#include <memdb/database.hpp>
#include <memdb/db_graph.hpp>
#include <memdb/references.hpp>

#include <QGraphicsObject>
#include <QGraphicsScene>
#include <QGraphicsView>

#include <map>

namespace memdb::viewer::detail {

/// @brief Ein Collection-Knoten der Struktur-Szene (QGraphicsFramework):
///        Titel + Dokumentanzahl + Schema-Felder (Typ, index/unique) – per
///        Doppelklick auf-/zuklappbar.
class CollectionNodeItem : public QGraphicsObject {
    Q_OBJECT
public:
    CollectionNodeItem(QString collection, QGraphicsItem* parent = nullptr);

    void setNode(const memdb::GraphNode& node);
    QString collectionName() const { return collection_; }
    bool isExpanded() const { return expanded_; }
    void setExpanded(bool on);
    void setHighlighted(bool on);
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    static constexpr qreal kWidth = 240.0;

    // oeffentlich (auch fuer Tests: direkter Event-Aufruf)
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

signals:
    void clicked(const QString& collection);
    void doubleClicked(const QString& collection);

private:
    QString collection_;
    memdb::GraphNode node_;
    bool expanded_ = true;
    bool highlighted_ = false;
};

/// @brief Struktur-Szene (QGraphicsFramework, FA-22 `DbGraph`): Collections als
///        Knoten, FA-18-Referenzen als gerichtete Kanten (Dangling = rot gestrichelt).
class GraphScene : public QGraphicsScene {
    Q_OBJECT
public:
    explicit GraphScene(QObject* parent = nullptr) : QGraphicsScene(parent) {}

    /// @brief Szene aus laufender Database neu aufbauen (DbGraph::fromDatabase).
    void rebuild(memdb::Database& db, memdb::ReferenceIndex* refs);
    void setGraph(const memdb::DbGraph& graph);
    const memdb::DbGraph& graph() const { return graph_; }

    int nodeCount() const { return static_cast<int>(nodes_.size()); }
    int edgeCount() const { return edgeCount_; }
    CollectionNodeItem* nodeItem(const QString& collection) const;
    void setExpanded(const QString& collection, bool on);
    void setHighlighted(const QString& collection); // hebt genau einen Knoten hervor

    bool exportPng(const QString& path);
    bool exportSvg(const QString& path);
    QString mermaid() const { return QString::fromStdString(graph_.toMermaid()); }
    QString graphSummary() const { return QString::fromStdString(graph_.summary()); }

signals:
    void collectionClicked(const QString& collection);

private:
    void clearAll();
    void layoutGrid();
    void drawEdges();

    memdb::DbGraph graph_;
    std::map<QString, CollectionNodeItem*> nodes_;
    int edgeCount_ = 0;
};

/// @brief View der Struktur-Szene: Pan (Ziehen) + Zoom (Mausrad), Selektion per Klick.
class GraphView : public QGraphicsView {
    Q_OBJECT
public:
    explicit GraphView(QWidget* parent = nullptr);
    void setGraphScene(GraphScene* scene);

protected:
    void wheelEvent(QWheelEvent* event) override;
};

} // namespace memdb::viewer::detail
