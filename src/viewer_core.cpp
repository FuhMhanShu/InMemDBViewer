#include "memdbviewer/viewer_core.hpp"

#include <memdb/memdb.hpp>

#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QTimer>

#include <atomic>
#include <map>
#include <mutex>
#include <thread>
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
}

bool ViewerCore::isBusy() const { return impl_->busy.load(); }

void ViewerCore::runAsync(const QString& path, std::function<void()> job) {
    bool expected = false;
    if (!impl_->busy.compare_exchange_strong(expected, true)) {
        emit importFinished(path, false, QStringLiteral("Es laeuft bereits ein Import/Ladevorgang."));
        return;
    }
    emit importStarted(path);
    auto done = std::make_shared<std::atomic<bool>>(false);
    std::thread t([this, job = std::move(job), done] {
        job();
        done->store(true);
        QMetaObject::invokeMethod(this, [this] {
            impl_->busy = false;
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
