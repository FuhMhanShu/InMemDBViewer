// Tests fuer Schritt 3 (Datenbetrachtung): DatabaseModel, DocumentTableModel,
// DocumentTreeModel, GraphScene (QGraphicsFramework/DbGraph FA-22) und DataViewWidget.
#include "test_viewer.hpp"

#include "../src/data_view_controller.hpp"
#include "../src/database_model.hpp"
#include "../src/document_table_model.hpp"
#include "../src/document_tree_model.hpp"
#include "../src/graph_scene.hpp"

#include <memdb/memdb.hpp>
#include <memdbviewer/data_view_widget.hpp>
#include <memdbviewer/viewer_core.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#ifndef TESTS_DATA_DIR
#define TESTS_DATA_DIR "data"
#endif

static QString dataFile(const QString& name) {
    return QStringLiteral(TESTS_DATA_DIR) + QLatin1Char('/') + name;
}

using namespace memdb;
using namespace memdb::viewer;

static Database demoDb() {
    Database db("demo");
    auto users = db.createCollection("users");
    Schema sub;
    sub.add("plz", Value::Type::String);
    Schema schema;
    schema.add("name", Value::Type::String).notNull();
    schema.add("age", Value::Type::Int);
    schema.add("adresse", Value::Type::Document).subSchema(sub);
    users.setSchema(schema);
    users.createIndex("age", true);
    users.insert({{"name", "Ada"},
                  {"age", 36},
                  {"adresse", Value(std::make_shared<Document>(Document{{"plz", "10115"}}))}});
    users.insert({{"name", "Bob"}, {"age", 17}});
    auto orders = db.createCollection("orders");
    orders.createIndex("total");
    orders.insert({{"total", 99.5}, {"status", "bezahlt"}});
    orders.insert({{"total", 12.0}, {"status", "offen"}});
    return db;
}

void TestDatabaseModel::overviewAndRefresh() {
    ViewerCore core;
    core.loadDemoData();
    viewer::detail::DatabaseModel model;
    model.setDatabase(&core.database());
    QCOMPARE(model.rowCount(), 2); // users, orders ("__references" wird ausgeblendet)
    QCOMPARE(model.columnCount(), 4);
    QCOMPARE(model.headerData(0, Qt::Horizontal).toString(), QStringLiteral("Collection"));
    QCOMPARE(model.data(model.index(0, 0)).toString(), QStringLiteral("orders"));
    QCOMPARE(model.data(model.index(0, 1)).toULongLong(), 3ull); // 3 Demo-Bestellungen
    QCOMPARE(model.data(model.index(1, 2)).toString(), QStringLiteral("ja"));  // users hat Schema
    QCOMPARE(model.data(model.index(1, 3)).toInt(), 1);                        // users: 1 Index
    QVERIFY(model.indexOf(QStringLiteral("users")).isValid());
    QVERIFY(!model.indexOf(QStringLiteral("__references")).isValid());

    // Refresh nach Aenderung
    core.database().collection("orders").insert({{"total", 1.0}, {"status", "neu"}});
    model.refresh();
    QCOMPARE(model.data(model.index(0, 1)).toULongLong(), 4ull);
}

void TestDocumentTableModel::rowsColumnsAndValues() {
    auto db = demoDb();
    viewer::detail::DocumentTableModel model;
    model.setCollection(db.collection("users"));
    QCOMPARE(model.rowCount(), 2);
    QVERIFY(model.columnCount() >= 4); // id, adresse.plz, age, name
    // Werte ueber Zeilen hinweg finden
    bool foundAda = false;
    for (int r = 0; r < model.rowCount(); ++r) {
        const quint64 id = model.idAt(r);
        QVERIFY(id != 0);
        for (int c = 0; c < model.columnCount(); ++c) {
            if (model.headerData(c, Qt::Horizontal).toString() == QLatin1String("name") &&
                model.data(model.index(r, c)).toString() == QLatin1String("Ada")) {
                foundAda = true;
                const int ageCol = model.headerData(0, Qt::Horizontal).toString().isEmpty() ? -1 : [&] {
                    for (int cc = 0; cc < model.columnCount(); ++cc)
                        if (model.headerData(cc, Qt::Horizontal).toString() == QLatin1String("age"))
                            return cc;
                    return -1;
                }();
                QCOMPARE(model.data(model.index(r, ageCol)).toString(), QStringLiteral("36"));
                QCOMPARE(model.data(model.index(r, ageCol), viewer::detail::DocumentTableModel::TypeRole).toString(),
                         QStringLiteral("int"));
            }
        }
    }
    QVERIFY(foundAda);
    // id-Spalte
    QCOMPARE(model.data(model.index(0, 0)).toULongLong(), model.idAt(0));
}

void TestDocumentTableModel::queryFilterAndIndexDiagnostics() {
    auto db = demoDb();
    viewer::detail::DocumentTableModel model;
    model.setCollection(db.collection("users"));
    QCOMPARE(model.rowCount(), 2);
    QString err;
    QVERIFY(model.applyQuery(QStringLiteral("age >= 18"), &err));
    QCOMPARE(model.rowCount(), 1);
    QVERIFY(model.lastQueryUsedIndex()); // Index auf age vorhanden
    QVERIFY(model.lastQueryCandidateCount() < 2);
    QVERIFY(!model.query().isEmpty());
    model.clearQuery();
    QCOMPARE(model.rowCount(), 2);
    // ohne Index: Vollscan-Diagnose
    viewer::detail::DocumentTableModel model2;
    model2.setCollection(db.collection("orders"));
    QVERIFY(model2.applyQuery(QStringLiteral("status = 'offen'"), &err));
    QCOMPARE(model2.rowCount(), 1);
    QVERIFY(!model2.lastQueryUsedIndex()); // kein Index auf status
}

void TestDocumentTableModel::invalidQueryReportsError() {
    auto db = demoDb();
    viewer::detail::DocumentTableModel model;
    model.setCollection(db.collection("users"));
    QString err;
    QVERIFY(!model.applyQuery(QStringLiteral("("), &err));
    QVERIFY(!err.isEmpty());
    QCOMPARE(model.rowCount(), 2); // unveraendert
}

void TestDocumentTreeModel::nestedDocumentTree() {
    auto db = demoDb();
    auto doc = db.collection("users").findOne("name = 'Ada'");
    QVERIFY(doc.has_value());
    viewer::detail::DocumentTreeModel model;
    model.setDocument(*doc);
    QCOMPARE(model.columnCount(), 3);
    QCOMPARE(model.rowCount(), 3); // name, age, adresse
    // adresse aufklappen
    QModelIndex adresse;
    for (int r = 0; r < model.rowCount(); ++r) {
        if (model.data(model.index(r, 0)).toString() == QLatin1String("adresse"))
            adresse = model.index(r, 0);
    }
    QVERIFY(adresse.isValid());
    QCOMPARE(model.data(model.index(adresse.row(), viewer::detail::DocumentTreeModel::TypeColumn)).toString(), QStringLiteral("document"));
    QCOMPARE(model.rowCount(adresse), 1); // plz
    const QModelIndex plz = model.index(0, 0, adresse);
    QCOMPARE(model.data(plz).toString(), QStringLiteral("plz"));
    QCOMPARE(model.data(model.index(0, 2, adresse)).toString(), QStringLiteral("10115"));
    QVERIFY(model.parent(plz) == adresse);
    model.clearDocument();
    QCOMPARE(model.rowCount(), 0);
}

void TestGraphScene::nodesAndEdgesMatchDbGraph() {
    auto db = demoDb();
    ReferenceIndex refs(db);
    const DocumentId ada = db.collection("users").findOne("name = 'Ada'")->id();
    const DocumentId o1 = db.collection("orders").findOne("total = 99.5")->id();
    refs.link("orders", o1, "users", ada, "kunde");

    viewer::detail::GraphScene scene;
    scene.rebuild(db, &refs);
    const DbGraph g = DbGraph::fromDatabase(db, &refs);
    QCOMPARE(scene.nodeCount(), static_cast<int>(g.nodes.size()));
    QCOMPARE(scene.edgeCount(), static_cast<int>(g.edges.size()));
    QVERIFY(scene.edgeCount() >= 1);
    auto* usersItem = scene.nodeItem(QStringLiteral("users"));
    QVERIFY(usersItem != nullptr);
    QVERIFY(usersItem->boundingRect().height() > 46); // Schema-Felder gerendert
    QVERIFY(scene.mermaid().contains(QStringLiteral("graph")));
    QVERIFY(scene.graphSummary().contains(QStringLiteral("Collections")));
}

void TestGraphScene::clickSignal() {
    auto db = demoDb();
    viewer::detail::GraphScene scene;
    scene.rebuild(db, nullptr);
    QSignalSpy spy(&scene, &viewer::detail::GraphScene::collectionClicked);
    auto* item = scene.nodeItem(QStringLiteral("users"));
    QVERIFY(item != nullptr);
    QGraphicsSceneMouseEvent ev(QEvent::GraphicsSceneMousePress);
    ev.setButton(Qt::LeftButton);
    item->mousePressEvent(&ev);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toString(), QStringLiteral("users"));
}

void TestGraphScene::exportPngAndMermaid() {
    auto db = demoDb();
    viewer::detail::GraphScene scene;
    scene.rebuild(db, nullptr);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString png = dir.filePath(QStringLiteral("scene.png"));
    QVERIFY(scene.exportPng(png));
    QFile f(png);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QVERIFY(f.size() > 100);
    QVERIFY(scene.mermaid().contains(QStringLiteral("users")));
}

void TestDataViewWidget::loadsAndSelectsCollection() {
    ViewerCore core;
    core.loadDemoData();
    DataViewWidget widget;
    widget.setCore(&core);
    widget.show();
    QTRY_VERIFY_WITH_TIMEOUT(!widget.currentCollection().isEmpty(), 3000);
    QVERIFY(core.collectionNames().contains(widget.currentCollection()));
    QCOMPARE(widget.graphNodeCount(), 2);
    QVERIFY(widget.graphEdgeCount() >= 1); // Demo-Referenz orders->users
    QVERIFY(widget.tableRowCount() > 0);
    QVERIFY(widget.tableColumnCount() >= 2);
}

void TestDataViewWidget::liveRefreshOnInsert() {
    ViewerCore core;
    core.loadDemoData();
    DataViewWidget widget;
    widget.setCore(&core);
    widget.show();
    QTRY_VERIFY_WITH_TIMEOUT(widget.currentCollection() == QStringLiteral("orders"), 3000);
    const int before = widget.tableRowCount();
    core.database().collection("orders").insert({{"total", 5.0}, {"status", "neu"}});
    QTRY_VERIFY_WITH_TIMEOUT(widget.tableRowCount() == before + 1, 3000);
}

void TestDataViewWidget::applyQueryViaWidget() {
    ViewerCore core;
    core.loadDemoData();
    DataViewWidget widget;
    widget.setCore(&core);
    widget.show();
    QTRY_VERIFY_WITH_TIMEOUT(widget.currentCollection() == QStringLiteral("orders"), 3000);
    QCOMPARE(widget.tableRowCount(), 3);
    widget.applyQuery(QStringLiteral("total > 50"));
    QTRY_VERIFY_WITH_TIMEOUT(widget.tableRowCount() == 1, 3000);
    QVERIFY(widget.queryDiagnostics().contains(QStringLiteral("Index: ja"))); // Index auf total
    widget.resetQuery();
    QTRY_VERIFY_WITH_TIMEOUT(widget.tableRowCount() == 3, 3000);
}
