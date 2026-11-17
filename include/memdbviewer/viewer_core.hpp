#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

namespace memdb {
class Database;
class ReferenceIndex;
struct ImportOptions;
} // namespace memdb

namespace memdb::viewer {

/// @brief Zentrales Model der Viewer-Anwendung (MVC): haelt die `memdb::Database`
///        und den `ReferenceIndex`, laedt externe Daten (FA-21 ImportRegistry:
///        CSV/JSON/JSONL/SQL-Dump/BSON/SQLite/Snapshot) sowie InMemDB-Snapshots
///        (FA-14, verschluesselt FA-19) und verteilt Aenderungen als Qt-Signale
///        an alle Widgets (DataInputWidget, DataViewWidget, AnalysisWidget).
///
/// Observer-Bridge (FA-15): `Collection::onChange` wird per QueuedConnection in
/// den GUI-Thread gehoben (`collectionChanged`), neue/geloeschte Collections
/// per leichtem Auto-Sync (Timer-Vergleich `collectionNames()` -> `collectionsChanged`).
///
/// Laden kann synchron (`importFile`/`loadSnapshot`, fuer Tests) oder asynchron
/// (`importFileAsync` …, Worker-Thread + queued Signale, fuer die UI) erfolgen.
class ViewerCore : public QObject {
    Q_OBJECT
public:
    explicit ViewerCore(QObject* parent = nullptr);
    ~ViewerCore() override;

    /// @brief Laufende Database (immer gesetzt; frische leere Database nach dem Ctor).
    memdb::Database& database();
    const memdb::Database& database() const;
    /// @brief Referenz-Index (FA-18) zur aktuellen Database.
    memdb::ReferenceIndex* references();

    /// @brief Import-Optionen (FA-21), die `importFile*` verwendet.
    void setImportOptions(const memdb::ImportOptions& opts);
    const memdb::ImportOptions& importOptions() const;

    /// @name Synchrones Laden (GUI-Thread oder Test; blockiert)
    /// @{
    /// @brief Importiert eine Datei ueber `ImportRegistry` (Auto-Erkennung per Endung:
    /// csv/json/jsonl/ndjson/sql/sqldump/bson/sqlite/snapshot). InMemDB-Snapshots
    /// (FA-14, inkl. `.inmemdb.json`) werden als Snapshot in die aktuelle Database
    /// importiert. Verschluesselte Snapshots (Magic `INMEMDB\x01`) werden abgelehnt
    /// -> `loadSnapshotEncrypted` verwenden. Rueckgabe false + `error` bei Fehler.
    bool importFile(const QString& path, QString* error = nullptr);
    /// @brief Wie `importFile`, aber mit explizitem Registry-Namen (z. B. "snapshot").
    bool importFileWith(const QString& path, const QString& sourceName, QString* error = nullptr);
    /// @brief Ersetzt die gesamte Database durch einen FA-14-JSON-Snapshot.
    bool loadSnapshot(const QString& path, QString* error = nullptr);
    /// @brief Wie `loadSnapshot`, fuer verschluesselte Snapshots (FA-19) mit Passwort.
    bool loadSnapshotEncrypted(const QString& path, const QString& password, QString* error = nullptr);
    /// @brief Frische, leere Database (alle Widgets aktualisieren sich via `databaseCleared`/`databaseLoaded`).
    void resetDatabase(const QString& name = QStringLiteral("viewer"));
    /// @brief Befuellt die Database mit kleinen Demo-Daten (2 Collections, Schema, Index, Referenz).
    void loadDemoData();
    /// @}

    /// @name Asynchrones Laden (Worker-Thread, UI bleibt responsiv)
    /// @{
    void importFileAsync(const QString& path);
    void loadSnapshotAsync(const QString& path);
    void loadSnapshotEncryptedAsync(const QString& path, const QString& password);
    bool isBusy() const;
    /// @}

    QString lastError() const;
    /// @brief Beschreibung der letzten Ladeaktion, z. B. "personen.csv (csv): 3 Zeilen, 1 Collection".
    QString lastSourceDescription() const;
    QStringList collectionNames() const;

    /// @brief true, wenn die Datei am Magic-Header (`INMEMDB\x01`) als verschluesselter Snapshot erkannt wird (FA-19).
    static bool isEncryptedSnapshotFile(const QString& path);
    /// @brief Registrierte Import-Quellen-Namen (ImportRegistry, z. B. "csv", "sqlite" falls verfuegbar).
    static QStringList supportedSourceNames();
    /// @brief Endungen, die `importFile` verarbeiten kann (mit Punkt, z. B. ".csv"), plus `.enc`.
    static QStringList supportedExtensions();
    /// @brief Filter-String fuer QFileDialog.
    static QString fileDialogFilter();

signals:
    /// @brief Neue Database uebernommen (Snapshot/Demo/Reset) – alle Widgets neu aufsetzen.
    void databaseLoaded(const QString& sourceDescription);
    void databaseCleared();
    /// @brief Dokument-Aenderung in einer Collection (Observer FA-15, queued).
    void collectionChanged(const QString& collectionName);
    /// @brief Collection-Liste hat sich geaendert (Auto-Sync).
    void collectionsChanged();
    void lastErrorChanged(const QString& message);
    void importStarted(const QString& path);
    /// @brief Asynchrones Laden fertig; `message` = Stats-Zusammenfassung oder Fehlertext.
    void importFinished(const QString& path, bool ok, const QString& message);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    void adoptDatabase(std::unique_ptr<memdb::Database> db, const QString& description);
    void runAsync(const QString& path, std::function<void()> job);
    void setLastError(const QString& message);
};

} // namespace memdb::viewer
