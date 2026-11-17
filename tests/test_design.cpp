// Tests fuer Schritt A: verifizierte Mutationen im ViewerCore (Collections, Schema,
// Indizes, Referenzen, Dokument-CRUD, CSV-Text, Snapshot) + Logging + OutputPanel.
// Schritt B: DatabaseDesignWidget (MVC). Schritt C: CSV aus Zwischenablage + Dokument-CRUD.
#include "test_viewer.hpp"

#include <memdb/memdb.hpp>
#include "../src/database_model.hpp"
#include "../src/design_controller.hpp"
#include "../src/document_table_model.hpp"
#include <memdbviewer/analysis_widget.hpp>
#include <memdbviewer/data_input_widget.hpp>
#include <memdbviewer/data_view_widget.hpp>
#include <memdbviewer/database_design_widget.hpp>
#include <memdbviewer/output_panel.hpp>
#include <memdbviewer/viewer_core.hpp>

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBrowser>
#include <QTimer>
#include <QTreeView>

#include <functional>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
// QVERIFY_THROWS_EXCEPTION erst ab Qt 6 – Qt-5.15-kompatible Variante
#define VERIFY_THROWS(excType, expr) \
    do { bool threw_ = false; try { expr; } catch (const excType&) { threw_ = true; } QVERIFY(threw_); } while (0)
#else
#define VERIFY_THROWS(excType, expr) QVERIFY_THROWS_EXCEPTION(excType, expr)
#endif

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
    VERIFY_THROWS(memdb::SchemaError, core.database().collection("users").insert({{"name", "X"}, {"age", "s"}}));

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
    VERIFY_THROWS(memdb::UniqueConstraintError, c.insert({{"name", "Ada"}}));

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

void TestDesignController::collectionSchemaIndexReferenceFlow() {
    ViewerCore core;
    viewer::detail::DesignController ctrl(&core);
    QSignalSpy changedSpy(&ctrl, &viewer::detail::DesignController::changed);

    ctrl.createCollection(QStringLiteral("users"));
    QVERIFY(core.collectionNames().contains(QStringLiteral("users")));
    QVERIFY(changedSpy.count() >= 1);

    // Dokumente, dann Schema anwenden (valide)
    core.insertDocumentJson(QStringLiteral("users"), QStringLiteral("{\"name\":\"Ada\",\"age\":36}"));
    std::vector<viewer::detail::FieldDef> fields = {
        {QStringLiteral("name"), QStringLiteral("string"), false, QString(), QString()},
        {QStringLiteral("age"), QStringLiteral("int"), true, QString(), QString()},
        {QStringLiteral("adresse"), QStringLiteral("document"), true, QStringLiteral("plz:string"), QString()},
    };
    ctrl.applySchema(QStringLiteral("users"), fields);
    QVERIFY(core.database().collection("users").hasSchema());
    QCOMPARE(core.database().collection("users").schema()->size(), std::size_t(3));
    // currentSchema liest es zurueck (inkl. Unter-Schema)
    const auto back = ctrl.currentSchema(QStringLiteral("users"));
    QCOMPARE(back.size(), std::size_t(3));
    bool hasAdresse = false;
    for (const auto& f : back)
        if (f.path == QLatin1String("adresse")) {
            hasAdresse = true;
            QCOMPARE(f.typeName, QStringLiteral("document"));
            QVERIFY(f.subSchema.contains(QStringLiteral("plz:string")));
        }
    QVERIFY(hasAdresse);

    // unvalides Schema (age: string gegen Bestand) -> Fehler im Log, Schema unveraendert
    QSignalSpy logSpy(&core, &ViewerCore::logged);
    std::vector<viewer::detail::FieldDef> bad = {{QStringLiteral("age"), QStringLiteral("string"), true, QString(), QString()}};
    ctrl.applySchema(QStringLiteral("users"), bad);
    QVERIFY(core.database().collection("users").schema()->find("age")->type() == Value::Type::Int);
    bool sawError = false;
    for (const auto& call : logSpy)
        if (call.at(1).toBool())
            sawError = true;
    QVERIFY(sawError);

    // Index anlegen + Duplikat-Fehler
    ctrl.addIndex(QStringLiteral("users"), QStringLiteral("name"), true);
    QCOMPARE(core.database().collection("users").indexes().size(), std::size_t(1));
    core.insertDocumentJson(QStringLiteral("users"), QStringLiteral("{\"name\":\"Bob\",\"age\":17}"));
    ctrl.addIndex(QStringLiteral("users"), QStringLiteral("age"), false);
    QCOMPARE(core.database().collection("users").indexes().size(), std::size_t(2));
    ctrl.removeIndex(QStringLiteral("users"), QStringLiteral("age"));
    QCOMPARE(core.database().collection("users").indexes().size(), std::size_t(1));

    // Referenz anlegen/entfernen
    const auto ada = core.database().collection("users").findOne("name = 'Ada'")->id();
    ctrl.createCollection(QStringLiteral("orders"));
    const auto o = core.insertDocumentJson(QStringLiteral("orders"), QStringLiteral("{\"total\":99.5}"));
    quint64 oid = 0;
    core.insertDocumentJson(QStringLiteral("orders"), QStringLiteral("{\"total\":1.0}"), &oid);
    ctrl.addReference(QStringLiteral("orders"), oid, true, QStringLiteral("users"), ada.value(), true,
                      QStringLiteral("kunde"));
    QVERIFY(!core.references()->references("orders", RefDirection::Outgoing, memdb::DocumentId(oid)).empty());
    ctrl.removeReference(QStringLiteral("orders"), oid, true, QStringLiteral("users"), ada.value(), true,
                         QStringLiteral("kunde"));
    QVERIFY(core.references()->references("orders", RefDirection::Outgoing, memdb::DocumentId(oid)).empty());

    // Feld-Vorschlaege + Loeschen/Leeren
    QVERIFY(ctrl.fieldSuggestions(QStringLiteral("users")).contains(QStringLiteral("name")));
    ctrl.clearCollection(QStringLiteral("orders"));
    QCOMPARE(core.database().collection("orders").size(), std::size_t(0));
    ctrl.dropCollection(QStringLiteral("orders"));
    QVERIFY(!core.collectionNames().contains(QStringLiteral("orders")));
}

void TestDatabaseDesignWidget::reflectsCoreState() {
    ViewerCore core;
    core.loadDemoData();
    DatabaseDesignWidget widget;
    widget.setCore(&core);
    widget.show();
    QTRY_VERIFY_WITH_TIMEOUT(widget.collectionItemCount() == 2, 3000);
    QCOMPARE(widget.currentCollection(), QStringLiteral("orders")); // erste Collection
    // orders hat 1 Index, users hat Schema mit 3 Feldern
    QCOMPARE(widget.indexItemCount(), 1);
    QCOMPARE(widget.referenceRowCount() >= 1, true); // Demo-Referenz orders->users
    // zur users wechseln: Schema-Tabelle zeigt 3 Felder, 1 unique-Index
    widget.selectCollection(QStringLiteral("users"));
    QTRY_VERIFY_WITH_TIMEOUT(widget.schemaRowCount() == 3, 3000);
    QCOMPARE(widget.currentCollection(), QStringLiteral("users"));
    QCOMPARE(widget.indexItemCount(), 1); // unique-Index auf age
}

// ---------------------------------------------------------------- Schritt C

namespace {
/// @brief Sucht einen Button anhand eines Text-Fragments.
QPushButton* findButton(QWidget* w, const QString& fragment) {
    for (auto* b : w->findChildren<QPushButton*>())
        if (b->text().contains(fragment))
            return b;
    return nullptr;
}

/// @brief Akzeptiert den aktiven modalen Dialog nach `msec` (Offscreen-Tests) und fuellt ihn aus.
void acceptModalDialog(int msec, const std::function<void(QDialog*)>& fill) {
    QTimer::singleShot(msec, [fill] {
        auto* d = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (!d)
            return;
        if (fill)
            fill(d);
        d->accept();
    });
}

/// @brief DataViewWidget mit Demo-Daten aufgesetzt + Collection "users" ausgewaehlt.
///        Rueckgabe: table/model ueber out-Parameter; QTRY-Makros nur im Test-Slot verwenden.
void setupViewWithUsers(ViewerCore& core, DataViewWidget& w, QTableView** tableOut,
                        viewer::detail::DocumentTableModel** modelOut) {
    core.loadDemoData();
    w.setCore(&core);
    w.show();
    *tableOut = w.findChild<QTableView*>();
    *modelOut = *tableOut ? qobject_cast<viewer::detail::DocumentTableModel*>((*tableOut)->model()) : nullptr;
}

int columnOf(viewer::detail::DocumentTableModel* model, const QString& name) {
    for (int c = 0; c < model->columnCount(); ++c)
        if (model->headerData(c, Qt::Horizontal).toString() == name)
            return c;
    return -1;
}

int rowOfName(viewer::detail::DocumentTableModel* model, int nameCol, const QString& value) {
    for (int r = 0; r < model->rowCount(); ++r)
        if (model->data(model->index(r, nameCol)).toString() == value)
            return r;
    return -1;
}

/// @brief Uebersichts-Baum (DatabaseModel) im DataViewWidget finden.
QTreeView* dbTreeOf(DataViewWidget& w) {
    for (auto* t : w.findChildren<QTreeView*>())
        if (qobject_cast<viewer::detail::DatabaseModel*>(t->model()))
            return t;
    return nullptr;
}

/// @brief Collection `name` im Uebersichts-Baum selektieren (loest currentRowChanged aus).
bool selectInDbTree(QTreeView* dbTree, const QString& name) {
    for (int r = 0; r < dbTree->model()->rowCount(); ++r)
        if (dbTree->model()->data(dbTree->model()->index(r, 0)).toString() == name) {
            dbTree->setCurrentIndex(dbTree->model()->index(r, 0));
            return true;
        }
    return false;
}
} // namespace

void TestClipboardCsvImport::importsFromClipboardWithDialog() {
    ViewerCore core;
    QSignalSpy mutSpy(&core, &ViewerCore::dataMutated);
    QSignalSpy logSpy(&core, &ViewerCore::logged);
    QGuiApplication::clipboard()->setText(QStringLiteral("name;age\nAda;36\nBob;41\n"));
    DataInputWidget w;
    w.setCore(&core);
    w.show();
    QPushButton* btn = findButton(&w, QStringLiteral("Zwischenablage"));
    QVERIFY(btn);
    acceptModalDialog(100, [](QDialog* d) {
        for (auto* le : d->findChildren<QLineEdit*>())
            if (le->text() == QStringLiteral("clipboard"))
                le->setText(QStringLiteral("clip"));
        if (auto* combo = d->findChild<QComboBox*>())
            combo->setCurrentIndex(1); // ';' (Semikolon)
    });
    btn->click();
    QTRY_VERIFY_WITH_TIMEOUT(core.collectionNames().contains(QStringLiteral("clip")), 3000);
    auto coll = core.database().collection("clip");
    QCOMPARE(coll.size(), std::size_t(2));
    bool foundAda = false;
    coll.forEach(memdb::Expression{}, [&](const memdb::Document& doc) {
        const auto* name = doc.findPath("name");
        if (name && name->asString() == "Ada") {
            foundAda = true;
            // ohne Typ-Inferenz sind alle Spalten Text
            QCOMPARE(doc.findPath("age")->type(), memdb::Value::Type::String);
        }
    });
    QVERIFY(foundAda);
    QVERIFY(mutSpy.count() >= 1); // dataMutated wurde ausgeloest
    QVERIFY(logSpy.count() >= 1); // Erfolg im Log
}

void TestClipboardCsvImport::emptyClipboardReportsStatus() {
    ViewerCore core;
    QGuiApplication::clipboard()->clear();
    DataInputWidget w;
    w.setCore(&core);
    w.show();
    QPushButton* btn = findButton(&w, QStringLiteral("Zwischenablage"));
    QVERIFY(btn);
    QSignalSpy statusSpy(&w, &DataInputWidget::statusMessage);
    btn->click();
    QVERIFY(statusSpy.count() >= 1);
    QVERIFY(statusSpy.first().at(0).toString().contains(QStringLiteral("Zwischenablage")));
    QVERIFY(core.collectionNames().isEmpty()); // nichts importiert
}

void TestDocumentCrudWidget::cellEditVerifiesAndMutates() {
    ViewerCore core;
    QSignalSpy mutSpy(&core, &ViewerCore::dataMutated);
    DataViewWidget w;
    QTableView* table = nullptr;
    viewer::detail::DocumentTableModel* model = nullptr;
    setupViewWithUsers(core, w, &table, &model);
    QVERIFY(table && model);
    QTRY_VERIFY_WITH_TIMEOUT(w.tableRowCount() > 0, 3000);
    // Collection "users" in der Uebersicht auswaehlen
    QTreeView* dbTree = dbTreeOf(w);
    QVERIFY(dbTree);
    QVERIFY(dbTree->model()->rowCount() >= 2);
    QVERIFY(selectInDbTree(dbTree, QStringLiteral("users")));
    QTRY_COMPARE_WITH_TIMEOUT(w.currentCollection(), QStringLiteral("users"), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() == 3, 3000);
    const int nameCol = columnOf(model, QStringLiteral("name"));
    const int ageCol = columnOf(model, QStringLiteral("age"));
    QVERIFY(nameCol > 0 && ageCol > 0);
    // Flags: id-Spalte geschuetzt, Datenzellen editierbar
    QVERIFY(!(model->flags(model->index(0, 0)) & Qt::ItemIsEditable));
    QVERIFY(model->flags(model->index(0, nameCol)) & Qt::ItemIsEditable);
    const int adaRow = rowOfName(model, nameCol, QStringLiteral("Ada"));
    QVERIFY(adaRow >= 0);
    const quint64 adaId = model->idAt(adaRow);
    const int mutBefore = mutSpy.count();
    // Zelle bearbeiten -> verifizierte Mutation ueber den ViewerCore
    QVERIFY(model->setData(model->index(adaRow, nameCol), QStringLiteral("Grace"), Qt::EditRole));
    const auto doc = core.database().collection("users").findById(memdb::DocumentId(adaId));
    QVERIFY(doc);
    QCOMPARE(doc->findPath("name")->asString(), std::string("Grace"));
    QVERIFY(mutSpy.count() > mutBefore); // dataMutated
}

void TestDocumentCrudWidget::cellEditSchemaErrorIsLogged() {
    ViewerCore core;
    QSignalSpy mutSpy(&core, &ViewerCore::dataMutated);
    QSignalSpy logSpy(&core, &ViewerCore::logged);
    DataViewWidget w;
    QTableView* table = nullptr;
    viewer::detail::DocumentTableModel* model = nullptr;
    setupViewWithUsers(core, w, &table, &model);
    QVERIFY(table && model);
    QTRY_VERIFY_WITH_TIMEOUT(w.tableRowCount() > 0, 3000);
    QTreeView* dbTree = dbTreeOf(w);
    QVERIFY(dbTree);
    QVERIFY(selectInDbTree(dbTree, QStringLiteral("users")));
    QTRY_COMPARE_WITH_TIMEOUT(w.currentCollection(), QStringLiteral("users"), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() == 3, 3000);
    const int nameCol = columnOf(model, QStringLiteral("name"));
    const int ageCol = columnOf(model, QStringLiteral("age"));
    const int adaRow = rowOfName(model, nameCol, QStringLiteral("Ada"));
    QVERIFY(adaRow >= 0 && ageCol > 0);
    const quint64 adaId = model->idAt(adaRow);
    const int logBefore = logSpy.count();
    const int mutBefore = mutSpy.count();
    // age ist im Demo-Schema Int -> "abc" (String) muss SchemaError werfen
    model->setData(model->index(adaRow, ageCol), QStringLiteral("abc"), Qt::EditRole);
    const auto doc = core.database().collection("users").findById(memdb::DocumentId(adaId));
    QVERIFY(doc);
    QCOMPARE(doc->findPath("age")->asInt(), 36); // Wert unveraendert
    QCOMPARE(mutSpy.count(), mutBefore);         // kein dataMutated bei Fehler
    bool sawError = false;
    for (int i = logBefore; i < logSpy.count(); ++i)
        if (logSpy.at(i).at(1).toBool())
            sawError = true;
    QVERIFY(sawError); // Fehler (rot) im Ausgabe-Log
}

void TestDocumentCrudWidget::insertAndDeleteDocument() {
    ViewerCore core;
    QSignalSpy mutSpy(&core, &ViewerCore::dataMutated);
    DataViewWidget w;
    QTableView* table = nullptr;
    viewer::detail::DocumentTableModel* model = nullptr;
    setupViewWithUsers(core, w, &table, &model);
    QVERIFY(table && model);
    QTRY_VERIFY_WITH_TIMEOUT(w.tableRowCount() > 0, 3000);
    QTreeView* dbTree = dbTreeOf(w);
    QVERIFY(dbTree);
    QVERIFY(selectInDbTree(dbTree, QStringLiteral("users")));
    QTRY_COMPARE_WITH_TIMEOUT(w.currentCollection(), QStringLiteral("users"), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(model->rowCount() == 3, 3000);
    const int nameCol = columnOf(model, QStringLiteral("name"));
    QVERIFY(nameCol > 0);
    const std::size_t before = core.database().collection("users").size();
    const int mutBefore = mutSpy.count();
    // "Dokument neu…" -> JSON-Dialog ausfuellen -> OK
    QPushButton* newBtn = findButton(&w, QStringLiteral("Dokument neu"));
    QVERIFY(newBtn);
    acceptModalDialog(100, [](QDialog* d) {
        if (auto* pe = d->findChild<QPlainTextEdit*>())
            pe->setPlainText(QStringLiteral("{\"name\":\"Linus\",\"age\":55}"));
    });
    newBtn->click();
    QTRY_COMPARE_WITH_TIMEOUT(core.database().collection("users").size(), before + 1, 3000);
    bool found = false;
    core.database().collection("users").forEach(memdb::Expression{}, [&](const memdb::Document& doc) {
        const auto* n = doc.findPath("name");
        if (n && n->asString() == "Linus")
            found = true;
    });
    QVERIFY(found);
    QVERIFY(mutSpy.count() > mutBefore);
    // neue Zeile erscheint in der Tabelle (Live-Refresh) und wird geloescht
    QTRY_VERIFY_WITH_TIMEOUT(rowOfName(model, nameCol, QStringLiteral("Linus")) >= 0, 3000);
    const int linusRow = rowOfName(model, nameCol, QStringLiteral("Linus"));
    table->selectRow(linusRow);
    QPushButton* delBtn = findButton(&w, QStringLiteral("Dokument löschen"));
    QVERIFY(delBtn);
    delBtn->click(); // Offscreen: ohne Rueckfrage
    QTRY_COMPARE_WITH_TIMEOUT(core.database().collection("users").size(), before, 3000);
    QVERIFY(mutSpy.count() > mutBefore + 1);
}

void TestDocumentCrudWidget::invalidJsonIsLogged() {
    ViewerCore core;
    QSignalSpy logSpy(&core, &ViewerCore::logged);
    DataViewWidget w;
    QTableView* table = nullptr;
    viewer::detail::DocumentTableModel* model = nullptr;
    setupViewWithUsers(core, w, &table, &model);
    QVERIFY(table && model);
    QTRY_VERIFY_WITH_TIMEOUT(w.tableRowCount() > 0, 3000);
    QTreeView* dbTree = dbTreeOf(w);
    QVERIFY(dbTree);
    QVERIFY(selectInDbTree(dbTree, QStringLiteral("users")));
    QTRY_COMPARE_WITH_TIMEOUT(w.currentCollection(), QStringLiteral("users"), 3000);
    const std::size_t before = core.database().collection("users").size();
    const int logBefore = logSpy.count();
    QPushButton* newBtn = findButton(&w, QStringLiteral("Dokument neu"));
    QVERIFY(newBtn);
    acceptModalDialog(100, [](QDialog* d) {
        if (auto* pe = d->findChild<QPlainTextEdit*>())
            pe->setPlainText(QStringLiteral("{invalid json"));
    });
    newBtn->click();
    QTest::qWait(150); // synchroner Fehler, kein Warten auf GUI-Events noetig
    QCOMPARE(core.database().collection("users").size(), before); // nichts eingefuegt
    bool sawError = false;
    for (int i = logBefore; i < logSpy.count(); ++i)
        if (logSpy.at(i).at(1).toBool())
            sawError = true;
    QVERIFY(sawError); // JSON-Fehler (rot) im Ausgabe-Log
}

// ---------------------------------------------------------------- Schritt D

void TestAutoAnalysis::mutationTriggersAutoAnalysis() {
    ViewerCore core;
    core.loadDemoData();
    AnalysisWidget w;
    w.setCore(&core);
    w.setCore(&core); // doppelt darf nicht doppelt verbinden
    w.show();
    QVERIFY(w.autoAnalysisEnabled()); // Default: an
    QSignalSpy finSpy(&w, &AnalysisWidget::analysisFinished);
    core.createCollection(QStringLiteral("extra")); // -> dataMutated -> Auto-Analyse (DE-06)
    QTRY_VERIFY_WITH_TIMEOUT(finSpy.count() >= 1, 15000);
    QVERIFY(w.healthScore() >= 0.0);
    QVERIFY(core.collectionNames().contains(QStringLiteral("extra")));
}

void TestAutoAnalysis::disabledDoesNotRun() {
    ViewerCore core;
    core.loadDemoData();
    AnalysisWidget w;
    w.setCore(&core);
    w.show();
    w.setAutoAnalysisEnabled(false);
    QVERIFY(!w.autoAnalysisEnabled());
    QSignalSpy finSpy(&w, &AnalysisWidget::analysisFinished);
    core.createCollection(QStringLiteral("x"));
    QTest::qWait(1500); // laenger als der Debounce (400 ms)
    QCOMPARE(finSpy.count(), 0);
}

void TestAutoAnalysis::rapidMutationsAreDebounced() {
    ViewerCore core;
    core.loadDemoData();
    AnalysisWidget w;
    w.setCore(&core);
    w.show();
    QSignalSpy finSpy(&w, &AnalysisWidget::analysisFinished);
    // drei schnelle Mutationen -> Debounce buendelt auf genau einen Lauf
    core.createCollection(QStringLiteral("a"));
    core.createCollection(QStringLiteral("b"));
    core.createCollection(QStringLiteral("c"));
    QTRY_VERIFY_WITH_TIMEOUT(finSpy.count() >= 1, 15000);
    QTest::qWait(1500);
    QCOMPARE(finSpy.count(), 1);
}

void TestAutoAnalysis::rerunAfterBusyMutation() {
    ViewerCore core;
    core.loadDemoData();
    AnalysisWidget w;
    w.setCore(&core);
    w.show();
    QSignalSpy finSpy(&w, &AnalysisWidget::analysisFinished);
    w.runAnalysis(); // manueller Lauf
    core.createCollection(QStringLiteral("busy")); // Mutation -> Debounce bzw. Rerun
    // egal ob die Mutation waehrend des Laufs (rerunPending) oder danach (Debounce) ankam:
    // es muss ein zweiter Lauf folgen
    QTRY_VERIFY_WITH_TIMEOUT(finSpy.count() >= 2, 20000);
}
