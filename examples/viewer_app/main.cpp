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

        auto* areas = new QSplitter(Qt::Horizontal, central);
        auto* viewBox = new QGroupBox(QStringLiteral("Datenbetrachtung"), areas);
        auto* viewLayout = new QVBoxLayout(viewBox);
        viewLayout->addWidget(new QLabel(
            QStringLiteral("DataViewWidget (Struktur/Tabelle/Dokument) – eigener Umsetzungs-Branch"), viewBox));
        auto* analysisBox = new QGroupBox(QStringLiteral("Analyse (Discovery + Analyse)"), areas);
        auto* analysisLayout = new QVBoxLayout(analysisBox);
        analysisLayout->addWidget(new QLabel(
            QStringLiteral("AnalysisWidget – eigener Umsetzungs-Branch"), analysisBox));
        areas->addWidget(viewBox);
        areas->addWidget(analysisBox);
        areas->setStretchFactor(0, 1);
        areas->setStretchFactor(1, 1);
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
    }

    void handleDroppedFiles(const QStringList& paths) { inputWidget_->handleDroppedFiles(paths); }

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
        auto* openAction = fileMenu->addAction(QStringLiteral("Daten oeffnen…"), QKeySequence::Open, this, [this] {
            inputWidget_->openFileDialog();
        });
        auto* openSnapAction = fileMenu->addAction(QStringLiteral("Snapshot oeffnen…"), this, [this] {
            const QString path = QFileDialog::getOpenFileName(
                this, QStringLiteral("InMemDB-Snapshot oeffnen"), QString(),
                QStringLiteral("InMemDB-Snapshot (*.json *.inmemdb.json *.enc);;Alle Dateien (*)"));
            if (!path.isEmpty())
                inputWidget_->handleDroppedFiles({path});
        });
        auto* newAction = fileMenu->addAction(QStringLiteral("Neue leere Database"), QKeySequence::New, this, [this] {
            core_->resetDatabase();
        });
        auto* demoAction = fileMenu->addAction(QStringLiteral("Beispiel laden"), this, [this] {
            core_->loadDemoData();
        });
        fileMenu->addSeparator();
        fileMenu->addAction(QStringLiteral("Beenden"), QKeySequence::Quit, this, &QWidget::close);

        auto* toolBar = addToolBar(QStringLiteral("Daten"));
        toolBar->addAction(openAction);
        toolBar->addAction(openSnapAction);
        toolBar->addAction(newAction);
        toolBar->addAction(demoAction);
    }

    ViewerCore* core_;
    DataInputWidget* inputWidget_ = nullptr;
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

    if (offscreen)
        QTimer::singleShot(300, &app, &QCoreApplication::quit);

    return app.exec();
}
