#include "test_viewer.hpp"

#include <memdb/memdb.hpp>
#include <memdb/qt/memdb_qt_model.hpp>
#include <memdbviewer/viewer.hpp>

#include <QFile>
#include <QSignalSpy>
#include <QStandardItemModel>
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

    const QModelIndex usersIdx = model.index(1, 0);
    QCOMPARE(usersIdx.data().toString(), QStringLiteral("users"));
    const QString details = usersIdx.sibling(1, 1).data().toString();
    QVERIFY(details.contains(QStringLiteral("2 Dokument(e)")));
    QVERIFY(details.contains(QStringLiteral("Schema: ja")));
    QVERIFY(details.contains(QStringLiteral("1 Index")));

    QCOMPARE(model.rowCount(usersIdx), 2); // Felder age, name (std::map: sortiert)
    const QModelIndex age = model.index(0, 0, usersIdx);
    QCOMPARE(age.data().toString(), QStringLiteral("age"));
    QVERIFY(age.sibling(0, 1).data().toString().contains(QStringLiteral("int")));
    QVERIFY(age.sibling(0, 1).data().toString().contains(QStringLiteral("unique index")));
    const QModelIndex name = model.index(1, 0, usersIdx);
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

void TestSnapshotExport::exportSvgJsonMarkdown() {
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
    const QString svg = dir.path() + QStringLiteral("/scene.svg");
    QVERIFY(scene.exportSvg(svg)); // FA-15: SVG-Export (QtSvg)
    QFile f(svg);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QString svgText = QString::fromUtf8(f.readAll());
    QVERIFY(svgText.contains(QStringLiteral("<svg")));

    // FA-15: JSON-/Markdown-Export ueber den DbGraph
    const QString js = scene.json();
    QVERIFY(js.contains(QStringLiteral("\"references\"")));
    QVERIFY(js.contains(QStringLiteral("kunde")));
    const QString md = scene.markdown();
    QVERIFY(md.contains(QStringLiteral("# Datenbank-Graph")));
    QVERIFY(md.contains(QStringLiteral("kunde")));
}

void TestDocumentTreeView::nestedDocument() {
    Document doc({{"name", "Ada"},
                  {"age", 36},
                  {"adresse", Value(std::make_shared<Document>(Document{{"plz", "10115"}}))},
                  {"tags", Value(std::vector<Value>{Value("a"), Value("b")})}});

    viewer::DocumentTreeView view;
    view.showDocument(doc); // FA-07
    auto* model = qobject_cast<QStandardItemModel*>(view.model());
    QVERIFY(model != nullptr);
    QCOMPARE(model->rowCount(), 1); // Dokument-Wurzel
    const QModelIndex root = model->index(0, 0);
    QVERIFY(root.data().toString().contains(QStringLiteral("Dokument #")));
    QCOMPARE(model->rowCount(root), 4); // name, age, adresse, tags

    QModelIndex adresse, tags;
    for (int i = 0; i < model->rowCount(root); ++i) {
        const QModelIndex idx = model->index(i, 0, root);
        if (idx.data().toString() == QStringLiteral("adresse"))
            adresse = idx;
        if (idx.data().toString() == QStringLiteral("tags"))
            tags = idx;
    }
    QVERIFY(adresse.isValid());
    QVERIFY(tags.isValid());
    QCOMPARE(model->rowCount(adresse), 1); // Unter-Dokument: plz
    QCOMPARE(model->index(0, 0, adresse).data().toString(), QStringLiteral("plz"));
    QCOMPARE(model->index(0, 1, adresse).data().toString(), QStringLiteral("string"));
    QCOMPARE(model->index(0, 2, adresse).data().toString(), QStringLiteral("10115"));
    QCOMPARE(model->rowCount(tags), 2); // Array: [0], [1]
    QCOMPARE(model->index(0, 0, tags).data().toString(), QStringLiteral("[0]"));
    QCOMPARE(model->index(0, 2, tags).data().toString(), QStringLiteral("a"));

    view.clearDocument();
    QCOMPARE(model->rowCount(), 0);
}

void TestDocumentView::insertAndDelete() {
    Database db("t");
    Collection users = db.createCollection("users");
    users.insert({{"name", "Ada"}});
    users.insert({{"name", "Bob"}});

    viewer::DocumentView view;
    QVERIFY(!view.insertEmptyDocument()); // noch keine Collection -> false
    view.showCollection(users);
    QCOMPARE(view.model()->rowCount(), 2);

    QVERIFY(view.insertEmptyDocument()); // FA-14: leeres Dokument einfuegen
    QCOMPARE(users.size(), 3);
    QTRY_VERIFY(view.model()->rowCount() == 3); // Observer -> queued Auto-Refresh

    view.selectRow(0);
    QVERIFY(view.deleteSelectedDocument()); // FA-14: selektierte Zeile loeschen
    QCOMPARE(users.size(), 2);
    QTRY_VERIFY(view.model()->rowCount() == 2);
}

void TestDocumentView::aggregate() {
    Database db("t");
    Collection users = db.createCollection("users");
    users.insert({{"age", 36}});
    users.insert({{"age", 17}});
    users.insert({{"age", 40}});

    viewer::DocumentView view;
    view.showCollection(users);
    QCOMPARE(view.applyAggregate(memdb::AggOp::Count, QString()), QStringLiteral("3")); // FA-17
    QCOMPARE(view.applyAggregate(memdb::AggOp::Sum, QStringLiteral("age")), QStringLiteral("93"));
    QCOMPARE(view.applyAggregate(memdb::AggOp::Min, QStringLiteral("age")), QStringLiteral("17"));
    QCOMPARE(view.applyAggregate(memdb::AggOp::Max, QStringLiteral("age")), QStringLiteral("40"));
    QVERIFY(view.statusText().contains(QStringLiteral("Agg")));
}

void TestDocumentView::documentSelectedSignal() {
    Database db("t");
    Collection users = db.createCollection("users");
    const DocumentId id = users.insert({{"name", "Ada"}});

    viewer::DocumentView view;
    view.showCollection(users);
    QSignalSpy spy(&view, &viewer::DocumentView::documentSelected);

    view.onCellClicked(view.model()->index(0, 0));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toULongLong(), static_cast<quint64>(id.value()));

    view.onCellClicked(QModelIndex()); // ungueltig -> kein Signal
    QCOMPARE(spy.count(), 0);
}

void TestViewerController::autoSyncDetectsNewAndDroppedCollection() {
    Database db("t");
    db.createCollection("users");

    viewer::ViewerController controller;
    controller.setDatabase(db);
    QSignalSpy dbSpy(&controller, &viewer::ViewerController::databaseChanged);
    dbSpy.clear(); // setDatabase emittiert bereits
    QVERIFY(controller.isAutoSyncEnabled());

    // createCollection feuert keinen Collection-Observer -> Auto-Sync-Timer muss es erkennen
    db.createCollection("orders");
    QVERIFY(dbSpy.wait(2000));
    QCOMPARE(controller.databaseModel()->rowCount(), 2);

    // dropCollection ebenfalls ohne Observer-Event -> Auto-Sync
    db.dropCollection("users");
    QVERIFY(dbSpy.wait(2000));
    QCOMPARE(controller.databaseModel()->rowCount(), 1);

    // Auto-Sync abschalten -> keine Signale mehr
    controller.setAutoSyncEnabled(false);
    QVERIFY(!controller.isAutoSyncEnabled());
    db.createCollection("logs");
    QVERIFY(!dbSpy.wait(600));
    QCOMPARE(controller.databaseModel()->rowCount(), 1); // erst nach refresh() sichtbar

    controller.refresh();
    QCOMPARE(controller.databaseModel()->rowCount(), 2);
}
