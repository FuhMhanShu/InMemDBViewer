// Tests fuer Schritt A: verifizierte Mutationen im ViewerCore (Collections, Schema,
// Indizes, Referenzen, Dokument-CRUD, CSV-Text, Snapshot) + Logging + OutputPanel.
#include "test_viewer.hpp"

#include <memdb/memdb.hpp>
#include <memdbviewer/output_panel.hpp>
#include <memdbviewer/viewer_core.hpp>

#include <QSignalSpy>
#include <QTextBrowser>
#include <QTemporaryDir>
#include <QTest>

using namespace memdb;
using namespace memdb::viewer;

void TestViewerCoreMutations::collectionOps() {
    ViewerCore core;
    QSignalSpy mutSpy(&core, &ViewerCore::dataMutated);
    QSignalSpy logSpy(&core, &ViewerCore::logged);

    QString err;
    QVERIFY(core.createCollection(QStringLiteral("users"), &err));
    QVERIFY(core.collectionNames().contains(QStringLiteral("users")));
    // doppelt -> verifizierter Fehler
    QVERIFY(!core.createCollection(QStringLiteral("users"), &err));
    QVERIFY(!err.isEmpty());
    QVERIFY(logSpy.count() >= 1);
    QVERIFY(mutSpy.count() >= 1);

    core.database().collection("users").insert({{"name", "Ada"}, {"age", 36}});
    QVERIFY(core.clearCollectionContent(QStringLiteral("users"), &err));
    QCOMPARE(core.database().collection("users").size(), std::size_t(0));

    QVERIFY(core.dropCollection(QStringLiteral("users"), &err));
    QVERIFY(!core.collectionNames().contains(QStringLiteral("users")));
    QVERIFY(!core.dropCollection(QStringLiteral("users"), &err)); // nicht mehr vorhanden -> Fehler
}

void TestViewerCoreMutations::schemaOps() {
    ViewerCore core;
    core.createCollection(QStringLiteral("users"));
    core.database().collection("users").insert({{"name", "Ada"}, {"age", 36}});

    QString err;
    Schema schema;
    schema.add("name", Value::Type::String).notNull();
    schema.add("age", Value::Type::Int);
    QVERIFY(core.setCollectionSchema(QStringLiteral("users"), schema, &err)); // valide gegen Bestand
    QVERIFY(core.database().collection("users").hasSchema());

    // unvalide gegen Bestand: age als String deklarieren -> SchemaError, verifiziert
    Schema bad;
    bad.add("name", Value::Type::String);
    bad.add("age", Value::Type::String);
    QVERIFY(!core.setCollectionSchema(QStringLiteral("users"), bad, &err));
    QVERIFY(!err.isEmpty());
    QVERIFY(core.database().collection("users").schema()->find("age")->type() == Value::Type::Int); // unveraendert

    // neues Dokument mit falschem Typ -> SchemaError beim insert (InMemDB prueft)
    QVERIFY_THROWS_EXCEPTION(memdb::SchemaError,
                             core.database().collection("users").insert({{"name", "X"}, {"age", "s"}}));

    QVERIFY(core.clearCollectionSchema(QStringLiteral("users"), &err));
    QVERIFY(!core.database().collection("users").hasSchema());
}

void TestViewerCoreMutations::indexOps() {
    ViewerCore core;
    core.createCollection(QStringLiteral("users"));
    auto c = core.database().collection("users");
    c.insert({{"name", "Ada"}, {"city", "Berlin"}});
    c.insert({{"name", "Bob"}, {"city", "Bonn"}});
    c.insert({{"name", "Cid"}, {"city", "Berlin"}}); // Duplikat in city

    QString err;
    QVERIFY(core.createIndex(QStringLiteral("users"), QStringLiteral("name"), true, &err)); // unique ok
    // Index existiert bereits -> IndexExistsError, verifiziert
    QVERIFY(!core.createIndex(QStringLiteral("users"), QStringLiteral("name"), false, &err));
    QVERIFY(!err.isEmpty());
    // unique-Index auf Feld MIT Duplikaten -> UniqueConstraintError, verifiziert
    QVERIFY(!core.createIndex(QStringLiteral("users"), QStringLiteral("city"), true, &err));
    QVERIFY(!err.isEmpty());
    // nicht-unique auf city geht
    QVERIFY(core.createIndex(QStringLiteral("users"), QStringLiteral("city"), false, &err));
    // insert mit Duplikat bei unique-Index throwt (Verifikation durch InMemDB)
    QVERIFY_THROWS_EXCEPTION(memdb::UniqueConstraintError, c.insert({{"name", "Ada"}}));

    QVERIFY(core.dropIndex(QStringLiteral("users"), QStringLiteral("name"), &err));
    QVERIFY(!core.dropIndex(QStringLiteral("users"), QStringLiteral("name"), &err)); // schon weg -> Fehler
}

void TestViewerCoreMutations::referenceOps() {
    ViewerCore core;
    core.loadDemoData();
    QString err;
    const auto users = core.database().collection("users");
    const auto orders = core.database().collection("orders");
    const auto ada = users.findOne("name = 'Ada'")->id();
    const auto o1 = orders.findOne("total = 99.5")->id();

    QVERIFY(core.linkReference(QStringLiteral("orders"), o1.value(), true, QStringLiteral("users"), ada.value(), true,
                               QStringLiteral("kunde2"), &err));
    QVERIFY(core.references() != nullptr);
    bool found = false;
    for (const auto& r : core.references()->references("orders", RefDirection::Outgoing, o1))
        if (r.relation == "kunde2")
            found = true;
    QVERIFY(found);

    QVERIFY(core.unlinkReference(QStringLiteral("orders"), o1.value(), true, QStringLiteral("users"), ada.value(), true,
                                 QStringLiteral("kunde2"), &err));

    // dangling: Referenz auf echtes Dokument, dann Dokument loeschen -> dangling -> prune
    const auto bob = users.findOne("name = 'Bob'")->id();
    QVERIFY(core.linkReference(QStringLiteral("orders"), o1.value(), true, QStringLiteral("users"), bob.value(), true,
                               QStringLiteral("dangling"), &err));
    QVERIFY(core.removeDocument(QStringLiteral("users"), bob.value(), &err));
    QVERIFY(!core.references()->dangling().empty());
    QSignalSpy mutSpy(&core, &ViewerCore::dataMutated);
    QCOMPARE(core.pruneDanglingReferences(&err), std::size_t(1));
    QVERIFY(core.references()->dangling().empty());
    QVERIFY(mutSpy.count() >= 1);
}

void TestViewerCoreMutations::documentCrud() {
    ViewerCore core;
    core.createCollection(QStringLiteral("users"));
    QString err;
    quint64 id = 0;
    QVERIFY(core.insertDocumentJson(QStringLiteral("users"), QStringLiteral("{\"name\":\"Ada\",\"age\":36}"), &id, &err));
    QVERIFY(id != 0);
    QCOMPARE(core.database().collection("users").size(), std::size_t(1));

    // ungueltiges JSON -> verifizierter Fehler
    QVERIFY(!core.insertDocumentJson(QStringLiteral("users"), QStringLiteral("{kein json"), nullptr, &err));
    QVERIFY(!err.isEmpty());

    // Feld aktualisieren (Typ-Inferenz: int, double, bool, string)
    QVERIFY(core.updateDocumentField(QStringLiteral("users"), id, QStringLiteral("age"), QStringLiteral("37"), &err));
    QCOMPARE(core.database().collection("users").findById(DocumentId(id))->at("age").asInt(), std::int64_t(37));
    QVERIFY(core.updateDocumentField(QStringLiteral("users"), id, QStringLiteral("score"), QStringLiteral("9.5"), &err));
    QVERIFY(core.database().collection("users").findById(DocumentId(id))->at("score").isDouble());
    QVERIFY(core.updateDocumentField(QStringLiteral("users"), id, QStringLiteral("vip"), QStringLiteral("true"), &err));
    QVERIFY(core.database().collection("users").findById(DocumentId(id))->at("vip").asBool());
    QVERIFY(core.updateDocumentField(QStringLiteral("users"), id, QStringLiteral("name"), QStringLiteral("Ada2"), &err));
    QCOMPARE(core.database().collection("users").findById(DocumentId(id))->at("name").asString(), std::string("Ada2"));
    // Punktnotation (verschachteltes Dokument wird angelegt); "Berlin" bleibt String, "10115" wuerde als Int geparst
    QVERIFY(core.updateDocumentField(QStringLiteral("users"), id, QStringLiteral("adresse.ort"), QStringLiteral("Berlin"), &err));
    QCOMPARE(core.database().collection("users").findById(DocumentId(id))->findPath("adresse.ort")->asString(),
             std::string("Berlin"));
    // nicht vorhandenes Dokument -> Fehler
    QVERIFY(!core.updateDocumentField(QStringLiteral("users"), 9999, QStringLiteral("age"), QStringLiteral("1"), &err));

    QVERIFY(core.removeDocument(QStringLiteral("users"), id, &err));
    QCOMPARE(core.database().collection("users").size(), std::size_t(0));
    QVERIFY(!core.removeDocument(QStringLiteral("users"), id, &err));
}

void TestViewerCoreMutations::csvTextImport() {
    ViewerCore core;
    QString err;
    memdb::CsvImportOptions opts;
    opts.delimiter = ';';
    opts.hasHeader = true;
    opts.inferTypes = true;
    QVERIFY(core.importCsvText(QStringLiteral("clip"), QStringLiteral("name;age\nAda;36\nBob;17\n"), opts, &err));
    QVERIFY(core.database().hasCollection("clip"));
    const auto c = core.database().collection("clip");
    QCOMPARE(c.size(), std::size_t(2));
    const auto ada = c.findOne("name = 'Ada'");
    QVERIFY(ada.has_value());
    QCOMPARE(ada->at("age").asInt(), std::int64_t(36)); // Typ-Inferenz
}

void TestViewerCoreMutations::snapshotSave() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ViewerCore core;
    core.loadDemoData();
    QString err;
    const QString plain = dir.filePath(QStringLiteral("db.inmemdb.json"));
    const QString enc = dir.filePath(QStringLiteral("db.enc"));
    QVERIFY(core.saveSnapshot(plain, &err));
    QVERIFY(core.saveSnapshotEncrypted(enc, QStringLiteral("pw"), &err));

    ViewerCore core2;
    QVERIFY(core2.loadSnapshot(plain, &err));
    QVERIFY(core2.collectionNames().contains(QStringLiteral("users")));
    QVERIFY(core2.loadSnapshotEncrypted(enc, QStringLiteral("pw"), &err));
    QVERIFY(!core2.loadSnapshotEncrypted(enc, QStringLiteral("falsch"), &err));
}

void TestViewerCoreMutations::dataMutatedOnImport() {
    ViewerCore core;
    QSignalSpy spy(&core, &ViewerCore::dataMutated);
#ifndef TESTS_DATA_DIR
#define TESTS_DATA_DIR "data"
#endif
    QVERIFY(core.importFile(QStringLiteral(TESTS_DATA_DIR) + QStringLiteral("/personen.csv")));
    QVERIFY(spy.count() >= 1);
}

void TestOutputPanel::logsAndFilters() {
    ViewerCore core;
    OutputPanel panel;
    panel.setCore(&core);
    panel.show();
    core.log(QStringLiteral("alles ok"));
    core.log(QStringLiteral("Fehler X"), true);
    const QString html = panel.findChild<QTextBrowser*>()->toHtml();
    QVERIFY(html.contains(QStringLiteral("alles ok")));
    QVERIFY(html.contains(QStringLiteral("Fehler X")));
    QVERIFY(html.contains(QStringLiteral("#c62828"))); // Fehler rot
    panel.clear();
    QVERIFY(!panel.findChild<QTextBrowser*>()->toHtml().contains(QStringLiteral("Fehler X")));
}
