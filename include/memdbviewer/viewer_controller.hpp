#pragma once

#include <memdb/database.hpp>

#include <QObject>
#include <QString>

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace memdb::viewer {

class DatabaseModel;

/// @brief Controller (MVC): haelt die visualisierte Database, stellt das DatabaseModel
///        bereit und uebersetzt InMemDB-Observer-Events (FA-15, auch nach Tx-Commit)
///        per Qt::QueuedConnection in Qt-Signale (FA-08, NFA-04).
///
/// Die Database ist nicht-owning und muss den Controller ueberdauern.
class ViewerController : public QObject {
    Q_OBJECT
public:
    explicit ViewerController(QObject* parent = nullptr);
    ~ViewerController() override;

    /// @brief Zu visualisierende Database setzen; registriert Observer auf allen Collections.
    ///        Uebernimmt nicht das Eigentum (externe Database muss den Controller ueberdauern).
    void setDatabase(memdb::Database& db);
    memdb::Database* database() const { return db_; }

    /// @brief Snapshot-Datei (FA-14 JSON) laden (FA-12): legt ein eigenes Database-Objekt an.
    /// @return false bei IO-/JSON-Fehler (Details in lastError()).
    bool openSnapshot(const QString& path);
    /// @brief Verschlüsselten Snapshot (FA-19) laden (FA-12).
    bool openSnapshotEncrypted(const QString& path, const QString& password);
    /// @brief Letzter Fehler von openSnapshot*/setDatabase-bezogenen Operationen.
    QString lastError() const { return lastError_; }

    /// @brief Collection selektieren (FA-09); emittiert collectionSelected, wenn vorhanden.
    Q_INVOKABLE void selectCollection(const QString& collectionName);

    /// @brief Model ueber alle Collections inkl. Schema-Felder/Indizes (FA-03).
    DatabaseModel* databaseModel() const { return model_; }

    /// @brief Alles neu einlesen: Observer (auch fuer neue Collections) registrieren,
    ///        Model aktualisieren, databaseChanged emittieren.
    Q_INVOKABLE void refresh();

signals:
    /// @brief Database wurde gesetzt oder vollstaendig neu aufgebaut.
    void databaseChanged();
    /// @brief Eine Collection hat sich geaendert (Observer-Event); das DatabaseModel
    ///        wird darueber automatisch refresht.
    void collectionChanged(const QString& collectionName);
    /// @brief Selektion einer Collection (FA-09), z. B. Klick auf einen Szenen-Knoten.
    void collectionSelected(const QString& collectionName);

private:
    void registerObservers();
    void unregisterObservers();
    void adoptDatabase(std::unique_ptr<memdb::Database> db);

    memdb::Database* db_ = nullptr;
    std::unique_ptr<memdb::Database> ownedDb_; ///< eigenes Objekt nach openSnapshot*
    DatabaseModel* model_ = nullptr;
    QString lastError_;
    std::unordered_map<std::string, std::uint64_t> observerHandles_;
};

} // namespace memdb::viewer
