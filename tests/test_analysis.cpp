// Tests fuer Schritt 4 (Datenanalyse): DiscoveryModel, Analyse-Models,
// AnalysisController (Worker-Thread: FA-27 Discovery, FA-29/30/31/32, v0.33 Health,
// v0.40 Ausreisser) und AnalysisWidget.
#include "test_viewer.hpp"

#include "../src/analysis_controller.hpp"
#include "../src/analysis_model.hpp"
#include "../src/discovery_model.hpp"

#include <memdb/memdb.hpp>
#include <memdbviewer/analysis_widget.hpp>
#include <memdbviewer/viewer_core.hpp>

#include <QSignalSpy>
#include <QTest>

using namespace memdb;
using namespace memdb::viewer;

static void fillAnalysisFixture(ViewerCore& core) {
    auto& db = core.database();
    auto users = db.createCollection("users");
    users.insert({{"id", 1}, {"name", "Ada"}, {"age", 36}});
    users.insert({{"id", 2}, {"name", "Bob"}, {"age", 17}});
    users.insert({{"id", 3}, {"name", "Cid"}, {"age", 44}});
    auto orders = db.createCollection("orders");
    orders.insert({{"order_id", 10}, {"user_id", 1}, {"total", 99.5}});
    orders.insert({{"order_id", 11}, {"user_id", 1}, {"total", 12.0}});
    orders.insert({{"order_id", 12}, {"user_id", 2}, {"total", 7.25}});
    orders.insert({{"order_id", 13}, {"user_id", 2}, {"total", 10000.0}}); // IQR-Ausreißer
}

void TestDiscoveryModel::relationsTable() {
    viewer::detail::DiscoveryModel model;
    std::vector<DiscoveredRelation> rels(2);
    rels[0].kind = RelationKind::ForeignKey;
    rels[0].fromCollection = "orders";
    rels[0].field = "user_id";
    rels[0].toCollection = "users";
    rels[0].toField = "id";
    rels[0].support = 1.0;
    rels[0].confidence = 0.9;
    rels[0].cardinality = "n:1";
    rels[0].sampleSize = 4;
    rels[1].kind = RelationKind::Correlation;
    rels[1].fromCollection = "users";
    rels[1].field = "age";
    rels[1].toCollection = "users";
    rels[1].toField = "id";
    model.setRelations(rels);
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.columnCount(), 8);
    QCOMPARE(model.data(model.index(0, viewer::detail::DiscoveryModel::KindColumn)).toString(), QStringLiteral("FK"));
    QCOMPARE(model.data(model.index(0, viewer::detail::DiscoveryModel::FromColumn)).toString(),
             QStringLiteral("orders.user_id"));
    QCOMPARE(model.data(model.index(0, viewer::detail::DiscoveryModel::ToColumn)).toString(),
             QStringLiteral("users.id"));
    QCOMPARE(model.data(model.index(0, viewer::detail::DiscoveryModel::CardinalityColumn)).toString(),
             QStringLiteral("n:1"));
    QCOMPARE(model.data(model.index(1, viewer::detail::DiscoveryModel::KindColumn)).toString(),
             QStringLiteral("Korrelation"));
}

void TestAnalysisModels::modelsPopulate() {
    viewer::detail::FieldStatsModel stats;
    NumericFieldAnalysis f;
    f.collection = "orders";
    f.field = "total";
    f.count = 4;
    f.mean = 2529.7;
    f.histogram = {1, 1, 1, 1};
    f.binMin = 0.0;
    f.binWidth = 2500.0;
    stats.setFields({f});
    QCOMPARE(stats.rowCount(), 1);
    QCOMPARE(stats.data(stats.index(0, viewer::detail::FieldStatsModel::FieldColumn)).toString(), QStringLiteral("total"));
    QVERIFY(stats.data(stats.index(0, viewer::detail::FieldStatsModel::MeanColumn)).toString().contains(QStringLiteral("2529")));
    QVERIFY(stats.fieldAt(0) != nullptr);
    QCOMPARE(stats.fieldAt(0)->histogram.size(), std::size_t(4));

    viewer::detail::InfluenceModel infl;
    infl.setRows({viewer::detail::InfluenceRow{QStringLiteral("orders"), QStringLiteral("total"), QStringLiteral("driver"), 0.9, 0.95, QStringLiteral("age"), QStringLiteral("hub")}});
    QCOMPARE(infl.rowCount(), 1);
    QCOMPARE(infl.data(infl.index(0, viewer::detail::InfluenceModel::RoleColumn)).toString(), QStringLiteral("driver"));

    viewer::detail::OutlierModel out;
    out.setRows({viewer::detail::OutlierRow{QStringLiteral("orders"), QStringLiteral("total"), 4, 1, 0.25, 2529.7, 4317.0, QStringLiteral("IQR"), QStringLiteral("10000")}});
    QCOMPARE(out.rowCount(), 1);
    QCOMPARE(out.data(out.index(0, viewer::detail::OutlierModel::ValuesColumn)).toString(), QStringLiteral("10000"));

    viewer::detail::CollectionOverviewModel ov;
    ov.setRows({viewer::detail::CollectionOverviewRow{QStringLiteral("orders"), 4, QString::fromStdString(memdb::toString(memdb::DataCategory::Numeric)), 0.9, QStringLiteral("viele Zahlen"), 1.0, QStringLiteral("total"), 1, 1}});
    QCOMPARE(ov.rowCount(), 1);
    QCOMPARE(ov.data(ov.index(0, viewer::detail::CollectionOverviewModel::CategoryColumn)).toString(),
             QString::fromStdString(memdb::toString(memdb::DataCategory::Numeric)));
}

void TestAnalysisController::discoversFkAndHealth() {
    ViewerCore core;
    fillAnalysisFixture(core);
    viewer::detail::DiscoveryModel disc;
    viewer::detail::TextListModel orphans, merges, keys, paths, profiles;
    viewer::detail::CollectionOverviewModel overview;
    viewer::detail::FieldStatsModel stats;
    viewer::detail::InfluenceModel infl;
    viewer::detail::OutlierModel outl;
    viewer::detail::AnalysisController controller(&core, &disc, &orphans, &merges, &keys, &paths, &profiles,
                                                  &overview, &stats, &infl, &outl);
    controller.discoveryOptions.minOutlierSamples = 4; // Fixture ist klein
    controller.numericOptions.minValuesPerField = 3;
    controller.influenceOptions.numeric.minValuesPerField = 3;

    QSignalSpy finSpy(&controller, &viewer::detail::AnalysisController::finished);
    QVERIFY(finSpy.isValid());
    controller.run(QString());
    QVERIFY(controller.isRunning() || finSpy.count() > 0);
    QTRY_VERIFY_WITH_TIMEOUT(finSpy.count() >= 1, 30000);
    QCOMPARE(finSpy.first().at(0).toBool(), true);

    // FK orders.user_id -> users.id entdeckt (FA-27)
    bool foundFk = false;
    for (const auto& r : disc.relations())
        if (r.kind == RelationKind::ForeignKey && r.fromCollection == "orders" && r.toCollection == "users")
            foundFk = true;
    QVERIFY(foundFk);
    QVERIFY(disc.rowCount() >= 1);

    // Health-Score + Reports
    QVERIFY(controller.healthScore() >= 0.0 && controller.healthScore() <= 100.0);
    QVERIFY(controller.reportMarkdown().contains(QStringLiteral("Health")) || controller.reportMarkdown().size() > 20);
    QVERIFY(controller.reportJson().contains(QLatin1Char('{')));
    QVERIFY(controller.reportMermaid().contains(QStringLiteral("graph")));

    // Uebersicht: 2 Collections; orders ist numerisch (FA-29, toString(DataCategory::Numeric) = "numeric")
    QCOMPARE(overview.rowCount(), 2);
    const QString numericName = QString::fromStdString(memdb::toString(memdb::DataCategory::Numeric));
    bool ordersNumeric = false;
    for (const auto& r : overview.rows())
        if (r.collection == QLatin1String("orders") && r.category == numericName)
            ordersNumeric = true;
    QVERIFY(ordersNumeric);

    // Numerische Feld-Statistik enthaelt total + age (FA-30)
    QVERIFY(stats.rowCount() >= 2);
    bool foundTotal = false;
    for (const auto& f : stats.fields())
        if (f.field == "total")
            foundTotal = true;
    QVERIFY(foundTotal);

    // Ausreisser: total=10000 via IQR (v0.40)
    QVERIFY(outl.rowCount() >= 1);
    bool foundOutlier = false;
    for (const auto& r : outl.rows())
        if (r.field == QLatin1String("total") && r.outliers >= 1)
            foundOutlier = true;
    QVERIFY(foundOutlier);

    // Einfluss (FA-31) hat Zeilen
    QVERIFY(infl.rowCount() >= 1);
    QVERIFY(!controller.isRunning());
}

void TestAnalysisController::scopeSingleCollection() {
    ViewerCore core;
    fillAnalysisFixture(core);
    viewer::detail::DiscoveryModel disc;
    viewer::detail::TextListModel orphans, merges, keys, paths, profiles;
    viewer::detail::CollectionOverviewModel overview;
    viewer::detail::FieldStatsModel stats;
    viewer::detail::InfluenceModel infl;
    viewer::detail::OutlierModel outl;
    viewer::detail::AnalysisController controller(&core, &disc, &orphans, &merges, &keys, &paths, &profiles,
                                                  &overview, &stats, &infl, &outl);
    QSignalSpy finSpy(&controller, &viewer::detail::AnalysisController::finished);
    controller.run(QStringLiteral("users")); // nur diese Collection
    QTRY_VERIFY_WITH_TIMEOUT(finSpy.count() >= 1, 30000);
    QCOMPARE(finSpy.first().at(0).toBool(), true);
    QCOMPARE(overview.rowCount(), 1);
    QCOMPARE(overview.rows().front().collection, QStringLiteral("users"));
}

void TestAnalysisWidget::runsAnalysisHeadless() {
    ViewerCore core;
    core.loadDemoData();
    AnalysisWidget widget;
    widget.setCore(&core);
    widget.show();
    QSignalSpy finSpy(&widget, &AnalysisWidget::analysisFinished);
    QVERIFY(finSpy.isValid());
    widget.runAnalysis();
    QTRY_VERIFY_WITH_TIMEOUT(finSpy.count() >= 1, 30000);
    QCOMPARE(finSpy.first().at(0).toBool(), true);
    QVERIFY(widget.healthScore() >= 0.0 && widget.healthScore() <= 100.0);
    QCOMPARE(widget.overviewRowCount(), 2); // users + orders
    QVERIFY(!widget.isRunning());
    // Scope umschaltbar
    widget.setScopeCollection(QStringLiteral("users"));
    QCOMPARE(widget.scopeCollection(), QStringLiteral("users"));
}

void TestAnalysisWidget::cancelDoesNotCrash() {
    ViewerCore core;
    fillAnalysisFixture(core);
    AnalysisWidget widget;
    widget.setCore(&core);
    QSignalSpy finSpy(&widget, &AnalysisWidget::analysisFinished);
    widget.runAnalysis();
    widget.cancelAnalysis();
    QTRY_VERIFY_WITH_TIMEOUT(finSpy.count() >= 1, 30000);
    QVERIFY(!widget.isRunning());
}
