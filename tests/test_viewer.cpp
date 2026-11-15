#include "test_viewer.hpp"

#include <memdb/memdb.hpp>
#include <memdbviewer/viewer.hpp>

#include <QSignalSpy>
#include <QtTest>

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
