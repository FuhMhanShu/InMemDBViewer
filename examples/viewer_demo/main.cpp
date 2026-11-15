// Demo-App (FA-16): bindet InMemDBViewer als Bibliothek ein und zeigt
// Schema-Szene (QGraphicsFramework, FA-04/FA-05), Collection-Baum (FA-03)
// und Dokument-Tabelle mit DSL-Filter (FA-07/FA-10).
// Headless-tauglich: QT_QPA_PLATFORM=offscreen ./viewer_demo (beendet sich nach 300 ms selbst).
#include <memdb/memdb.hpp>
#include <memdbviewer/viewer.hpp>

#include <QAction>
#include <QApplication>
#include <QGuiApplication>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QMainWindow>
#include <QSplitter>
#include <QTimer>
#include <QToolBar>
#include <QTreeView>

using namespace memdb;

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    // Demo-Datenbank: zwei Collections, Schema + Unique-Index, eine Referenz (FA-18)
    Database db("demo");
    Collection users = db.createCollection("users");
    Schema schema;
    schema.add("name", Value::Type::String).notNull();
    schema.add("age", Value::Type::Int);
    users.setSchema(schema);
    users.createIndex("age", true);
    const DocumentId ada = users.insert({{"name", "Ada"}, {"age", 36}});
    users.insert({{"name", "Bob"}, {"age", 17}});
    Collection orders = db.createCollection("orders");
    orders.createIndex("total");
    const DocumentId o1 = orders.insert({{"total", 99.5}, {"status", "offen"}});
    orders.insert({{"total", 12.0}, {"status", "bezahlt"}});
    ReferenceIndex refs(db);
    refs.link("orders", o1, "users", ada, "kunde");

    // Controller (MVC) + Model
    viewer::ViewerController controller;
    controller.setDatabase(db);

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("InMemDBViewer – Demo"));

    auto* mainSplitter = new QSplitter(&window);
    auto* leftSplitter = new QSplitter(Qt::Vertical, mainSplitter);

    auto* tree = new QTreeView(leftSplitter);
    tree->setModel(controller.databaseModel());
    tree->setHeaderHidden(false);
    tree->header()->setStretchLastSection(true);
    leftSplitter->addWidget(tree);

    auto* docView = new viewer::DocumentView(leftSplitter);
    leftSplitter->addWidget(docView);

    auto* scene = new viewer::SchemaScene(mainSplitter);
    scene->setDatabase(db, &refs);
    auto* schemaView = new viewer::SchemaView(mainSplitter);
    schemaView->setSchemaScene(scene);

    mainSplitter->addWidget(leftSplitter);
    mainSplitter->addWidget(schemaView);
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 2);
    window.setCentralWidget(mainSplitter);

    // Selektion im Baum -> Dokument-Tabelle der Collection (nur Top-Level-Zeilen)
    QObject::connect(tree->selectionModel(), &QItemSelectionModel::currentRowChanged,
                     [&](const QModelIndex& current, const QModelIndex&) {
                         if (!current.isValid() || current.parent().isValid())
                             return;
                         const std::string name = current.data(Qt::DisplayRole).toString().toStdString();
                         if (db.hasCollection(name))
                             docView->showCollection(db.collection(name));
                     });
    if (!db.collectionNames().empty())
        docView->showCollection(db.collection(db.collectionNames().front()));

    // Live-Update (FA-08): Observer-Event -> Szene neu aufbauen
    QObject::connect(&controller, &viewer::ViewerController::collectionChanged, scene,
                     &viewer::SchemaScene::rebuild);
    auto* toolBar = window.addToolBar(QStringLiteral("Demo"));
    auto* insertAction = toolBar->addAction(QStringLiteral("Insert in orders (live)"));
    QObject::connect(insertAction, &QAction::triggered, [&] {
        static int n = 0;
        orders.insert({{"total", 1.0 * ++n}, {"status", "neu"}});
    });

    window.resize(1100, 650);
    window.show();

    // Headless-Smoke-Test (CI): offscreen nach 300 ms beenden
    if (QGuiApplication::platformName() == QLatin1String("offscreen"))
        QTimer::singleShot(300, &app, &QCoreApplication::quit);

    return app.exec();
}
