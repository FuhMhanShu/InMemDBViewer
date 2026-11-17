#include "data_view_controller.hpp"

#include "database_model.hpp"
#include "document_table_model.hpp"
#include "document_tree_model.hpp"
#include "graph_scene.hpp"

#include "memdbviewer/viewer_core.hpp"

#include <memdb/memdb.hpp>

namespace memdb::viewer::detail {

DataViewController::DataViewController(ViewerCore* core, DatabaseModel* dbModel,
                                       DocumentTableModel* tableModel, DocumentTreeModel* treeModel,
                                       GraphScene* scene, QObject* parent)
    : QObject(parent), dbModel_(dbModel), tableModel_(tableModel), treeModel_(treeModel), scene_(scene) {
    // Zellen-Edit aus der Tabelle -> verifizierte Mutation (DE-05); nur einmal verbinden
    connect(tableModel_, &DocumentTableModel::editRequested, this,
            [this](quint64 id, const QString& field, const QString& value) {
                QString err;
                if (!updateField(id, field, value, &err))
                    tableModel_->refresh(); // Anzeige auf alten Wert zuruecksetzen
            });
    setCore(core);
}

void DataViewController::setCore(ViewerCore* core) {
    if (core_)
        disconnect(core_, nullptr, this, nullptr);
    core_ = core;
    if (!core_)
        return;
    dbModel_->setDatabase(&core_->database());
    connect(core_, &ViewerCore::databaseLoaded, this, [this](const QString&) { refreshAll(); });
    connect(core_, &ViewerCore::databaseCleared, this, [this] { refreshAll(); });
    connect(core_, &ViewerCore::collectionsChanged, this, [this] { onCollectionsChanged(); });
    connect(core_, &ViewerCore::collectionChanged, this, [this](const QString& name) { onCollectionChanged(name); });
    // jede verifizierte Mutation (DE-01) -> Struktur/Models neu aufbauen (Schema/Indizes feuern kein collectionChanged)
    connect(core_, &ViewerCore::dataMutated, this, [this] { onCollectionsChanged(); });
    refreshAll();
}

void DataViewController::refreshAll() {
    if (!core_)
        return;
    // adoptDatabase() ersetzt die Database-Instanz -> Zeiger immer neu holen
    dbModel_->setDatabase(&core_->database());
    scene_->rebuild(core_->database(), core_->references());
    onCollectionsChanged();
}

void DataViewController::onCollectionsChanged() {
    if (!core_)
        return;
    if (dbModel_->database() != &core_->database())
        dbModel_->setDatabase(&core_->database());
    dbModel_->refresh();
    scene_->rebuild(core_->database(), core_->references());
    // Auswahl beibehalten, sonst erste Collection waehlen
    if (current_.isEmpty() || !core_->collectionNames().contains(current_)) {
        const QStringList names = core_->collectionNames();
        if (!names.isEmpty())
            selectCollection(names.first());
        else {
            current_.clear();
            tableModel_->setCollection(std::nullopt);
            treeModel_->clearDocument();
            emit collectionSelected(QString());
        }
    }
    emit metricsTextChanged(metricsText());
}

void DataViewController::onCollectionChanged(const QString& name) {
    if (!core_)
        return;
    if (name.startsWith(QLatin1String("__")))
        scene_->rebuild(core_->database(), core_->references()); // Referenzen geaendert
    if (name == current_) {
        tableModel_->refresh();
        emit metricsTextChanged(metricsText());
    }
    if (name == current_ || name.startsWith(QLatin1String("__")))
        dbModel_->refresh();
}

void DataViewController::selectCollection(const QString& name) {
    if (!core_ || name.isEmpty() || !core_->database().hasCollection(name.toStdString()))
        return;
    current_ = name;
    tableModel_->setCollection(core_->database().collection(name.toStdString()));
    treeModel_->clearDocument();
    scene_->setHighlighted(name);
    emit collectionSelected(name);
    emit metricsTextChanged(metricsText());
    emit statusMessage(QStringLiteral("Collection „%1“: %2 Dokumente")
                           .arg(name)
                           .arg(core_->database().collection(name.toStdString()).size()));
}

bool DataViewController::applyQuery(const QString& dsl, QString* error) {
    const bool ok = tableModel_->applyQuery(dsl, error);
    if (ok) {
        const bool usedIndex = tableModel_->lastQueryUsedIndex();
        const std::size_t cand = tableModel_->lastQueryCandidateCount();
        const std::size_t total = core_ && core_->database().hasCollection(current_.toStdString())
                                      ? core_->database().collection(current_.toStdString()).size()
                                      : 0;
        emit statusMessage(QStringLiteral("Filter „%1“: %2 Treffer (Index: %3, Kandidaten: %4 von %5)")
                               .arg(dsl, QString::number(tableModel_->rowCount()),
                                    usedIndex ? QStringLiteral("ja") : QStringLiteral("nein"))
                               .arg(cand)
                               .arg(total));
    } else if (error) {
        emit statusMessage(QStringLiteral("Query-Fehler: %1").arg(*error));
    }
    return ok;
}

void DataViewController::clearQuery() {
    tableModel_->clearQuery();
    emit statusMessage(QStringLiteral("Filter zurueckgesetzt."));
}

void DataViewController::showDocument(quint64 id) {
    if (!core_ || current_.isEmpty())
        return;
    auto doc = core_->database().collection(current_.toStdString()).findById(memdb::DocumentId(id));
    if (doc)
        treeModel_->setDocument(*doc);
    else
        treeModel_->clearDocument();
    emit documentSelected(id);
}

bool DataViewController::updateField(quint64 id, const QString& fieldPath, const QString& valueText,
                                     QString* error) {
    if (!core_ || current_.isEmpty()) {
        if (error)
            *error = QStringLiteral("Keine Collection ausgewaehlt.");
        return false;
    }
    const bool ok = core_->updateDocumentField(current_, id, fieldPath, valueText, error);
    emit statusMessage(ok ? QStringLiteral("Gespeichert: %1 #%2: %3 = %4")
                              .arg(current_)
                              .arg(id)
                              .arg(fieldPath, valueText)
                          : QStringLiteral("Fehler: %1").arg(error ? *error : QString()));
    return ok;
}

bool DataViewController::insertDocument(const QString& docJson, quint64* newId, QString* error) {
    if (!core_ || current_.isEmpty()) {
        if (error)
            *error = QStringLiteral("Keine Collection ausgewaehlt.");
        return false;
    }
    quint64 id = 0;
    const bool ok = core_->insertDocumentJson(current_, docJson, &id, error);
    if (ok) {
        if (newId)
            *newId = id;
        showDocument(id);
        emit statusMessage(QStringLiteral("Dokument #%1 in „%2“ eingefuegt.").arg(id).arg(current_));
    } else {
        emit statusMessage(QStringLiteral("Fehler: %1").arg(error ? *error : QString()));
    }
    return ok;
}

bool DataViewController::removeDocument(quint64 id, QString* error) {
    if (!core_ || current_.isEmpty()) {
        if (error)
            *error = QStringLiteral("Keine Collection ausgewaehlt.");
        return false;
    }
    const bool ok = core_->removeDocument(current_, id, error);
    if (ok) {
        treeModel_->clearDocument();
        emit statusMessage(QStringLiteral("Dokument #%1 aus „%2“ geloescht.").arg(id).arg(current_));
    } else {
        emit statusMessage(QStringLiteral("Fehler: %1").arg(error ? *error : QString()));
    }
    return ok;
}

QString DataViewController::metricsText() const {
    if (!core_ || current_.isEmpty() || !core_->database().hasCollection(current_.toStdString()))
        return QStringLiteral("<i>Keine Collection ausgewaehlt.</i>");
    auto coll = core_->database().collection(current_.toStdString());
    const auto m = coll.metrics();
    const auto ov = coll.overflowInfo();
    QString html = QStringLiteral(
                       "<b>%1</b> – %2 Dokumente, %3 Indizes<br>"
                       "inserts %4 · updates %5 · removes %6 · queries %7 (Index: %8, Ø %9 ms)<br>"
                       "scanned %10 · Tx commit/rollback %11/%12 · spill R/W %13/%14")
                       .arg(current_)
                       .arg(m.docs)
                       .arg(m.indexCount)
                       .arg(m.inserts)
                       .arg(m.updates)
                       .arg(m.removes)
                       .arg(m.queries)
                       .arg(m.indexQueries)
                       .arg(m.lastQueryMs, 0, 'f', 3)
                       .arg(m.scannedDocs)
                       .arg(m.txCommits)
                       .arg(m.txRollbacks)
                       .arg(m.spillReads)
                       .arg(m.spillWrites);
    if (ov.enabled)
        html += QStringLiteral("<br><b>Hybrid:</b> im RAM %1, gespillt %2 (%3 Bytes)")
                    .arg(ov.inMemory)
                    .arg(ov.spilled)
                    .arg(ov.spillBytes);
    return html;
}

} // namespace memdb::viewer::detail
