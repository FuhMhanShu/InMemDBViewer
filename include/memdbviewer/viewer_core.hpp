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
struct CsvImportOptions;
class Schema;
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

    /// @name Verifizierte Mutationen (Design/CRUD): true bei Erfolg, sonst false + `error`;
    ///        jede Operation wird ins Log geschrieben und stoesst `dataMutated()` an.
    /// @{
    void log(const QString& message, bool error = false);

    bool createCollection(const QString& name, QString* error = nullptr);
    bool dropCollection(const QString& name, QString* error = nullptr);
    bool clearCollectionContent(const QString& name, QString* error = nullptr);
    /// @brief Setzt das strikte Schema (FA-20); InMemDB prueft alle vorhandenen Dokumente (SchemaError).
    bool setCollectionSchema(const QString& collection, const memdb::Schema& schema, QString* error = nullptr);
    bool clearCollectionSchema(const QString& collection, QString* error = nullptr);
    /// @brief Legt einen Index an (FA-10); bei unique werden Duplikate geprueft (UniqueConstraintError).
    bool createIndex(const QString& collection, const QString& fieldPath, bool unique, QString* error = nullptr);
    bool dropIndex(const QString& collection, const QString& fieldPath, QString* error = nullptr);
    /// @brief Referenz (FA-18) anlegen; id 0 + hasId=false = Collection-Level.
    bool linkReference(const QString& fromColl, quint64 fromId, bool hasFromId, const QString& toColl,
                       quint64 toId, bool hasToId, const QString& relation, QString* error = nullptr);
    bool unlinkReference(const QString& fromColl, quint64 fromId, bool hasFromId, const QString& toColl,
                         quint64 toId, bool hasToId, const QString& relation, QString* error = nullptr);
    std::size_t pruneDanglingReferences(QString* error = nullptr);
    /// @brief Dokument aus JSON-Objekt einfuegen (z. B. `{"name":"Ada","age":36}`).
    bool insertDocumentJson(const QString& collection, const QString& docJson, quint64* newId = nullptr,
                            QString* error = nullptr);
    /// @brief Feld eines Dokuments setzen (Punktnotation); Wert wird als null/bool/int/double/string interpretiert.
    bool updateDocumentField(const QString& collection, quint64 id, const QString& fieldPath,
                             const QString& valueText, QString* error = nullptr);
    bool removeDocument(const QString& collection, quint64 id, QString* error = nullptr);
    /// @brief CSV-Text (z. B. aus der Zwischenablage) als Collection importieren (FA-21).
    bool importCsvText(const QString& tableName, const QString& csvText, const memdb::CsvImportOptions& opts,
                       QString* error = nullptr);
    /// @brief Snapshot speichern (FA-14 / verschluesselt FA-19).
    bool saveSnapshot(const QString& path, QString* error = nullptr);
    bool saveSnapshotEncrypted(const QString& path, const QString& password, QString* error = nullptr);
    /// @}
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
    /// @brief Log-Ausgabe (Ausgabe-Panel): `error` = Fehler (rot dargestellt).
    void logged(const QString& message, bool error);
    /// @brief Eine Datenmanipulation war erfolgreich -> Discovery/Analyse anstossen.
    void dataMutated();
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
