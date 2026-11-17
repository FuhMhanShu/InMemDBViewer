#include "analysis_controller.hpp"

#include "analysis_model.hpp"
#include "confusion_model.hpp"
#include "discovery_model.hpp"

#include "memdbviewer/viewer_core.hpp"

#include <memdb/memdb.hpp>

#include <QMetaObject>

#include <functional>
#include <map>
#include <set>

namespace {
struct AnalysisCancelled {};
} // namespace

namespace memdb::viewer::detail {

AnalysisController::AnalysisController(ViewerCore* core, DiscoveryModel* discovery, TextListModel* orphans,
                                       TextListModel* merges, TextListModel* keys, TextListModel* paths,
                                       TextListModel* profiles, CollectionOverviewModel* overview,
                                       FieldStatsModel* fieldStats, InfluenceModel* influence,
                                       OutlierModel* outliers, ConfusionModel* confusion, QObject* parent)
    : QObject(parent), discovery_(discovery), orphans_(orphans), merges_(merges), keys_(keys), paths_(paths),
      profiles_(profiles), overview_(overview), fieldStats_(fieldStats), influence_(influence), outliers_(outliers),
      confusionModel_(confusion) {
    setCore(core);
}

AnalysisController::~AnalysisController() {
    cancel_.store(true);
    if (worker_.joinable())
        worker_.join();
}

void AnalysisController::setCore(ViewerCore* core) { core_ = core; }

void AnalysisController::deliver(std::function<void()> fn) {
    QMetaObject::invokeMethod(this, std::move(fn), Qt::QueuedConnection);
}

void AnalysisController::run(const QString& scopeCollection) {
    if (running_.load())
        return;
    cancel_.store(false);
    running_.store(true);
    emit started();
    emit progress(QStringLiteral("Starte Analyse …"));
    worker_ = std::thread([this, scopeCollection] { runWorker(scopeCollection); });
}

void AnalysisController::cancel() {
    cancel_.store(true);
    emit statusMessage(QStringLiteral("Analyse wird abgebrochen …"));
}

static QString relSummary(const std::vector<memdb::DiscoveredRelation>& rels) {
    std::map<memdb::RelationKind, int> byKind;
    for (const auto& r : rels)
        ++byKind[r.kind];
    QStringList parts;
    for (const auto& [k, n] : byKind) {
        QString kind = k == memdb::RelationKind::ForeignKey ? QStringLiteral("FK")
                       : k == memdb::RelationKind::FunctionalDependency ? QStringLiteral("FD")
                       : k == memdb::RelationKind::Correlation ? QStringLiteral("Korrelation")
                                                                 : QStringLiteral("Near-Dupe");
        parts << QStringLiteral("%1× %2").arg(n).arg(kind);
    }
    return parts.join(QStringLiteral(", "));
}

void AnalysisController::runWorker(const QString scopeCollection) {
    auto step = [this](const QString& msg) {
        if (cancel_.load())
            throw AnalysisCancelled{};
        deliver([this, msg] { emit progress(msg); });
    };

    try {
        if (!core_) {
            deliver([this] { emit finished(false, QStringLiteral("Kein ViewerCore gesetzt.")); });
            running_.store(false);
            return;
        }
        // Deep-Copy: Analyse laeuft thread-sicher, auch wenn der Core eine neue Database adoptiert
        memdb::Database copy = memdb::Database::fromJson(core_->database().toJson(), core_->database().name());
        std::vector<memdb::Collection> colls;
        if (!scopeCollection.isEmpty() && copy.hasCollection(scopeCollection.toStdString()))
            colls.push_back(copy.collection(scopeCollection.toStdString()));
        else
            for (const auto& n : copy.collectionNames())
                if (n.rfind("__", 0) != 0)
                    colls.push_back(copy.collection(n));

        // --- Discovery (FA-27) ---
        step(QStringLiteral("Discovery: Relationen …"));
        const auto rels = memdb::discoverRelations(colls, discoveryOptions);
        deliver([this, rels] { discovery_->setRelations(rels); });

        step(QStringLiteral("Discovery: Orphans/Pfade/Merges/Keys …"));
        const auto orphans = memdb::findOrphansForRelations(colls, rels, discoveryOptions);
        QStringList orphanEntries;
        for (const auto& o : orphans) {
            orphanEntries << QStringLiteral("%1.%2 -> %3.%4: %5/%6 Orphans")
                                 .arg(QString::fromStdString(o.fromCollection), QString::fromStdString(o.field),
                                      QString::fromStdString(o.toCollection), QString::fromStdString(o.toField))
                                 .arg(o.orphans)
                                 .arg(o.scanned);
        }
        deliver([this, orphanEntries] { orphans_->setEntries(orphanEntries); });

        QStringList pathEntries;
        {
            std::set<std::pair<std::string, std::string>> pairs;
            for (const auto& r : rels)
                if (r.toCollection != r.fromCollection)
                    pairs.insert({r.fromCollection, r.toCollection});
            std::size_t shown = 0;
            for (const auto& [from, to] : pairs) {
                if (shown++ >= 10)
                    break;
                for (const auto& path : memdb::findPaths(rels, from, to, 3)) {
                    QStringList hopNames;
                    for (const auto& hop : path)
                        hopNames << QString::fromStdString(hop.fromCollection + "." + hop.field + " -> " +
                                                           hop.toCollection + "." + hop.toField);
                    pathEntries << QStringLiteral("%1 -> %2: %3")
                                       .arg(QString::fromStdString(from), QString::fromStdString(to),
                                            hopNames.join(QStringLiteral("  ⇒  ")));
                }
            }
        }
        deliver([this, pathEntries] { paths_->setEntries(pathEntries); });

        const auto mergeSugs = memdb::suggestMerges(colls, discoveryOptions);
        QStringList mergeEntries;
        for (const auto& m : mergeSugs)
            mergeEntries << QStringLiteral("%1 <-> %2: Schema %3, Werte-Overlap %4 (Feld %5)")
                                .arg(QString::fromStdString(m.collectionA), QString::fromStdString(m.collectionB))
                                .arg(m.schemaSimilarity, 0, 'f', 2)
                                .arg(m.valueOverlap, 0, 'f', 2)
                                .arg(QString::fromStdString(m.commonField));
        deliver([this, mergeEntries] { merges_->setEntries(mergeEntries); });

        QStringList keyEntries;
        for (const auto& c : colls) {
            for (const auto& key : memdb::discoverCandidateKeys(c, discoveryOptions))
                keyEntries << QStringLiteral("%1: {%2}").arg(QString::fromStdString(c.name()),
                    [&] {
                        QStringList f;
                        for (const auto& k : key)
                            f << QString::fromStdString(k);
                        return f.join(QStringLiteral(", "));
                    }());
        }
        deliver([this, keyEntries] { keys_->setEntries(keyEntries); });

        QStringList profileEntries;
        for (const auto& c : colls) {
            const auto p = memdb::profileCollection(c, discoveryOptions);
            profileEntries << QStringLiteral("%1: %2 Dokumente, %3 Felder")
                                  .arg(QString::fromStdString(p.collection))
                                  .arg(p.docCount)
                                  .arg(p.fields.size());
            for (const auto& f : p.fields) {
                QStringList types;
                for (const auto& [t, n] : f.typeCounts)
                    types << QStringLiteral("%1×%2").arg(QString::fromStdString(memdb::typeName(t))).arg(n);
                profileEntries << QStringLiteral("    %1: %2, distinct %3, min %4, max %5")
                                      .arg(QString::fromStdString(f.name), types.join(QStringLiteral(", ")),
                                           QString::number(f.distinct),
                                           f.min ? QString::fromStdString(memdb::toString(*f.min)) : QStringLiteral("–"),
                                           f.max ? QString::fromStdString(memdb::toString(*f.max)) : QStringLiteral("–"));
            }
        }
        deliver([this, profileEntries] { profiles_->setEntries(profileEntries); });

        // --- Gesamt-Report + Health-Score (v0.33) ---
        step(QStringLiteral("Gesamt-Report + Health-Score …"));
        const memdb::AnalysisReport report = scopeCollection.isEmpty()
                                                 ? memdb::analyze(copy, discoveryOptions)
                                                 : memdb::analyze(colls, discoveryOptions);
        const QString md = QString::fromStdString(memdb::toMarkdown(report));
        const QString js = QString::fromStdString(memdb::toJson(report));
        const QString mm = QString::fromStdString(memdb::toMermaid(report));
        deliver([this, report, md, js, mm] {
            healthScore_ = report.healthScore;
            confusion_ = report.confusion; // v0.42: nur bei analyze(Database&) mit __references gefuellt
            if (confusion_)
                confusionModel_->setConfusion(confusion_);
            else
                confusionModel_->setConfusion(std::nullopt);
            reportMarkdown_ = md;
            reportJson_ = js;
            reportMermaid_ = mm;
        });

        // --- Charakter (FA-29) + numerisch (FA-30) + Einfluss (FA-31/32) + Ausreisser (v0.40) ---
        step(QStringLiteral("Datencharakter (FA-29) …"));
        const auto characters = scopeCollection.isEmpty() ? memdb::assessCharacter(copy, discoveryOptions)
                                                          : memdb::assessCharacter(colls, discoveryOptions);
        step(QStringLiteral("Numerische Analyse (FA-30) …"));
        const auto numeric = scopeCollection.isEmpty() ? memdb::analyzeNumeric(copy, numericOptions)
                                                       : memdb::analyzeNumeric(colls, numericOptions);
        std::vector<memdb::NumericFieldAnalysis> allFields;
        for (const auto& na : numeric)
            for (const auto& f : na.fields)
                allFields.push_back(f);
        deliver([this, allFields] { fieldStats_->setFields(allFields); });

        step(QStringLiteral("Einfluss-Analyse (FA-31/FA-32) …"));
        const auto influence = scopeCollection.isEmpty() ? memdb::analyzeInfluence(copy, influenceOptions)
                                                          : memdb::analyzeInfluence(colls, influenceOptions);
        std::vector<InfluenceRow> influenceRows;
        for (const auto& ia : influence) {
            for (const auto& f : ia.fields) {
                influenceRows.push_back(InfluenceRow{
                    QString::fromStdString(ia.collection), QString::fromStdString(f.field),
                    QString::fromStdString(memdb::toString(f.role)), f.influenceScore, f.maxAbsPearson,
                    QString::fromStdString(f.strongestPartner), QString::fromStdString(f.rationale)});
            }
        }
        memdb::CrossInfluenceResult cross;
        if (scopeCollection.isEmpty()) {
            cross = memdb::analyzeCrossInfluence(copy, crossInfluenceOptions);
            for (const auto& pair : cross.pairs) {
                const QString scope = QStringLiteral("%1 -> %2 (%3, %4)")
                                          .arg(QString::fromStdString(pair.collectionA),
                                               QString::fromStdString(pair.collectionB), QString::fromStdString(pair.origin),
                                               QString::fromStdString(pair.relation));
                for (const auto& f : pair.fields) {
                    influenceRows.push_back(InfluenceRow{scope, QString::fromStdString(f.field),
                                                         QString::fromStdString(memdb::toString(f.role)),
                                                         f.influenceScore, f.maxAbsPearson,
                                                         QString::fromStdString(f.strongestPartner),
                                                         QString::fromStdString(f.rationale)});
                }
            }
        }
        deliver([this, influenceRows] { influence_->setRows(influenceRows); });

        step(QStringLiteral("Ausreißer (v0.40) …"));
        const auto outliers = memdb::discoverOutliers(colls, discoveryOptions);
        std::vector<OutlierRow> outlierRows;
        for (const auto& o : outliers) {
            QStringList vals;
            for (const auto& v : o.values)
                vals << QString::fromStdString(memdb::toString(v));
            outlierRows.push_back(OutlierRow{QString::fromStdString(o.collection), QString::fromStdString(o.field),
                                             o.count, o.outliers, o.outlierRate, o.mean, o.stddev,
                                             QString::fromStdString(o.method), vals.join(QStringLiteral(", "))});
        }
        deliver([this, outlierRows] { outliers_->setRows(outlierRows); });

        // --- Uebersicht pro Collection zusammensetzen ---
        step(QStringLiteral("Übersicht …"));
        std::vector<CollectionOverviewRow> overviewRows;
        for (const auto& c : colls) {
            const std::string name = c.name();
            CollectionOverviewRow row;
            row.collection = QString::fromStdString(name);
            row.docs = c.size();
            for (const auto& ch : characters)
                if (ch.collection == name) {
                    row.category = QString::fromStdString(memdb::toString(ch.category));
                    row.categoryConfidence = ch.confidence;
                    row.rationale = QString::fromStdString(ch.rationale);
                    row.numericRatio = ch.numericFieldRatio;
                }
            for (const auto& na : numeric)
                if (na.collection == name)
                    row.numericRatio = na.numericFieldRatio;
            for (const auto& ia : influence)
                if (ia.collection == name)
                    row.topDriver = QString::fromStdString(ia.topDriver);
            std::size_t fields = 0, total = 0;
            for (const auto& o : outliers)
                if (o.collection == name) {
                    ++fields;
                    total += o.outliers;
                }
            row.outlierFields = fields;
            row.totalOutliers = total;
            overviewRows.push_back(std::move(row));
        }
        deliver([this, overviewRows] { overview_->setRows(overviewRows); });

        const QString summary = cancel_.load()
            ? QStringLiteral("abgebrochen")
            : QStringLiteral("Health-Score %1/100 · %2 · %3 Collections, %4 Dokumente")
                  .arg(report.healthScore, 0, 'f', 1)
                  .arg(relSummary(rels))
                  .arg(report.totalCollections)
                  .arg(report.totalDocs);
        deliver([this, summary, cancelled = cancel_.load()] {
            emit finished(!cancelled, summary);
        });
    } catch (const AnalysisCancelled&) {
        deliver([this] { emit finished(false, QStringLiteral("abgebrochen")); });
    } catch (const std::exception& e) {
        const QString msg = QString::fromUtf8(e.what());
        deliver([this, msg] {
            emit statusMessage(QStringLiteral("Analyse-Fehler: %1").arg(msg));
            emit finished(false, msg);
        });
    }
    running_.store(false);
}

} // namespace memdb::viewer::detail
