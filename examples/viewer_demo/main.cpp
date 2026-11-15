// Demo-App (FA-16): bindet InMemDBViewer als Bibliothek ein und zeigt
// Schema-Szene (QGraphicsFramework, FA-04/FA-05/FA-06), Collection-Baum (FA-03),
// Dokument-Tabelle mit DSL-Filter (FA-07/FA-10) und Metrik-Panel (FA-11/FA-13).
// Toolbar: Live-Insert (FA-08), Snapshot oeffnen (FA-12), Export PNG/Mermaid (FA-15).
// Daten laden (FA-12): Snapshot per Datei-Dialog oder Drag & Drop auf das Fenster
// (verschluesselte Snapshots werden am Magic-Header erkannt, Passwort wird abgefragt).
// Headless-tauglich: QT_QPA_PLATFORM=offscreen ./viewer_demo (rendert PNG in ein
// Temp-Dir und beendet sich nach 300 ms selbst).
#include <memdb/memdb.hpp>
#include <memdbviewer/viewer.hpp>

#include <QAction>
#include <QApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMimeData>
#include <QMessageBox>
#include <QStatusBar>
#include <QSplitter>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolBar>
#include <QTreeView>
#include <QUrl>

#include <functional>
#include <memory>

using namespace memdb;

/// @brief true, wenn die Datei mit dem Magic-Header verschluesselter Snapshots
///        ("INMEMDB" + 0x01) beginnt (FA-19) – dann Passwort-Abfrage (FA-12).
bool isEncryptedSnapshot(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    return f.read(8) == QByteArray("INMEMDB\x01", 8);
}

/// @brief Hauptfenster der Demo: nimmt lokale Dateien per Drag & Drop an (FA-12).
class DemoWindow : public QMainWindow {
public:
    explicit DemoWindow(QWidget* parent = nullptr) : QMainWindow(parent) { setAcceptDrops(true); }

    /// @brief Callback fuer eine per Drag & Drop abgelegte lokale Datei.
    std::function<void(const QString&)> onFileDropped;

protected:
    void dragEnterEvent(QDragEnterEvent* event) override {
        const QList<QUrl> urls = event->mimeData()->urls();
        if (event->mimeData()->hasUrls() && !urls.isEmpty() && urls.first().isLocalFile())
            event->acceptProposedAction();
    }
    void dropEvent(QDropEvent* event) override {
        const QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty() && urls.first().isLocalFile()) {
            if (onFileDropped)
                onFileDropped(urls.first().toLocalFile());
            event->acceptProposedAction();
        }
    }
};

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    const bool offscreen = QGuiApplication::platformName() == QLatin1String("offscreen");

    // Demo-Datenbank: zwei Collections, Schema (inkl. Unter-Schema) + Unique-Index, Referenz (FA-18)
    Database db("demo");
    Collection users = db.createCollection("users");
    Schema sub;
    sub.add("plz", Value::Type::String);
    Schema schema;
    schema.add("name", Value::Type::String).notNull();
    schema.add("age", Value::Type::Int);
    schema.add("adresse", Value::Type::Document).subSchema(sub);
    users.setSchema(schema);
    users.createIndex("age", true);
    const DocumentId ada = users.insert({{"name", "Ada"},
                                         {"age", 36},
                                         {"adresse", Value(std::make_shared<Document>(
                                                         Document{{"plz", "10115"}}))}});
    users.insert({{"name", "Bob"}, {"age", 17}});
    Collection orders = db.createCollection("orders");
    orders.createIndex("total");
    const DocumentId o1 = orders.insert({{"total", 99.5}, {"status", "offen"}});
    orders.insert({{"total", 12.0}, {"status", "bezahlt"}});

    // Controller (MVC) + Model
    viewer::ViewerController controller;
    controller.setDatabase(db);
    auto refs = std::make_unique<ReferenceIndex>(db);
    refs->link("orders", o1, "users", ada, "kunde");

    DemoWindow window;
    window.setWindowTitle(QStringLiteral("InMemDBViewer – Demo"));

    auto* mainSplitter = new QSplitter(&window);
    auto* leftSplitter = new QSplitter(Qt::Vertical, mainSplitter);

    auto* tree = new QTreeView(leftSplitter);
    tree->setModel(controller.databaseModel());
    tree->header()->setStretchLastSection(true);
    leftSplitter->addWidget(tree);

    auto* docView = new viewer::DocumentView(leftSplitter);
    leftSplitter->addWidget(docView);

    auto* docTree = new viewer::DocumentTreeView(leftSplitter); // FA-07: Dokument-Detailbaum
    leftSplitter->addWidget(docTree);

    auto* metricsPanel = new viewer::MetricsPanel(leftSplitter);
    leftSplitter->addWidget(metricsPanel);

    auto* scene = new viewer::SchemaScene(mainSplitter);
    auto* schemaView = new viewer::SchemaView(mainSplitter);
    mainSplitter->addWidget(leftSplitter);
    mainSplitter->addWidget(schemaView);
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 2);
    window.setCentralWidget(mainSplitter);

    auto rebuildScene = [&] {
        if (controller.database())
            scene->setDatabase(*controller.database(), refs.get());
        schemaView->setSchemaScene(scene);
    };
    rebuildScene();

    const auto currentDb = [&]() -> Database& { return *controller.database(); };
    QString currentCollection;
    const auto showCollection = [&](const QString& name) {
        if (!controller.database() || !currentDb().hasCollection(name.toStdString()))
            return;
        currentCollection = name;
        docView->showCollection(currentDb().collection(name.toStdString()));
        metricsPanel->showCollection(currentDb().collection(name.toStdString()));
    };
    const auto findTopLevel = [&](const QString& name) {
        auto* m = controller.databaseModel();
        for (int i = 0; i < m->rowCount(); ++i) {
            QModelIndex idx = m->index(i, 0);
            if (idx.data().toString() == name)
                return idx;
        }
        return QModelIndex();
    };

    // FA-09: Selektion bidirektional syncen (Baum <-> Szene)
    QObject::connect(tree->selectionModel(), &QItemSelectionModel::currentRowChanged,
                     [&](const QModelIndex& current, const QModelIndex&) {
                         if (current.isValid() && !current.parent().isValid())
                             showCollection(current.data().toString());
                     });
    QObject::connect(schemaView, &viewer::SchemaView::collectionClicked, &controller,
                     &viewer::ViewerController::selectCollection);
    QObject::connect(&controller, &viewer::ViewerController::collectionSelected, [&](const QString& name) {
        showCollection(name);
        if (const QModelIndex idx = findTopLevel(name); idx.isValid())
            tree->selectionModel()->select(
                idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
    });

    // FA-07: Klick auf Dokument-Zeile -> Detailbaum (verschachtelte Dokumente/Arrays)
    QObject::connect(docView, &viewer::DocumentView::documentSelected, [&](quint64 id) {
        if (currentCollection.isEmpty() || !controller.database())
            return;
        auto doc = currentDb().collection(currentCollection.toStdString()).findById(DocumentId(id));
        if (doc)
            docTree->showDocument(*doc);
        else
            docTree->clearDocument();
    });

    // FA-08: Live-Update – Observer-Event -> Model (im Controller), Szene und Metriken
    QObject::connect(&controller, &viewer::ViewerController::collectionChanged, scene,
                     &viewer::SchemaScene::rebuild);
    QObject::connect(&controller, &viewer::ViewerController::collectionChanged, metricsPanel,
                     &viewer::MetricsPanel::refresh);
    auto* refreshTimer = new QTimer(&window);
    refreshTimer->setInterval(1000);
    QObject::connect(refreshTimer, &QTimer::timeout, metricsPanel, &viewer::MetricsPanel::refresh);
    refreshTimer->start();

    // FA-12: nach Snapshot-Wechsel Szene/Referenzen auf die neue Database umbiegen
    QObject::connect(&controller, &viewer::ViewerController::databaseChanged, [&] {
        if (controller.database())
            refs = std::make_unique<ReferenceIndex>(currentDb());
        rebuildScene();
        if (controller.database() && !currentDb().collectionNames().empty())
            showCollection(QString::fromStdString(currentDb().collectionNames().front()));
    });

    // FA-12: Snapshot laden – gemeinsamer Pfad fuer Datei-Dialog und Drag & Drop.
    // Verschluesselte Snapshots werden am Magic-Header erkannt (Passwort-Abfrage),
    // Fehler landen in einer MessageBox mit lastError().
    const auto loadSnapshotFile = [&](const QString& path) {
        if (path.isEmpty())
            return;
        bool ok = false;
        if (isEncryptedSnapshot(path)) {
            QString password;
            if (!offscreen) {
                bool entered = false;
                password = QInputDialog::getText(&window, QStringLiteral("Verschluesselter Snapshot"),
                                                 QStringLiteral("Passwort:"), QLineEdit::Password,
                                                 QString(), &entered);
                if (!entered || password.isEmpty())
                    return;
            }
            ok = controller.openSnapshotEncrypted(path, password);
        } else {
            ok = controller.openSnapshot(path);
        }
        if (!ok) {
            if (!offscreen)
                QMessageBox::warning(&window, QStringLiteral("Snapshot laden"),
                                     QStringLiteral("Laden fehlgeschlagen:\n%1")
                                         .arg(controller.lastError()));
            return;
        }
        window.setWindowTitle(
            QStringLiteral("InMemDBViewer – Demo – %1").arg(QFileInfo(path).fileName()));
    };
    window.onFileDropped = loadSnapshotFile;
    window.statusBar()->showMessage(
        QStringLiteral("Snapshot-Datei per Drag & Drop auf das Fenster ziehen oder „Snapshot "
                       "öffnen…“ in der Toolbar klicken"));

    // Toolbar: Live-Insert, Snapshot oeffnen (FA-12), Export (FA-15)
    auto* toolBar = window.addToolBar(QStringLiteral("Demo"));
    auto* insertAction = toolBar->addAction(QStringLiteral("Insert in orders (live)"));
    QObject::connect(insertAction, &QAction::triggered, [&] {
        static int n = 0;
        if (currentDb().hasCollection("orders"))
            currentDb().collection("orders").insert({{"total", 1.0 * ++n}, {"status", "neu"}});
    });
    auto* openAction = toolBar->addAction(QStringLiteral("Snapshot öffnen…"));
    QObject::connect(openAction, &QAction::triggered, [&] {
        if (offscreen)
            return;
        const QString path = QFileDialog::getOpenFileName(
            &window, QStringLiteral("Snapshot öffnen"), QString(),
            QStringLiteral("InMemDB Snapshot (*.json *.enc);;Alle Dateien (*)"));
        if (!path.isEmpty())
            loadSnapshotFile(path);
    });
    auto* pngAction = toolBar->addAction(QStringLiteral("Export PNG…"));
    QObject::connect(pngAction, &QAction::triggered, [&] {
        const QString path = offscreen ? QString() : QFileDialog::getSaveFileName(&window, QStringLiteral("Szene als PNG exportieren"),
                                                          QString(), QStringLiteral("PNG (*.png)"));
        if (!path.isEmpty())
            scene->exportPng(path);
    });
    auto* mmdAction = toolBar->addAction(QStringLiteral("Export Mermaid…"));
    QObject::connect(mmdAction, &QAction::triggered, [&] {
        if (offscreen)
            return;
        const QString path = QFileDialog::getSaveFileName(&window, QStringLiteral("Graph als Mermaid exportieren"),
                                                          QString(), QStringLiteral("Mermaid (*.mmd *.md)"));
        if (path.isEmpty())
            return;
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
            f.write(scene->mermaid().toUtf8());
    });

    if (!currentDb().collectionNames().empty())
        showCollection(QString::fromStdString(currentDb().collectionNames().front()));

    window.resize(1100, 650);
    window.show();

    // Headless-Smoke-Test (CI): Szene offscreen als PNG rendern, dann nach 300 ms beenden
    if (offscreen) {
        QTemporaryDir dir;
        if (dir.isValid())
            scene->exportPng(dir.path() + QStringLiteral("/scene.png"));
        QTimer::singleShot(300, &app, &QCoreApplication::quit);
    }

    return app.exec();
}
