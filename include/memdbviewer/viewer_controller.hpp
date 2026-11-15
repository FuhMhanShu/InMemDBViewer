#pragma once

#include <memdb/database.hpp>

#include <QObject>
#include <QString>

#include <cstdint>
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
    void setDatabase(memdb::Database& db);
    memdb::Database* database() const { return db_; }

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

private:
    void registerObservers();
    void unregisterObservers();

    memdb::Database* db_ = nullptr;
    DatabaseModel* model_ = nullptr;
    std::unordered_map<std::string, std::uint64_t> observerHandles_;
};

} // namespace memdb::viewer
