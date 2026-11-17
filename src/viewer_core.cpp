#include "memdbviewer/viewer_core.hpp"

#include <memdb/memdb.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QTemporaryFile>
#include <QTimer>

#include <atomic>
#include <deque>
#include <map>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace memdb::viewer {

struct ViewerCore::Impl {
    explicit Impl(ViewerCore* q) : q(q) {
        db = std::make_unique<memdb::Database>("viewer");
        refs = std::make_unique<memdb::ReferenceIndex>(*db);
        syncTimer = new QTimer(q);
        syncTimer->setInterval(250);
        QObject::connect(syncTimer, &QTimer::timeout, q, [this] { syncCollections(); });
        syncTimer->start();
        subscribeAll();
    }

    void subscribeAll() {
        for (const auto& name : db->collectionNames()) {
            if (observerHandles.count(name))
                continue;
            auto coll = db->collection(name);
            const std::string collName = name;
            std::uint64_t handle = coll.onChange([this, collName](const memdb::ChangeEvent&) {
                QMetaObject::invokeMethod(q, [this, collName] {
                    emit q->collectionChanged(QString::fromStdString(collName));
                }, Qt::QueuedConnection);
            });
            observerHandles[name] = handle;
        }
        for (auto it = observerHandles.begin(); it != observerHandles.end();) {
            if (!db->hasCollection(it->first))
                it = observerHandles.erase(it); // offChange auf alten Handle ist unschaedlich
            else
                ++it;
        }
    }

    void syncCollections() {
        const auto names = db->collectionNames();
        bool changed = names.size() != lastNames.size();
        if (!changed) {
            for (std::size_t i = 0; i < names.size(); ++i)
                if (names[i] != lastNames[i]) { changed = true; break; }
        }
        lastNames = names;
        subscribeAll();
        if (changed)
            emit q->collectionsChanged();
    }

    ViewerCore* q;
    std::unique_ptr<memdb::Database> db;
    std::unique_ptr<memdb::ReferenceIndex> refs;
    memdb::ImportOptions importOptions;
    QString lastError;
    QString lastSource;
    std::map<std::string, std::uint64_t> observerHandles;
    std::vector<std::string> lastNames;
    QTimer* syncTimer = nullptr;
    std::vector<std::pair<std::thread, std::shared_ptr<std::atomic<bool>>>> workers;
    std::mutex workersMutex;
    std::atomic<bool> busy{false};
    std::atomic<bool> shuttingDown{false};
    // Warteschlange fuer asynchrone Ladevorgaenge (z. B. mehrere per Drag & Drop abgelegte Dateien)
    std::deque<std::pair<QString, std::function<void()>>> pending;
    std::mutex pendingMutex;
};

ViewerCore::ViewerCore(QObject* parent) : QObject(parent), impl_(std::make_unique<Impl>(this)) {}

ViewerCore::~ViewerCore() {
    impl_->shuttingDown = true;
    std::vector<std::pair<std::thread, std::shared_ptr<std::atomic<bool>>>> toJoin;
    {
        std::lock_guard<std::mutex> lock(impl_->workersMutex);
        toJoin.swap(impl_->workers);
    }
    for (auto& t : toJoin)
        if (t.first.joinable())
            t.first.join();
}

memdb::Database& ViewerCore::database() { return *impl_->db; }
const memdb::Database& ViewerCore::database() const { return *impl_->db; }
memdb::ReferenceIndex* ViewerCore::references() { return impl_->refs.get(); }

void ViewerCore::setImportOptions(const memdb::ImportOptions& opts) { impl_->importOptions = opts; }
const memdb::ImportOptions& ViewerCore::importOptions() const { return impl_->importOptions; }

void ViewerCore::setLastError(const QString& message) {
    impl_->lastError = message;
    emit lastErrorChanged(message);
}

QString ViewerCore::lastError() const { return impl_->lastError; }
QString ViewerCore::lastSourceDescription() const { return impl_->lastSource; }
QStringList ViewerCore::collectionNames() const {
    QStringList out;
    for (const auto& n : impl_->db->collectionNames()) {
        // interne Speicher-Collections (z. B. "__references" der FA-18) nicht anzeigen
        if (n.rfind("__", 0) == 0)
            continue;
        out << QString::fromStdString(n);
    }
    return out;
}

void ViewerCore::adoptDatabase(std::unique_ptr<memdb::Database> db, const QString& description) {
    impl_->db = std::move(db);
    impl_->refs = std::make_unique<memdb::ReferenceIndex>(*impl_->db);
    impl_->observerHandles.clear();
    impl_->lastNames.clear();
    impl_->subscribeAll();
    impl_->syncCollections();
    impl_->lastSource = description;
    emit databaseLoaded(description);
}

bool ViewerCore::isEncryptedSnapshotFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    return f.read(8) == QByteArray("INMEMDB\x01", 8);
}

QStringList ViewerCore::supportedSourceNames() {
    QStringList out;
    for (const auto& n : memdb::ImportRegistry::instance().sourceNames())
        out << QString::fromStdString(n);
    return out;
}

QStringList ViewerCore::supportedExtensions() {
    // Endungen per Probe ueber sourceForFile() ermitteln (filtert nicht verfuegbare
    // Quellen wie sqlite heraus, wenn InMemDB ohne INMEMDB_WITH_SQLITE gebaut wurde)
    static const std::vector<std::pair<QString, QString>> candidates = {
        {"csv", ".csv"}, {"json", ".json"}, {"jsonl", ".jsonl"}, {"ndjson", ".ndjson"},
        {"sqldump", ".sql"}, {"sqldump", ".dump"}, {"bson", ".bson"},
        {"sqlite", ".db"}, {"sqlite", ".sqlite"}, {"sqlite", ".sqlite3"},
        {"snapshot", ".inmemdb.json"},
    };
    QStringList out;
    out << QStringLiteral(".enc"); // verschluesselte Snapshots (FA-19)
    for (const auto& [name, ext] : candidates) {
        try {
            if (memdb::ImportRegistry::instance().sourceForFile("probe" + ext.toStdString()) ==
                name.toStdString())
                out << ext;
        } catch (...) {
        }
    }
    out.removeDuplicates();
    return out;
}

QString ViewerCore::fileDialogFilter() {
    QString all = QStringLiteral("Alle unterstuetzten Dateien");
    const QStringList exts = supportedExtensions();
    QStringList patterns;
    for (const QString& e : exts)
        patterns << QStringLiteral("*") + e;
    all += QStringLiteral(" (") + patterns.join(QLatin1Char(' ')) + QStringLiteral(")");
    QString filter = all + QStringLiteral(";;Alle Dateien (*)");
    return filter;
}

namespace {
/// @brief true, wenn die Datei wie ein FA-14-Database-Snapshot aussieht (Top-Level-Key "collections").
bool looksLikeDatabaseSnapshot(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QByteArray head = f.read(8192);
    return head.contains("\"collections\"");
}

QString describeStats(const QString& path, const QString& sourceName, const memdb::ImportStats& stats) {
    QStringList perTable;
    for (const auto& [table, rows] : stats.rowsPerTable)
        perTable << QStringLiteral("%1: %2").arg(QString::fromStdString(table)).arg(rows);
    QString s = QStringLiteral("%1 (%2): %3 Zeilen, %4 Collection(s)")
                    .arg(QFileInfo(path).fileName(), sourceName)
                    .arg(stats.totalRows)
                    .arg(stats.rowsPerTable.size());
    if (!perTable.isEmpty())
        s += QStringLiteral(" [") + perTable.join(QStringLiteral(", ")) + QStringLiteral("]");
    if (!stats.skippedTables.empty())
        s += QStringLiteral(" – uebersprungen: ") +
             QString::fromStdString(stats.skippedTables.front());
    if (stats.importedReferences > 0)
        s += QStringLiteral(" – %1 Referenzen").arg(stats.importedReferences);
    return s;
}
} // namespace

bool ViewerCore::importFileWith(const QString& path, const QString& sourceName, QString* error) {
    if (isEncryptedSnapshotFile(path)) {
        if (error)
            *error = QStringLiteral("Verschluesselter Snapshot – bitte ueber \"Snapshot oeffnen\" mit Passwort laden.");
        setLastError(error ? *error : QString());
        return false;
    }
    try {
        auto& registry = memdb::ImportRegistry::instance();
        auto source = registry.create(sourceName.toStdString(), path.toStdString());
        memdb::ImportStats stats = memdb::Importer::import(*impl_->db, *source, impl_->importOptions);
        impl_->lastSource = describeStats(path, sourceName, stats);
        impl_->syncCollections();
        log(impl_->lastSource);
        emit dataMutated();
        return true;
    } catch (const std::exception& e) {
        if (error)
            *error = QString::fromUtf8(e.what());
        setLastError(error ? *error : QString());
        return false;
    }
}

bool ViewerCore::importFile(const QString& path, QString* error) {
    if (isEncryptedSnapshotFile(path)) {
        if (error)
            *error = QStringLiteral("Verschluesselter Snapshot – bitte ueber \"Snapshot oeffnen\" mit Passwort laden.");
        setLastError(error ? *error : QString());
        return false;
    }
    QString sourceName;
    try {
        sourceName = QString::fromStdString(memdb::ImportRegistry::instance().sourceForFile(path.toStdString()));
    } catch (const std::exception&) {
        if (path.endsWith(QLatin1String(".json"), Qt::CaseInsensitive)) {
            sourceName = QStringLiteral("snapshot"); // FA-14-Snapshot per loadFromFile-Erkennung unten
        } else {
            const QString msg = QStringLiteral("Kein Import fuer Dateiendung von \"%1\" registriert.").arg(path);
            if (error)
                *error = msg;
            setLastError(msg);
            return false;
        }
    }
    if (sourceName == QLatin1String("json") && looksLikeDatabaseSnapshot(path))
        sourceName = QStringLiteral("snapshot");
    if (sourceName == QLatin1String("snapshot"))
        return importFileWith(path, QStringLiteral("snapshot"), error);
    return importFileWith(path, sourceName, error);
}

bool ViewerCore::loadSnapshot(const QString& path, QString* error) {
    try {
        auto db = std::make_unique<memdb::Database>(memdb::Database::loadFromFile(path.toStdString()));
        const QString desc = QStringLiteral("%1 (Snapshot): %2 Collections, %3 Dokumente")
                                 .arg(QFileInfo(path).fileName())
                                 .arg(db->collectionNames().size())
                                 .arg(db->metrics().totalDocs);
        adoptDatabase(std::move(db), desc);
        setLastError(QString());
        log(desc);
        emit dataMutated();
        return true;
    } catch (const std::exception& e) {
        if (error)
            *error = QString::fromUtf8(e.what());
        setLastError(error ? *error : QString());
        return false;
    }
}

bool ViewerCore::loadSnapshotEncrypted(const QString& path, const QString& password, QString* error) {
    try {
        auto db = std::make_unique<memdb::Database>(
            memdb::Database::loadFromFileEncrypted(path.toStdString(), password.toStdString()));
        const QString desc = QStringLiteral("%1 (Snapshot, entschluesselt): %2 Collections, %3 Dokumente")
                                 .arg(QFileInfo(path).fileName())
                                 .arg(db->collectionNames().size())
                                 .arg(db->metrics().totalDocs);
        adoptDatabase(std::move(db), desc);
        setLastError(QString());
        log(desc);
        emit dataMutated();
        return true;
    } catch (const std::exception& e) {
        if (error)
            *error = QString::fromUtf8(e.what());
        setLastError(error ? *error : QString());
        return false;
    }
}

void ViewerCore::resetDatabase(const QString& name) {
    emit databaseCleared();
    adoptDatabase(std::make_unique<memdb::Database>(name.toStdString()),
                  QStringLiteral("Neue leere Database \"%1\"").arg(name));
    log(QStringLiteral("Neue leere Database „%1“ angelegt.").arg(name));
    emit dataMutated();
}

void ViewerCore::loadDemoData() {
    auto db = std::make_unique<memdb::Database>("demo");
    auto users = db->createCollection("users");
    memdb::Schema sub;
    sub.add("plz", memdb::Value::Type::String);
    memdb::Schema schema;
    schema.add("name", memdb::Value::Type::String).notNull();
    schema.add("age", memdb::Value::Type::Int);
    schema.add("adresse", memdb::Value::Type::Document).subSchema(sub);
    users.setSchema(schema);
    users.createIndex("age", true);
    const memdb::DocumentId ada = users.insert(
        {{"name", "Ada"},
         {"age", 36},
         {"adresse", memdb::Value(std::make_shared<memdb::Document>(
                         memdb::Document{{"plz", "10115"}}))}});
    users.insert({{"name", "Bob"}, {"age", 17}});
    users.insert({{"name", "Cid"}, {"age", 44}});
    auto orders = db->createCollection("orders");
    orders.createIndex("total");
    const memdb::DocumentId o1 = orders.insert({{"total", 99.5}, {"status", "bezahlt"}, {"kunde_id", 1}});
    orders.insert({{"total", 12.0}, {"status", "offen"}, {"kunde_id", 1}});
    orders.insert({{"total", 7.25}, {"status", "offen"}, {"kunde_id", 2}});
    adoptDatabase(std::move(db), QStringLiteral("Demo-Daten: 2 Collections, 6 Dokumente, 1 Referenz"));
    references()->link("orders", o1, "users", ada, "kunde");
    log(QStringLiteral("Demo-Daten geladen (users, orders + 1 Referenz)."));
    emit dataMutated();
}

// ------------------------------------------------------------------ Mutationen

void ViewerCore::log(const QString& message, bool error) {
    emit logged(message, error);
}

namespace {
/// @brief Fängt memdb-Exceptions ab, schreibt Fehler ins Log, liefert false + error.
template <typename F>
bool verified(ViewerCore* core, const QString& what, QString* error, F&& fn) {
    try {
        fn();
        return true;
    } catch (const std::exception& e) {
        const QString msg = QStringLiteral("%1 fehlgeschlagen: %2").arg(what, QString::fromUtf8(e.what()));
        if (error)
            *error = QString::fromUtf8(e.what());
        core->log(msg, true);
        return false;
    }
}

/// @brief Wert-Text (aus Zellen-Editor) nach memdb::Value parsen: null/bool/int/double/string.
memdb::Value parseValueText(const QString& text) {
    const QString t = text.trimmed();
    if (t.compare(QLatin1String("null"), Qt::CaseInsensitive) == 0)
        return memdb::Value();
    if (t.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0)
        return memdb::Value(true);
    if (t.compare(QLatin1String("false"), Qt::CaseInsensitive) == 0)
        return memdb::Value(false);
    bool ok = false;
    const long long i = t.toLongLong(&ok);
    if (ok && QString::number(i) == t)
        return memdb::Value(i);
    const double d = t.toDouble(&ok);
    if (ok && t.contains(QLatin1Char('.')))
        return memdb::Value(d);
    return memdb::Value(t.toStdString());
}
} // namespace

bool ViewerCore::createCollection(const QString& name, QString* error) {
    const bool ok = verified(this, QStringLiteral("Collection „%1“ anlegen").arg(name), error, [&] {
        impl_->db->createCollection(name.toStdString());
    });
    if (ok) {
        impl_->syncCollections();
        log(QStringLiteral("Collection „%1“ angelegt.").arg(name));
        emit dataMutated();
    }
    return ok;
}

bool ViewerCore::dropCollection(const QString& name, QString* error) {
    const bool ok = verified(this, QStringLiteral("Collection „%1“ löschen").arg(name), error, [&] {
        if (!impl_->db->dropCollection(name.toStdString()))
            throw memdb::Error("Collection existiert nicht: " + name.toStdString());
    });
    if (ok) {
        impl_->syncCollections();
        log(QStringLiteral("Collection „%1“ gelöscht.").arg(name));
        emit dataMutated();
    }
    return ok;
}

bool ViewerCore::clearCollectionContent(const QString& name, QString* error) {
    const bool ok = verified(this, QStringLiteral("Collection „%1“ leeren").arg(name), error, [&] {
        impl_->db->collection(name.toStdString()).clear();
    });
    if (ok) {
        log(QStringLiteral("Collection „%1“ geleert.").arg(name));
        emit dataMutated();
    }
    return ok;
}

bool ViewerCore::setCollectionSchema(const QString& collection, const memdb::Schema& schema, QString* error) {
    const bool ok = verified(this, QStringLiteral("Schema für „%1“ setzen").arg(collection), error, [&] {
        impl_->db->collection(collection.toStdString()).setSchema(schema); // prueft alle Dokumente (FA-20)
    });
    if (ok) {
        log(QStringLiteral("Schema für „%1“ gesetzt (%2 Felder) – alle Dokumente valide.")
                .arg(collection)
                .arg(schema.size()));
        emit dataMutated();
    }
    return ok;
}

bool ViewerCore::clearCollectionSchema(const QString& collection, QString* error) {
    const bool ok = verified(this, QStringLiteral("Schema von „%1“ entfernen").arg(collection), error, [&] {
        impl_->db->collection(collection.toStdString()).clearSchema();
    });
    if (ok) {
        log(QStringLiteral("Schema von „%1“ entfernt (schema-los).").arg(collection));
        emit dataMutated();
    }
    return ok;
}

bool ViewerCore::createIndex(const QString& collection, const QString& fieldPath, bool unique, QString* error) {
    const bool ok = verified(this, QStringLiteral("Index auf %1.%2 anlegen").arg(collection, fieldPath), error, [&] {
        impl_->db->collection(collection.toStdString()).createIndex(fieldPath.toStdString(), unique);
    });
    if (ok) {
        log(QStringLiteral("Index auf %1.%2 angelegt (%3).")
                .arg(collection, fieldPath, unique ? QStringLiteral("unique") : QStringLiteral("index")));
        emit dataMutated();
    }
    return ok;
}

bool ViewerCore::dropIndex(const QString& collection, const QString& fieldPath, QString* error) {
    const bool ok = verified(this, QStringLiteral("Index auf %1.%2 löschen").arg(collection, fieldPath), error, [&] {
        if (!impl_->db->collection(collection.toStdString()).dropIndex(fieldPath.toStdString()))
            throw memdb::Error("kein Index auf " + fieldPath.toStdString());
    });
    if (ok) {
        log(QStringLiteral("Index auf %1.%2 gelöscht.").arg(collection, fieldPath));
        emit dataMutated();
    }
    return ok;
}

bool ViewerCore::linkReference(const QString& fromColl, quint64 fromId, bool hasFromId, const QString& toColl,
                               quint64 toId, bool hasToId, const QString& relation, QString* error) {
    const bool ok = verified(this, QStringLiteral("Referenz %1 -> %2 anlegen").arg(fromColl, toColl), error, [&] {
        impl_->refs->link(fromColl.toStdString(), hasFromId ? std::optional<memdb::DocumentId>(memdb::DocumentId(fromId)) : std::nullopt,
                          toColl.toStdString(), hasToId ? std::optional<memdb::DocumentId>(memdb::DocumentId(toId)) : std::nullopt,
                          relation.toStdString());
    });
    if (ok) {
        log(QStringLiteral("Referenz „%1“: %2 -> %3 angelegt.").arg(relation, fromColl, toColl));
        emit dataMutated();
    }
    return ok;
}

bool ViewerCore::unlinkReference(const QString& fromColl, quint64 fromId, bool hasFromId, const QString& toColl,
                                 quint64 toId, bool hasToId, const QString& relation, QString* error) {
    const bool ok = verified(this, QStringLiteral("Referenz %1 -> %2 entfernen").arg(fromColl, toColl), error, [&] {
        impl_->refs->unlink(fromColl.toStdString(), hasFromId ? std::optional<memdb::DocumentId>(memdb::DocumentId(fromId)) : std::nullopt,
                            toColl.toStdString(), hasToId ? std::optional<memdb::DocumentId>(memdb::DocumentId(toId)) : std::nullopt,
                            relation.toStdString());
    });
    if (ok) {
        log(QStringLiteral("Referenz „%1“: %2 -> %3 entfernt.").arg(relation, fromColl, toColl));
        emit dataMutated();
    }
    return ok;
}

std::size_t ViewerCore::pruneDanglingReferences(QString* error) {
    std::size_t n = 0;
    const bool ok = verified(this, QStringLiteral("Dangling-Referenzen bereinigen"), error, [&] {
        n = impl_->refs->pruneDangling();
    });
    if (ok) {
        log(QStringLiteral("%1 dangling Referenzen entfernt.").arg(n));
        emit dataMutated();
    }
    return ok ? n : 0;
}

bool ViewerCore::insertDocumentJson(const QString& collection, const QString& docJson, quint64* newId,
                                    QString* error) {
    quint64 id = 0;
    const bool ok = verified(this, QStringLiteral("Dokument in „%1“ einfügen").arg(collection), error, [&] {
        // JSON-Objekt ueber eine Hilfs-Database parsen (oeffentliche API)
        const std::string wrapped = "{\"name\":\"__tmp\",\"collections\":{\"__tmp\":{\"documents\":[" +
                                    docJson.toStdString() + "]}}}";
        memdb::Database tmp = memdb::Database::fromJson(wrapped, "__tmp");
        const auto doc = tmp.collection("__tmp").findOne();
        if (!doc)
            throw memdb::Error("kein Dokument im JSON");
        id = impl_->db->collection(collection.toStdString()).insert(*doc).value();
    });
    if (ok) {
        if (newId)
            *newId = id;
        log(QStringLiteral("Dokument #%1 in „%2“ eingefügt.").arg(id).arg(collection));
        emit dataMutated();
    }
    return ok;
}

bool ViewerCore::updateDocumentField(const QString& collection, quint64 id, const QString& fieldPath,
                                     const QString& valueText, QString* error) {
    const bool ok = verified(this, QStringLiteral("Feld %1.%2 aktualisieren").arg(collection, fieldPath), error, [&] {
        const memdb::Value v = parseValueText(valueText);
        const bool changed = impl_->db->collection(collection.toStdString())
                                 .updateById(memdb::DocumentId(id), [&](memdb::Document& d) {
                                     d.setPath(fieldPath.toStdString(), v);
                                 });
        if (!changed)
            throw memdb::Error("Dokument #" + std::to_string(id) + " nicht vorhanden");
    });
    if (ok) {
        log(QStringLiteral("Dokument #%1 in „%2“: %3 = %4").arg(id).arg(collection, fieldPath, valueText));
        emit dataMutated();
    }
    return ok;
}

bool ViewerCore::removeDocument(const QString& collection, quint64 id, QString* error) {
    const bool ok = verified(this, QStringLiteral("Dokument #%1 aus „%2“ löschen").arg(id).arg(collection), error, [&] {
        if (!impl_->db->collection(collection.toStdString()).removeById(memdb::DocumentId(id)))
            throw memdb::Error("Dokument #" + std::to_string(id) + " nicht vorhanden");
    });
    if (ok) {
        log(QStringLiteral("Dokument #%1 aus „%2“ gelöscht.").arg(id).arg(collection));
        emit dataMutated();
    }
    return ok;
}

bool ViewerCore::importCsvText(const QString& tableName, const QString& csvText, const memdb::CsvImportOptions& opts,
                               QString* error) {
    const bool ok = verified(this, QStringLiteral("CSV aus Zwischenablage importieren („%1“)").arg(tableName), error, [&] {
        QTemporaryFile tmp(QDir::tempPath() + QStringLiteral("/viewer-clip-XXXXXX.csv"));
        tmp.setAutoRemove(true);
        if (!tmp.open())
            throw memdb::Error("Temp-Datei nicht beschreibbar");
        tmp.write(csvText.toUtf8());
        tmp.flush();
        memdb::CsvImportOptions o = opts;
        if (!tableName.isEmpty())
            o.tableName = tableName.toStdString();
        memdb::CsvImportSource src(tmp.fileName().toStdString(), o);
        const auto stats = memdb::Importer::import(*impl_->db, src, impl_->importOptions);
        impl_->lastSource = describeStats(QStringLiteral(" Zwischenablage.csv"), QStringLiteral("csv"), stats);
        impl_->syncCollections();
    });
    if (ok) {
        log(QStringLiteral("CSV aus Zwischenablage als „%1“ importiert – %2.").arg(tableName, impl_->lastSource));
        emit dataMutated();
    }
    return ok;
}

bool ViewerCore::saveSnapshot(const QString& path, QString* error) {
    const bool ok = verified(this, QStringLiteral("Snapshot speichern (%1)").arg(path), error, [&] {
        impl_->db->saveToFile(path.toStdString());
    });
    if (ok)
        log(QStringLiteral("Snapshot gespeichert: %1 (%2 Collections, %3 Dokumente).")
                .arg(path)
                .arg(impl_->db->collectionNames().size())
                .arg(impl_->db->metrics().totalDocs));
    return ok;
}

bool ViewerCore::saveSnapshotEncrypted(const QString& path, const QString& password, QString* error) {
    const bool ok = verified(this, QStringLiteral("Verschlüsselten Snapshot speichern (%1)").arg(path), error, [&] {
        impl_->db->saveToFileEncrypted(path.toStdString(), password.toStdString());
    });
    if (ok)
        log(QStringLiteral("Verschlüsselter Snapshot (FA-19) gespeichert: %1.").arg(path));
    return ok;
}

bool ViewerCore::isBusy() const { return impl_->busy.load(); }

void ViewerCore::runAsync(const QString& path, std::function<void()> job) {
    {
        std::lock_guard<std::mutex> lock(impl_->pendingMutex);
        if (impl_->busy.load()) {
            // bereits laufend -> an die Warteschlange (z. B. mehrere Drag&Drop-Dateien)
            impl_->pending.emplace_back(path, std::move(job));
            emit importStarted(path);
            return;
        }
        impl_->busy.store(true);
    }
    emit importStarted(path);
    auto done = std::make_shared<std::atomic<bool>>(false);
    std::thread t([this, job = std::move(job), done] {
        job();
        done->store(true);
        QMetaObject::invokeMethod(this, [this] {
            impl_->busy.store(false);
            // naechsten wartenden Vorgang starten (GUI-Thread)
            std::pair<QString, std::function<void()>> next;
            {
                std::lock_guard<std::mutex> lock(impl_->pendingMutex);
                if (!impl_->pending.empty()) {
                    next = std::move(impl_->pending.front());
                    impl_->pending.pop_front();
                }
            }
            if (!next.first.isEmpty() || next.second)
                runAsync(next.first, std::move(next.second));
        }, Qt::QueuedConnection);
    });
    std::lock_guard<std::mutex> lock(impl_->workersMutex);
    auto& w = impl_->workers;
    for (auto it = w.begin(); it != w.end();) {
        if (it->second->load() && it->first.joinable()) {
            it->first.join();
            it = w.erase(it);
        } else {
            ++it;
        }
    }
    if (impl_->shuttingDown) {
        if (t.joinable())
            t.join();
    } else {
        w.emplace_back(std::move(t), done);
    }
}

void ViewerCore::importFileAsync(const QString& path) {
    runAsync(path, [this, path] {
        QString error;
        const bool ok = importFile(path, &error);
        QMetaObject::invokeMethod(this, [this, path, ok, error] {
            emit importFinished(path, ok, ok ? impl_->lastSource : error);
        }, Qt::QueuedConnection);
    });
}

void ViewerCore::loadSnapshotAsync(const QString& path) {
    runAsync(path, [this, path] {
        QString error;
        try {
            auto db = std::make_unique<memdb::Database>(memdb::Database::loadFromFile(path.toStdString()));
            const QString desc = QStringLiteral("%1 (Snapshot): %2 Collections, %3 Dokumente")
                                     .arg(QFileInfo(path).fileName())
                                     .arg(db->collectionNames().size())
                                     .arg(db->metrics().totalDocs);
            QMetaObject::invokeMethod(this, [this, db = std::move(db), desc, path]() mutable {
                adoptDatabase(std::move(db), desc);
                emit importFinished(path, true, desc);
            }, Qt::QueuedConnection);
        } catch (const std::exception& e) {
            error = QString::fromUtf8(e.what());
            QMetaObject::invokeMethod(this, [this, path, error] {
                setLastError(error);
                emit importFinished(path, false, error);
            }, Qt::QueuedConnection);
        }
    });
}

void ViewerCore::loadSnapshotEncryptedAsync(const QString& path, const QString& password) {
    runAsync(path, [this, path, password] {
        QString error;
        try {
            auto db = std::make_unique<memdb::Database>(
                memdb::Database::loadFromFileEncrypted(path.toStdString(), password.toStdString()));
            const QString desc = QStringLiteral("%1 (Snapshot, entschluesselt): %2 Collections, %3 Dokumente")
                                     .arg(QFileInfo(path).fileName())
                                     .arg(db->collectionNames().size())
                                     .arg(db->metrics().totalDocs);
            QMetaObject::invokeMethod(this, [this, db = std::move(db), desc, path]() mutable {
                adoptDatabase(std::move(db), desc);
                emit importFinished(path, true, desc);
            }, Qt::QueuedConnection);
        } catch (const std::exception& e) {
            error = QString::fromUtf8(e.what());
            QMetaObject::invokeMethod(this, [this, path, error] {
                setLastError(error);
                emit importFinished(path, false, error);
            }, Qt::QueuedConnection);
        }
    });
}

} // namespace memdb::viewer
