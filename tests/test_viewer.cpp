#include "test_viewer.hpp"

#include <memdb/memdb.hpp>
#include <memdbviewer/viewer.hpp>

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

using namespace memdb;

void TestDatabaseModel::overview() {
    Database db("t");
    Collection users = db.createCollection("users");
    Schema s;
    s.add("name", Value::Type::String).notNull();
    s.add("age", Value::Type::Int);
    users.setSchema(s);
    users.createIndex("age", true);
    users.insert({{"name", "Ada"}, {"age", 36}});
    users.insert({{"name", "Bob"}, {"age", 17}});
    db.createCollection("orders");

    viewer::DatabaseModel model;
    model.setDatabase(&db);

    QCOMPARE(model.rowCount(), 2); // collectionNames() ist sortiert: orders, users
    QCOMPARE(model.columnCount(), 2);

    const QModelIndex orders = model.index(0, 0);
    QCOMPARE(orders.data().toString(), QStringLiteral("orders"));
    QVERIFY(orders.sibling(0, 1).data().toString().contains(QStringLiteral("Schema: nein")));
    QCOMPARE(model.rowCount(orders), 0); // schema-los, keine Indizes -> keine Kind-Zeilen

    const QModelIndex users = model.index(1, 0);
    QCOMPARE(users.data().toString(), QStringLiteral("users"));
    const QString details = users.sibling(1, 1).data().toString();
    QVERIFY(details.contains(QStringLiteral("2 Dokument(e)")));
    QVERIFY(details.contains(QStringLiteral("Schema: ja")));
    QVERIFY(details.contains(QStringLiteral("1 Index")));

    QCOMPARE(model.rowCount(users), 2); // Felder age, name (std::map: sortiert)
    const QModelIndex age = model.index(0, 0, users);
    QCOMPARE(age.data().toString(), QStringLiteral("age"));
    QVERIFY(age.sibling(0, 1).data().toString().contains(QStringLiteral("int")));
    QVERIFY(age.sibling(0, 1).data().toString().contains(QStringLiteral("unique index")));
    const QModelIndex name = model.index(1, 0, users);
    QCOMPARE(name.data().toString(), QStringLiteral("name"));
    QVERIFY(name.sibling(1, 1).data().toString().contains(QStringLiteral("string")));
    QVERIFY(name.sibling(1, 1).data().toString().contains(QStringLiteral("notNull")));
}

void TestDatabaseModel::refreshAfterInsert() {
    Database db("t");
    Collection orders = db.createCollection("orders");

    viewer::DatabaseModel model;
    model.setDatabase(&db);
    QVERIFY(model.index(0, 0).sibling(0, 1).data().toString().contains(QStringLiteral("0 Dokument(e)")));

    orders.insert({{"total", 5}});
    model.refresh();
    QVERIFY(model.index(0, 0).sibling(0, 1).data().toString().contains(QStringLiteral("1 Dokument(e)")));
}

void TestSchemaScene::nodesAndEdges() {
    Database db("t");
    Collection users = db.createCollection("users");
    const DocumentId u1 = users.insert({{"name", "Ada"}});
    Collection orders = db.createCollection("orders");
    const DocumentId o1 = orders.insert({{"total", 5}});
    ReferenceIndex refs(db);
    refs.link("orders", o1, "users", u1, "kunde");

    viewer::SchemaScene scene;
    scene.setDatabase(db, &refs);

    QCOMPARE(scene.collectionItemCount(), 2); // __references ist kein Knoten
    QCOMPARE(scene.edgeItemCount(), 1);       // eine Referenz-Kante
    for (const viewer::CollectionItem* item : scene.collectionItems())
        QVERIFY(item->title() != QStringLiteral("__references"));
    QVERIFY(scene.itemForCollection(QStringLiteral("users")) != nullptr);
    QVERIFY(scene.itemForCollection(QStringLiteral("unbekannt")) == nullptr);
}

void TestSchemaScene::schemaFieldsRendered() {
    Database db("t");
    Collection users = db.createCollection("users");
    Schema s;
    s.add("name", Value::Type::String).notNull();
    s.add("age", Value::Type::Int);
    users.setSchema(s);
    users.createIndex("age", true);
    users.insert({{"name", "Ada"}, {"age", 36}});

    viewer::SchemaScene scene;
    scene.setDatabase(db);
    QCOMPARE(scene.collectionItemCount(), 1);

    const viewer::CollectionItem* item = scene.collectionItems().front();
    QCOMPARE(item->title(), QStringLiteral("users"));
    QCOMPARE(item->fields().size(), static_cast<std::size_t>(2));

    bool foundAge = false;
    for (const auto& f : item->fields()) {
        if (f.name == QStringLiteral("age")) {
            foundAge = true;
            QCOMPARE(f.type, QStringLiteral("int"));
            QVERIFY(f.flags.contains(QStringLiteral("unique")));
        }
        if (f.name == QStringLiteral("name")) {
            QCOMPARE(f.type, QStringLiteral("string"));
            QVERIFY(f.flags.contains(QStringLiteral("notNull")));
        }
    }
    QVERIFY(foundAge);
}

void TestSchemaScene::nestedSchemaInScene() {
    Database db("t");
    Collection users = db.createCollection("users");
    Schema sub;
    sub.add("plz", Value::Type::String);
    Schema s;
    s.add("name", Value::Type::String).notNull();
    s.add("adresse", Value::Type::Document).subSchema(sub);
    users.setSchema(s);
    users.insert({{"name", "Ada"},
                  {"adresse", Value(std::make_shared<Document>(Document{{"plz", "10115"}}))}});

    viewer::SchemaScene scene;
    scene.setDatabase(db);
    QCOMPARE(scene.collectionItemCount(), 1);

    // FA-06: Unter-Schema erscheint als eigene, eingerueckte Zeile (adresse.plz, depth 1)
    auto* item = scene.collectionItems().front();
    QCOMPARE(item->fields().size(), static_cast<std::size_t>(3)); // adresse, adresse.plz, name
    bool foundPlz = false;
    for (const auto& f : item->fields())
        if (f.path == QStringLiteral("adresse.plz")) {
            foundPlz = true;
            QCOMPARE(f.depth, 1);
            QCOMPARE(f.type, QStringLiteral("string"));
        }
    QVERIFY(foundPlz);

    // Collapse von "adresse" (erste Zeile) versteckt die Unter-Zeile
    QCOMPARE(item->visibleRowCount(), 3);
    const QPointF row0(viewer::CollectionItem::kPadding,
                       viewer::CollectionItem::kHeaderHeight + viewer::CollectionItem::kPadding / 2 + 1);
    QCOMPARE(item->rowAt(row0), 0);
    item->toggleCollapseAt(row0);
    QCOMPARE(item->visibleRowCount(), 2);
    QVERIFY(item->isCollapsed(QStringLiteral("adresse")));
}

void TestCollectionItem::collapseExpand() {
    viewer::CollectionItem item(QStringLiteral("users"), QStringLiteral("1 Dokument(e)"), {
        {QStringLiteral("adresse"), QStringLiteral("adresse"), QStringLiteral("document"), QString(), 0},
        {QStringLiteral("plz"), QStringLiteral("adresse.plz"), QStringLiteral("string"), QString(), 1},
        {QStringLiteral("name"), QStringLiteral("name"), QStringLiteral("string"), QStringLiteral("notNull"), 0},
    });
    QCOMPARE(item.visibleRowCount(), 3);
    const QPointF firstRow(viewer::CollectionItem::kPadding,
                           viewer::CollectionItem::kHeaderHeight + viewer::CollectionItem::kPadding / 2 + 1);
    QCOMPARE(item.rowAt(firstRow), 0);
    item.toggleCollapseAt(firstRow);
    QVERIFY(item.isCollapsed(QStringLiteral("adresse")));
    QCOMPARE(item.visibleRowCount(), 2);
    item.toggleCollapseAt(firstRow);
    QVERIFY(!item.isCollapsed(QStringLiteral("adresse")));
    QCOMPARE(item.visibleRowCount(), 3);

    // Zeile ohne Kinder (name, dritte Zeile) reagiert nicht auf Toggle
    const QPointF thirdRow(viewer::CollectionItem::kPadding,
                           viewer::CollectionItem::kHeaderHeight + viewer::CollectionItem::kPadding / 2 +
                               2 * viewer::CollectionItem::kRowHeight + 1);
    QCOMPARE(item.rowAt(thirdRow), 2);
    item.toggleCollapseAt(thirdRow);
    QCOMPARE(item.visibleRowCount(), 3);
    QVERIFY(item.rowAt(QPointF(-5, 100)) == -1);
}

void TestViewerController::observerTriggersSignal() {
    Database db("t");
    Collection users = db.createCollection("users");

    viewer::ViewerController controller;
    QSignalSpy dbSpy(&controller, &viewer::ViewerController::databaseChanged);
    QSignalSpy collSpy(&controller, &viewer::ViewerController::collectionChanged);

    controller.setDatabase(db);
    QCOMPARE(dbSpy.count(), 1);
    QVERIFY(controller.databaseModel() != nullptr);
    QCOMPARE(controller.databaseModel()->rowCount(), 1);

    users.insert({{"name", "Ada"}}); // Observer-Event -> queued -> GUI-Thread
    QVERIFY(collSpy.wait(1000));
    QCOMPARE(collSpy.count(), 1);
    QCOMPARE(collSpy.takeFirst().at(0).toString(), QStringLiteral("users"));
}

void TestViewerController::modelFollowsObserver() {
    Database db("t");
    Collection users = db.createCollection("users");

    viewer::ViewerController controller;
    controller.setDatabase(db);
    QSignalSpy collSpy(&controller, &viewer::ViewerController::collectionChanged);

    users.insert({{"a", 1}});
    QVERIFY(collSpy.wait(1000));
    // collectionChanged -> DatabaseModel::refresh (direkte Verbindung im GUI-Thread)
    QVERIFY(controller.databaseModel()->index(0, 0).sibling(0, 1).data().toString().contains(
        QStringLiteral("1 Dokument(e)")));
}

void TestViewerController::selectCollection() {
    Database db("t");
    db.createCollection("users");
    db.createCollection("orders");

    viewer::ViewerController controller;
    controller.setDatabase(db);
    QSignalSpy spy(&controller, &viewer::ViewerController::collectionSelected);

    controller.selectCollection(QStringLiteral("orders"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toString(), QStringLiteral("orders"));

    controller.selectCollection(QStringLiteral("unbekannt")); // existiert nicht -> kein Signal
    QCOMPARE(spy.count(), 0);
}

void TestMetricsPanel::showsCollectionMetrics() {
    Database db("t");
    Collection users = db.createCollection("users");
    users.createIndex("age");
    users.insert({{"age", 36}});
    users.insert({{"age", 17}});
    users.count("age >= 18");

    viewer::MetricsPanel panel;
    panel.showCollection(users);
    const QString text = panel.bodyText();
    QVERIFY(text.contains(QStringLiteral("docs: 2")));
    QVERIFY(text.contains(QStringLiteral("inserts: 2")));
    QVERIFY(text.contains(QStringLiteral("queries: 1")));
    QVERIFY(text.contains(QStringLiteral("indexCount: 1")));

    panel.refresh(); // Abdeckung des Slots
    QVERIFY(panel.bodyText().contains(QStringLiteral("Collection \"users\"")));
}

void TestMetricsPanel::showsOverflowInfo() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Database db("t");
    Collection c = db.createCollection("c");
    c.setOverflow((dir.path() + QStringLiteral("/spill.log")).toStdString(), 1);
    c.insert({{"n", 1}});
    c.insert({{"n", 2}}); // aeltestes Dokument wird ausgespillt
    QVERIFY(c.overflowInfo().enabled);

    viewer::MetricsPanel panel;
    panel.showCollection(c);
    QVERIFY(panel.bodyText().contains(QStringLiteral("OVERFLOW aktiv")));
    QVERIFY(panel.bodyText().contains(QStringLiteral("spilled=1")));

    // Database-Modus (aggregierte Metriken)
    panel.showDatabase(db);
    QVERIFY(panel.bodyText().contains(QStringLiteral("Collections: 1")));
}

void TestSnapshotExport::openSnapshotRoundtrip() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.path() + QStringLiteral("/snap.json");
    {
        Database db("snap");
        db.createCollection("a").insert({{"x", 1}});
        db.createCollection("b");
        db.saveToFile(path.toStdString());
    }

    viewer::ViewerController controller;
    QVERIFY(controller.openSnapshot(path)); // FA-12
    QVERIFY(controller.database() != nullptr);
    QCOMPARE(controller.databaseModel()->rowCount(), 2);
    QCOMPARE(controller.lastError(), QString());

    // Datei existiert nicht -> false + lastError
    QVERIFY(!controller.openSnapshot(dir.path() + QStringLiteral("/fehlt.json")));
    QVERIFY(!controller.lastError().isEmpty());
}

void TestSnapshotExport::openSnapshotEncrypted() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.path() + QStringLiteral("/snap.enc");
    {
        Database db("snap");
        db.createCollection("a").insert({{"x", 1}});
        db.saveToFileEncrypted(path.toStdString(), "geheim");
    }

    viewer::ViewerController controller;
    QVERIFY(controller.openSnapshotEncrypted(path, QStringLiteral("geheim")));
    QCOMPARE(controller.databaseModel()->rowCount(), 1);
}

void TestSnapshotExport::wrongPasswordFails() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.path() + QStringLiteral("/snap.enc");
    {
        Database db("snap");
        db.createCollection("a");
        db.saveToFileEncrypted(path.toStdString(), "geheim");
    }

    viewer::ViewerController controller;
    QVERIFY(!controller.openSnapshotEncrypted(path, QStringLiteral("falsch")));
    QVERIFY(!controller.lastError().isEmpty());
    QVERIFY(controller.database() == nullptr); // vorherige Database bleibt unberuehrt
}

void TestSnapshotExport::exportPngAndMermaid() {
    Database db("t");
    Collection users = db.createCollection("users");
    const DocumentId u = users.insert({{"name", "Ada"}});
    Collection orders = db.createCollection("orders");
    const DocumentId o = orders.insert({{"total", 1}});
    ReferenceIndex refs(db);
    refs.link("orders", o, "users", u, "kunde");

    viewer::SchemaScene scene;
    scene.setDatabase(db, &refs);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString png = dir.path() + QStringLiteral("/scene.png");
    QVERIFY(scene.exportPng(png)); // FA-15
    QFile f(png);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QVERIFY(f.size() > 0);

    // FA-15: Mermaid-Export ueber den DbGraph
    const QString mmd = scene.mermaid();
    QVERIFY(mmd.contains(QStringLiteral("graph")));
    QVERIFY(mmd.contains(QStringLiteral("kunde")));
    QCOMPARE(scene.graph().nodes.size(), static_cast<std::size_t>(2));
    QCOMPARE(scene.graph().edges.size(), static_cast<std::size_t>(1));
}
