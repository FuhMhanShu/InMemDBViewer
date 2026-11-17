#pragma once

#include <QWidget>
#include <QString>

namespace memdb::viewer {
class ViewerCore;
namespace detail {
class DataViewController;
class DatabaseModel;
class DocumentTableModel;
class DocumentTreeModel;
class GraphScene;
}
} // namespace memdb::viewer

namespace memdb::viewer {

/// @brief Gekapseltes MVC-Widget „Datenbetrachtung" (DB-01…DB-10): Datenbank-Übersicht
///        (Collections mit Dokumentanzahl/Schema/Indizes), Strukturansicht mit dem
///        QGraphicsFramework (`DbGraph` FA-22: Collections als Knoten, FA-18-Referenzen
///        als Kanten), tabellarische Dokumentansicht (virtualisiert, DSL-Query-Filter
///        mit Index-Diagnose, Aggregationen FA-13) und Dokument-Detailbaum.
///        Live-Aktualisierung ueber `ViewerCore` (Observer FA-15 + Auto-Sync).
///
/// Verwendung: `auto* view = new DataViewWidget; view->setCore(core);`
class DataViewWidget : public QWidget {
    Q_OBJECT
public:
    explicit DataViewWidget(QWidget* parent = nullptr);
    ~DataViewWidget() override;

    void setCore(ViewerCore* core);
    ViewerCore* core() const;

    QString currentCollection() const;

    // Introspection fuer Tests/Diagnose
    int graphNodeCount() const;
    int graphEdgeCount() const;
    int tableRowCount() const;
    int tableColumnCount() const;
    QString queryDiagnostics() const;
    /// @brief Struktur-Szene als PNG exportieren (DB-09; auch fuer Headless-Smoke).
    bool exportStructurePng(const QString& path);

public slots:
    void applyQuery(const QString& dsl);
    void resetQuery();
    void refresh();

signals:
    void statusMessage(const QString& message);
    void collectionSelected(const QString& name);
    void documentSelected(quint64 id);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace memdb::viewer
