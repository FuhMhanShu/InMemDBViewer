// InMemDBViewer-App (GA-01/GA-03): Shell mit uebergreifendem Datenladen oben
// (Menue/Toolbar/Drag & Drop -> DataInputWidget) und zwei Bereichen darunter:
// "Datenbetrachtung" (DataViewWidget, eigener Branch) und "Analyse" (AnalysisWidget,
// eigener Branch). Alle Widgets haengen am gemeinsamen ViewerCore.
// Headless (CI-Smoke): QT_QPA_PLATFORM=offscreen ./viewer_app laedt Demo-Daten und
// beendet sich nach 300 ms selbst mit Exit-Code 0.
#include <memdbviewer/viewer.hpp>

#include <QAction>
#include <QApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QGuiApplication>
#include <QGroupBox>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenuBar>
#include <QMimeData>
#include <QSplitter>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>

using namespace memdb::viewer;

namespace {
class MainWindow : public QMainWindow {
public:
    explicit MainWindow(ViewerCore* core, QWidget* parent = nullptr) : QMainWindow(parent), core_(core) {
        setAcceptDrops(true);
        setWindowTitle(QStringLiteral("InMemDBViewer"));

        inputWidget_ = new DataInputWidget(this);
        inputWidget_->setCore(core_);

        auto* central = new QSplitter(Qt::Vertical, this);
        central->addWidget(inputWidget_);

        viewWidget_ = new DataViewWidget(this);
        viewWidget_->setCore(core_);

        auto* areas = new QSplitter(Qt::Horizontal, central);
        auto* viewBox = new QGroupBox(QStringLiteral("Datenbetrachtung"), areas);
        auto* viewLayout = new QVBoxLayout(viewBox);
        viewLayout->setContentsMargins(2, 2, 2, 2);
        viewLayout->addWidget(viewWidget_);
        analysisWidget_ = new AnalysisWidget(this);
        analysisWidget_->setCore(core_);
        auto* analysisBox = new QGroupBox(QStringLiteral("Analyse (Discovery + Analyse)"), areas);
        auto* analysisLayout = new QVBoxLayout(analysisBox);
        analysisLayout->setContentsMargins(2, 2, 2, 2);
        analysisLayout->addWidget(analysisWidget_);
        areas->addWidget(viewBox);
        areas->addWidget(analysisBox);
        areas->setStretchFactor(0, 2);
        areas->setStretchFactor(1, 1);

        // Selektion aus der Datenbetrachtung als Analyse-Umfang uebernehmen
        connect(viewWidget_, &DataViewWidget::collectionSelected, analysisWidget_,
                &AnalysisWidget::setScopeCollection);
        central->addWidget(areas);
        central->setStretchFactor(0, 0);
        central->setStretchFactor(1, 1);
        setCentralWidget(central);

        buildMenuAndToolbar();

        connect(inputWidget_, &DataInputWidget::statusMessage, this,
                [this](const QString& msg) { statusBar()->showMessage(msg, 10000); });
        connect(core_, &ViewerCore::databaseLoaded, this, [this](const QString& desc) {
            statusBar()->showMessage(desc, 10000);
            setWindowTitle(QStringLiteral("InMemDBViewer – %1").arg(desc));
        });
        connect(core_, &ViewerCore::lastErrorChanged, this, [this](const QString& msg) {
            if (!msg.isEmpty())
                statusBar()->showMessage(QStringLiteral("Fehler: %1").arg(msg), 10000);
        });
        connect(viewWidget_, &DataViewWidget::statusMessage, this,
                [this](const QString& msg) { statusBar()->showMessage(msg, 10000); });
        connect(analysisWidget_, &AnalysisWidget::statusMessage, this,
                [this](const QString& msg) { statusBar()->showMessage(msg, 10000); });
        // F5: Analyse starten
        auto* analyzeAction = new QAction(QStringLiteral("Analysieren"), this);
        analyzeAction->setShortcut(QKeySequence(QStringLiteral("F5")));
        connect(analyzeAction, &QAction::triggered, analysisWidget_, &AnalysisWidget::runAnalysis);
        addAction(analyzeAction);
    }

    void handleDroppedFiles(const QStringList& paths) { inputWidget_->handleDroppedFiles(paths); }

    DataViewWidget* viewWidget() const { return viewWidget_; }
    AnalysisWidget* analysisWidget() const { return analysisWidget_; }
    bool exportStructurePng(const QString& path) { return viewWidget_->exportStructurePng(path); }

protected:
    void dragEnterEvent(QDragEnterEvent* event) override {
        if (event->mimeData()->hasUrls())
            event->acceptProposedAction();
    }
    void dropEvent(QDropEvent* event) override {
        QStringList paths;
        for (const QUrl& url : event->mimeData()->urls())
            if (url.isLocalFile())
                paths << url.toLocalFile();
        if (!paths.isEmpty()) {
            handleDroppedFiles(paths);
            event->acceptProposedAction();
        }
    }

private:
    void buildMenuAndToolbar() {
        auto* fileMenu = menuBar()->addMenu(QStringLiteral("&Datei"));
        auto* openAction = fileMenu->addAction(QStringLiteral("Daten oeffnen…"), this, [this] {
            inputWidget_->openFileDialog();
        });
        openAction->setShortcut(QKeySequence::Open);
        auto* openSnapAction = fileMenu->addAction(QStringLiteral("Snapshot oeffnen…"), this, [this] {
            const QString path = QFileDialog::getOpenFileName(
                this, QStringLiteral("InMemDB-Snapshot oeffnen"), QString(),
                QStringLiteral("InMemDB-Snapshot (*.json *.inmemdb.json *.enc);;Alle Dateien (*)"));
            if (!path.isEmpty())
                inputWidget_->handleDroppedFiles({path});
        });
        auto* newAction = fileMenu->addAction(QStringLiteral("Neue leere Database"), this, [this] {
            core_->resetDatabase();
        });
        newAction->setShortcut(QKeySequence::New);
        auto* demoAction = fileMenu->addAction(QStringLiteral("Beispiel laden"), this, [this] {
            core_->loadDemoData();
        });
        fileMenu->addSeparator();
        auto* quitAction = fileMenu->addAction(QStringLiteral("Beenden"), this, &QWidget::close);
        quitAction->setShortcut(QKeySequence::Quit);

        auto* toolBar = addToolBar(QStringLiteral("Daten"));
        toolBar->addAction(openAction);
        toolBar->addAction(openSnapAction);
        toolBar->addAction(newAction);
        toolBar->addAction(demoAction);
    }

    ViewerCore* core_;
    DataInputWidget* inputWidget_ = nullptr;
    DataViewWidget* viewWidget_ = nullptr;
    AnalysisWidget* analysisWidget_ = nullptr;
};
} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    const bool offscreen = QGuiApplication::platformName() == QLatin1String("offscreen");

    ViewerCore core;
    MainWindow window(&core);
    core.loadDemoData();
    window.resize(1200, 750);
    window.show();

    if (offscreen) {
        // CI-Smoke (GA-03): Struktur-Szene rendern + Analyse ausfuehren, dann beenden
        QTemporaryDir dir;
        if (dir.isValid())
            window.exportStructurePng(dir.path() + QStringLiteral("/structure.png"));
        QObject::connect(window.analysisWidget(), &AnalysisWidget::analysisFinished, &app,
                         [&](bool, const QString&) { QTimer::singleShot(100, &app, &QCoreApplication::quit); });
        QTimer::singleShot(0, window.analysisWidget(), &AnalysisWidget::runAnalysis);
        QTimer::singleShot(20000, &app, &QCoreApplication::quit); // Safety
    }

    return app.exec();
}
