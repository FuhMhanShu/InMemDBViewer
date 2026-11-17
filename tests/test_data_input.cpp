// Tests fuer Schritt 2 (Dateneingabe): ViewerCore (Import FA-21, Snapshots FA-14/FA-19,
// Observer-Bridge FA-15, Auto-Sync), ImportModel und DataInputWidget (Drag & Drop).
// Headless: QT_QPA_PLATFORM=offscreen ./viewer_tests
#include "test_viewer.hpp"

#include "../src/data_input_controller.hpp"
#include "../src/import_model.hpp"

#include <memdb/memdb.hpp>
#include <memdbviewer/data_input_widget.hpp>
#include <memdbviewer/viewer_core.hpp>

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#ifndef TESTS_DATA_DIR
#define TESTS_DATA_DIR "data"
#endif

static QString dataFile(const QString& name) {
    return QStringLiteral(TESTS_DATA_DIR) + QLatin1Char('/') + name;
}

using namespace memdb;
using namespace memdb::viewer;

void TestViewerCore::importCsv() {
    ViewerCore core;
    QString err;
    QVERIFY(core.importFile(dataFile(QStringLiteral("personen.csv")), &err));
    QVERIFY(err.isEmpty());
    QVERIFY(core.database().hasCollection("personen"));
    QCOMPARE(core.database().collection("personen").size(), std::size_t(3));
    QVERIFY(core.lastSourceDescription().contains(QStringLiteral("personen.csv")));
    QVERIFY(core.collectionNames().contains(QStringLiteral("personen")));
}

void TestViewerCore::importJson() {
    ViewerCore core;
    QString err;
    QVERIFY(core.importFile(dataFile(QStringLiteral("personen.json")), &err));
    QVERIFY(core.database().hasCollection("personen"));
    QCOMPARE(core.database().collection("personen").size(), std::size_t(3));
    auto doc = core.database().collection("personen").findOne("name = 'Ada'");
    QVERIFY(doc.has_value());
    QCOMPARE(doc->at("age").asInt(), std::int64_t(36));
}

void TestViewerCore::importJsonl() {
    ViewerCore core;
    QString err;
    QVERIFY(core.importFile(dataFile(QStringLiteral("personen.jsonl")), &err));
    QVERIFY(core.database().hasCollection("personen"));
    QCOMPARE(core.database().collection("personen").size(), std::size_t(2));
}

void TestViewerCore::importSqlDump() {
    ViewerCore core;
    QString err;
    QVERIFY(core.importFile(dataFile(QStringLiteral("shop.sql")), &err));
    QVERIFY(core.database().hasCollection("kunden"));
    QVERIFY(core.database().hasCollection("bestellungen"));
    QCOMPARE(core.database().collection("kunden").size(), std::size_t(2));
    QCOMPARE(core.database().collection("bestellungen").size(), std::size_t(2));
    // FA-26: Fremdschluessel kunde_id -> kunden.id als FA-18-Referenz
    QVERIFY(core.lastSourceDescription().contains(QStringLiteral("Referenzen")));
    QVERIFY(core.references() != nullptr);
}

void TestViewerCore::importSqlite() {
#ifdef INMEMDB_HAS_SQLITE
    ViewerCore core;
    QString err;
    QVERIFY(core.importFile(dataFile(QStringLiteral("shop.db")), &err));
    QVERIFY(core.database().hasCollection("kunden"));
    QVERIFY(core.database().hasCollection("bestellungen"));
    QCOMPARE(core.database().collection("kunden").size(), std::size_t(3));
    QCOMPARE(core.database().collection("bestellungen").size(), std::size_t(3));
    // SQLite-Index + Unique-Index uebernommen (FA-21 createIndexes)
    QVERIFY(!core.database().collection("kunden").indexes().empty());
#else
    // Ohne INMEMDB_WITH_SQLITE: .db ist nicht registriert -> sauberer Fehler
    ViewerCore core;
    QString err;
    QVERIFY(!core.importFile(dataFile(QStringLiteral("shop.db")), &err));
    QVERIFY(!err.isEmpty());
#endif
}

void TestViewerCore::unknownExtensionFails() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("data.xyz"));
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("hello");
    f.close();
    ViewerCore core;
    QString err;
    QVERIFY(!core.importFile(path, &err));
    QVERIFY(!err.isEmpty());
    QCOMPARE(core.lastError(), err);
}

void TestViewerCore::encryptedSnapshotRoundtrip() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString encPath = dir.filePath(QStringLiteral("shop.enc"));

    Database db("shop");
    db.createCollection("kunden").insert({{"name", "Ada"}});
    db.saveToFileEncrypted(encPath.toStdString(), "geheim");

    QVERIFY(ViewerCore::isEncryptedSnapshotFile(encPath));
    QVERIFY(!ViewerCore::isEncryptedSnapshotFile(dataFile(QStringLiteral("personen.csv"))));

    ViewerCore core;
    QString err;
    // Falsches Passwort -> Fehler
    QVERIFY(!core.loadSnapshotEncrypted(encPath, QStringLiteral("falsch"), &err));
    QVERIFY(!err.isEmpty());
    // Richtiges Passwort -> Database ersetzt
    QSignalSpy loadedSpy(&core, &ViewerCore::databaseLoaded);
    QVERIFY(core.loadSnapshotEncrypted(encPath, QStringLiteral("geheim"), &err));
    QVERIFY(core.database().hasCollection("kunden"));
    QCOMPARE(loadedSpy.count(), 1);
    // Import-Pfad lehnt verschluesselte Datei mit klarem Hinweis ab
    QVERIFY(!core.importFile(encPath, &err));
    QVERIFY(err.contains(QStringLiteral("Passwort")) || err.contains(QStringLiteral("Snapshot")));
}

void TestViewerCore::snapshotRoundtrip() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString snapPath = dir.filePath(QStringLiteral("shop.inmemdb.json"));

    Database db("shop");
    auto kunden = db.createCollection("kunden");
    kunden.createIndex("name", true);
    kunden.insert({{"name", "Ada"}, {"age", 36}});
    kunden.insert({{"name", "Bob"}, {"age", 17}});
    db.saveToFile(snapPath.toStdString());

    ViewerCore core;
    QString err;
    QVERIFY(core.loadSnapshot(snapPath, &err));
    QCOMPARE(core.collectionNames(), QStringList{QStringLiteral("kunden")});
    QCOMPARE(core.database().collection("kunden").size(), std::size_t(2));
    QCOMPARE(core.database().collection("kunden").indexes().size(), std::size_t(1));

    // Snapshot-Datei auch als Import (in laufende DB, z. B. .inmemdb.json per Drag & Drop)
    ViewerCore core2;
    QVERIFY(core2.importFile(snapPath, &err));
    QVERIFY(core2.database().hasCollection("kunden"));
}

void TestViewerCore::resetDatabaseEmitsSignals() {
    ViewerCore core;
    QVERIFY(core.importFile(dataFile(QStringLiteral("personen.csv"))));
    QSignalSpy clearedSpy(&core, &ViewerCore::databaseCleared);
    QSignalSpy loadedSpy(&core, &ViewerCore::databaseLoaded);
    core.resetDatabase(QStringLiteral("leer"));
    QCOMPARE(clearedSpy.count(), 1);
    QCOMPARE(loadedSpy.count(), 1);
    QVERIFY(core.collectionNames().isEmpty());
}

void TestViewerCore::observerTriggersSignal() {
    ViewerCore core;
    memdb::ImportOptions opts; // kein Schema generieren, damit beliebige Typen eingefügt werden duerfen
    opts.generateSchema = false;
    core.setImportOptions(opts);
    QVERIFY(core.importFile(dataFile(QStringLiteral("personen.csv"))));
    QSignalSpy spy(&core, &ViewerCore::collectionChanged);
    core.database().collection("personen").insert({{"name", "Neu"}, {"age", 1}});
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 3000);
    QCOMPARE(spy.first().at(0).toString(), QStringLiteral("personen"));
}

void TestViewerCore::autoSyncDetectsNewCollection() {
    ViewerCore core;
    QSignalSpy spy(&core, &ViewerCore::collectionsChanged);
    core.database().createCollection("neu");
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 3000);
    QVERIFY(core.collectionNames().contains(QStringLiteral("neu")));
}

void TestViewerCore::demoDataLoads() {
    ViewerCore core;
    QSignalSpy spy(&core, &ViewerCore::databaseLoaded);
    core.loadDemoData();
    QCOMPARE(spy.count(), 1);
    QVERIFY(core.collectionNames().contains(QStringLiteral("users")));
    QVERIFY(core.collectionNames().contains(QStringLiteral("orders")));
    QCOMPARE(core.database().collection("users").size(), std::size_t(3));
}

void TestViewerCore::asyncImportWorks() {
    ViewerCore core;
    QSignalSpy spy(&core, &ViewerCore::importFinished);
    core.importFileAsync(dataFile(QStringLiteral("personen.csv")));
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 10000);
    QCOMPARE(spy.first().at(1).toBool(), true);
    QVERIFY(core.database().hasCollection("personen"));
    QTRY_VERIFY_WITH_TIMEOUT(!core.isBusy(), 3000);
}

void TestViewerCore::asyncImportQueue() {
    // Zwei asynchrone Imports direkt hintereinander (z. B. Mehrfach-Drag&Drop):
    // der zweite wird gequeued statt abgelehnt.
    ViewerCore core;
    QSignalSpy spy(&core, &ViewerCore::importFinished);
    core.importFileAsync(dataFile(QStringLiteral("personen.csv")));
    core.importFileAsync(dataFile(QStringLiteral("shop.sql")));
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 2, 15000);
    int okCount = 0;
    for (const auto& call : spy)
        if (call.at(1).toBool())
            ++okCount;
    QCOMPARE(okCount, 2);
    QVERIFY(core.database().hasCollection("personen"));
    QVERIFY(core.database().hasCollection("bestellungen"));
    QTRY_VERIFY_WITH_TIMEOUT(!core.isBusy(), 3000);
}

void TestImportModel::addUpdateClear() {
    viewer::detail::ImportModel model;
    QCOMPARE(model.rowCount(), 0);
    model.addResult(QStringLiteral("/tmp/a.csv"), QStringLiteral("csv"), true, QStringLiteral("3 Zeilen"));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), viewer::detail::ImportModel::PathRole).toString(), QStringLiteral("/tmp/a.csv"));
    QCOMPARE(model.data(model.index(0, 0), viewer::detail::ImportModel::OkRole).toBool(), true);
    QVERIFY(model.data(model.index(0, 0), Qt::DisplayRole).toString().contains(QStringLiteral("3 Zeilen")));
    model.updateLatest(QStringLiteral("/tmp/a.csv"), false, QStringLiteral("Fehler"));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), viewer::detail::ImportModel::OkRole).toBool(), false);
    model.addResult(QStringLiteral("/tmp/b.json"), QStringLiteral("json"), true, QStringLiteral("ok"));
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.pathAt(0), QStringLiteral("/tmp/b.json"));
    model.clearHistory();
    QCOMPARE(model.rowCount(), 0);
}

void TestDataInputWidget::supportedFormatsListed() {
    const QString text = DataInputWidget::supportedFormatsText();
    QVERIFY(text.contains(QStringLiteral("csv")));
    QVERIFY(text.contains(QStringLiteral("json")));
    QVERIFY(text.contains(QStringLiteral(".enc")));
    QVERIFY(ViewerCore::supportedExtensions().contains(QStringLiteral(".csv")));
    QVERIFY(ViewerCore::fileDialogFilter().contains(QStringLiteral("*.csv")));
#ifdef INMEMDB_HAS_SQLITE
    QVERIFY(text.contains(QStringLiteral("sqlite")));
#else
    QVERIFY(!text.contains(QStringLiteral("sqlite")));
#endif
}

namespace {
// Test-Hook: macht die geschuetzten Drag&Drop-Handler direkt aufrufbar
// (QApplication::sendEvent umgeht in Qt6 den Drop-Handler des Widgets).
class TestableInputWidget : public DataInputWidget {
public:
    using DataInputWidget::DataInputWidget;
    using DataInputWidget::dragEnterEvent;
    using DataInputWidget::dropEvent;
};
} // namespace

void TestDataInputWidget::dropCsvImports() {
    ViewerCore core;
    TestableInputWidget widget;
    widget.setCore(&core);
    widget.show();

    QSignalSpy spy(&core, &ViewerCore::importFinished);
    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(dataFile(QStringLiteral("personen.csv")))});
    const Qt::DropActions actions = Qt::DropActions(Qt::CopyAction);
    const Qt::MouseButtons buttons = Qt::MouseButtons(Qt::LeftButton);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QDragEnterEvent enter(QPoint(5, 5), actions, &mime, buttons, Qt::NoModifier);
    QDropEvent drop(QPointF(5, 5), actions, &mime, buttons, Qt::NoModifier);
#else
    QDragEnterEvent enter(QPoint(5, 5), actions, &mime, buttons, Qt::NoModifier);
    QDropEvent drop(QPoint(5, 5), actions, &mime, buttons, Qt::NoModifier);
#endif
    widget.dragEnterEvent(&enter);
    widget.dropEvent(&drop);

    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 10000);
    QCOMPARE(spy.first().at(1).toBool(), true);
    QVERIFY(core.database().hasCollection("personen"));
    QCOMPARE(core.database().collection("personen").size(), std::size_t(3));
}

void TestDataInputWidget::handleDroppedFilesAsync() {
    ViewerCore core;
    DataInputWidget widget;
    widget.setCore(&core);
    QSignalSpy statusSpy(&widget, &DataInputWidget::statusMessage);
    widget.handleDroppedFiles({dataFile(QStringLiteral("shop.sql"))});
    QSignalSpy finSpy(&core, &ViewerCore::importFinished);
    QTRY_VERIFY_WITH_TIMEOUT(finSpy.count() >= 1, 10000);
    QVERIFY(core.database().hasCollection("bestellungen"));
    QTRY_VERIFY_WITH_TIMEOUT(statusSpy.count() >= 1, 3000);
}

void TestDataInputWidget::encryptedDropOffscreenIsHandled() {
    // Im Offscreen-Modus darf der Passwort-Dialog nicht aufgehen; der Drop wird sauber gemeldet.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString encPath = dir.filePath(QStringLiteral("x.enc"));
    Database db("x");
    db.createCollection("c").insert({{"a", 1}});
    db.saveToFileEncrypted(encPath.toStdString(), "pw");

    ViewerCore core;
    DataInputWidget widget;
    widget.setCore(&core);
    QSignalSpy statusSpy(&widget, &DataInputWidget::statusMessage);
    widget.handleDroppedFiles({encPath});
    QTRY_VERIFY_WITH_TIMEOUT(statusSpy.count() >= 1, 3000);
    QVERIFY(core.collectionNames().isEmpty()); // nichts geladen
}
