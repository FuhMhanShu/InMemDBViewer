#pragma once

#include <QObject>
#include <QString>

namespace memdb {
class Database;
}

namespace memdb::viewer {
class ViewerCore;
namespace detail {
class DatabaseModel;
class DocumentTableModel;
class DocumentTreeModel;
class GraphScene;
}
} // namespace memdb::viewer

namespace memdb::viewer::detail {

/// @brief Controller (MVC) des DataViewWidgets: verdrahtet ViewerCore (Model)
///        mit den internen Models (DatabaseModel, DocumentTableModel,
///        DocumentTreeModel, GraphScene) – kennt keine konkreten Widgets.
class DataViewController : public QObject {
    Q_OBJECT
public:
    DataViewController(ViewerCore* core, DatabaseModel* dbModel, DocumentTableModel* tableModel,
                       DocumentTreeModel* treeModel, GraphScene* scene, QObject* parent = nullptr);

    void setCore(ViewerCore* core);

    /// @brief Collection auswaehlen (Uebersicht/Tabelle/Struktur koppeln).
    void selectCollection(const QString& name);
    QString currentCollection() const { return current_; }

    /// @brief DSL-Filter auf die Tabelle anwenden; false + Fehlertext bei QueryError.
    bool applyQuery(const QString& dsl, QString* error);
    void clearQuery();

    /// @brief Dokument-Zeile ausgewaehlt -> Detailbaum aktualisieren.
    void showDocument(quint64 id);

    /// @name Verifiziertes Dokument-CRUD (DE-05) ueber den ViewerCore (DE-01):
    ///        false + error bei Fehler (Schema/Unique/JSON …), Erfolg wird geloggt + `dataMutated`.
    /// @{
    bool updateField(quint64 id, const QString& fieldPath, const QString& valueText, QString* error = nullptr);
    bool insertDocument(const QString& docJson, quint64* newId = nullptr, QString* error = nullptr);
    bool removeDocument(quint64 id, QString* error = nullptr);
    /// @}

    /// @brief Alle Models aktualisieren (z. B. nach databaseLoaded).
    void refreshAll();

    /// @brief Metrik-/Overflow-Text der aktuellen Collection (DB-07) als HTML.
    QString metricsText() const;

signals:
    void statusMessage(const QString& message);
    void collectionSelected(const QString& name);
    void documentSelected(quint64 id);
    void metricsTextChanged(const QString& html);

private:
    void onCollectionsChanged();
    void onCollectionChanged(const QString& name);

    ViewerCore* core_ = nullptr;
    DatabaseModel* dbModel_;
    DocumentTableModel* tableModel_;
    DocumentTreeModel* treeModel_;
    GraphScene* scene_;
    QString current_;
};

} // namespace memdb::viewer::detail
