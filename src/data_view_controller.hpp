#pragma once

#include <QObject>
#include <QString>

namespace memdb {
class Database;
}
class QTimer;

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
    /// @brief Wie applyQuery, aber die Query läuft im Worker-Thread (UI blockiert nicht, UI-09);
    ///        Ergebnis landet per queued Aufruf im Model und wird über `queryApplied` gemeldet.
    void applyQueryAsync(const QString& dsl);
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
    /// @brief Ergebnis der asynchronen Query (UI-09): Zeilen, Index genutzt, Kandidaten.
    void queryApplied(int rowCount, bool usedIndex, std::size_t candidates);

private:
    void onCollectionsChanged(); // fuehrt den Refresh sofort aus
    void scheduleRefresh();      // UI-08: bündelt Change-Events (SingleShot-Timer, restart bei laufendem Timer)

    ViewerCore* core_ = nullptr;
    QTimer* debounce_ = nullptr;
    DatabaseModel* dbModel_;
    DocumentTableModel* tableModel_;
    DocumentTreeModel* treeModel_;
    GraphScene* scene_;
    QString current_;
};

} // namespace memdb::viewer::detail
