// InMemDBViewer-App (GA-01/GA-03, UI-01..UI-06): schlanke Shell mit zentralem Theme
// (applyViewerTheme): oben einklappbares Dock "Daten laden" (DataInputWidget),
// in der Mitte Tabs statt breiter Spalten (Datenbetrachtung / Datenbank-Design / Analyse),
// unten einklappbares Ausgabe-Dock mit Fehler-Zaehler. Drag & Drop ueberall im Fenster.
// Headless (CI-Smoke): QT_QPA_PLATFORM=offscreen ./viewer_app laedt Demo-Daten, rendert die
// Struktur-Szene, fuehrt eine Analyse aus und beendet sich selbst mit Exit-Code 0.
#include <memdbviewer/viewer.hpp>

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QGuiApplication>
#include <QInputDialog>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenuBar>
#include <QMimeData>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolBar>
#include <QUrl>

using namespace memdb::viewer;

namespace {
class MainWindow : public QMainWindow {
public:
    explicit MainWindow(ViewerCore* core, QWidget* parent = nullptr) : QMainWindow(parent), core_(core) {
        setAcceptDrops(true);
        setWindowTitle(QStringLiteral("InMemDBViewer"));
        setMinimumSize(940, 600);

        // oben: Dateneingabe als einklappbares Dock (spart Platz, bleibt per Drag & Drop erreichbar)
        inputWidget_ = new DataInputWidget(this);
        inputWidget_->setCore(core_);
        inputDock_ = new QDockWidget(QStringLiteral("Daten laden"), this);
        inputDock_->setObjectName(QStringLiteral("inputDock"));
        inputDock_->setWidget(inputWidget_);
        inputDock_->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable |
                                QDockWidget::DockWidgetClosable);
        addDockWidget(Qt::TopDockWidgetArea, inputDock_);

        // Mitte: Tabs (Datenbetrachtung / Datenbank-Design / Analyse) statt drei Spalten
        viewWidget_ = new DataViewWidget(this);
        viewWidget_->setCore(core_);
        designWidget_ = new DatabaseDesignWidget(this);
        designWidget_->setCore(core_);
        analysisWidget_ = new AnalysisWidget(this);
        analysisWidget_->setCore(core_);
        auto* tabs = new QTabWidget(this);
        tabs->setObjectName(QStringLiteral("areaTabs"));
        tabs->addTab(viewWidget_, QStringLiteral("Datenbetrachtung"));
        tabs->addTab(designWidget_, QStringLiteral("Datenbank-Design"));
        tabs->addTab(analysisWidget_, QStringLiteral("Analyse (Discovery + Analyse)"));
        setCentralWidget(tabs);

        // unten: Ausgabe als einklappbares Dock; Titel zeigt Fehler-Zaehler
        outputPanel_ = new OutputPanel(this);
        outputPanel_->setCore(core_);
        outputDock_ = new QDockWidget(QStringLiteral("Ausgabe"), this);
        outputDock_->setObjectName(QStringLiteral("outputDock"));
        outputDock_->setWidget(outputPanel_);
        outputDock_->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable |
                                 QDockWidget::DockWidgetClosable);
        addDockWidget(Qt::BottomDockWidgetArea, outputDock_);
        connect(core_, &ViewerCore::logged, this, [this](const QString&, bool error) {
            if (error) {
                ++errorCount_;
                outputDock_->setWindowTitle(QStringLiteral("Ausgabe (%1 Fehler)").arg(errorCount_));
            }
        });

        // Selektion aus der Datenbetrachtung als Analyse-Umfang uebernehmen
        connect(viewWidget_, &DataViewWidget::collectionSelected, analysisWidget_,
                &AnalysisWidget::setScopeCollection);

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
        connect(designWidget_, &DatabaseDesignWidget::statusMessage, this,
                [this](const QString& msg) { statusBar()->showMessage(msg, 10000); });
        connect(analysisWidget_, &AnalysisWidget::statusMessage, this,
                [this](const QString& msg) { statusBar()->showMessage(msg, 10000); });

        statusBar()->showMessage(
            QStringLiteral("Dateien per Drag & Drop ins Fenster ziehen oder Datei → Daten öffnen… (Strg+O)"));
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
    QIcon icon(QStyle::StandardPixmap sp) const { return style()->standardIcon(sp); }

    void buildMenuAndToolbar() {
        auto* fileMenu = menuBar()->addMenu(QStringLiteral("&Datei"));
        auto* openAction = fileMenu->addAction(icon(QStyle::SP_DialogOpenButton),
                                               QStringLiteral("Daten öffnen…"), this, [this] {
                                                   inputWidget_->openFileDialog();
                                               });
        openAction->setShortcut(QKeySequence::Open);
        auto* openSnapAction = fileMenu->addAction(icon(QStyle::SP_DialogOpenButton),
                                                   QStringLiteral("Snapshot öffnen…"), this, [this] {
            const QString path = QFileDialog::getOpenFileName(
                this, QStringLiteral("InMemDB-Snapshot öffnen"), QString(),
                QStringLiteral("InMemDB-Snapshot (*.json *.inmemdb.json *.enc);;Alle Dateien (*)"));
            if (!path.isEmpty())
                inputWidget_->handleDroppedFiles({path});
        });
        auto* csvAction = fileMenu->addAction(icon(QStyle::SP_FileDialogListView),
                                              QStringLiteral("CSV aus Zwischenablage…"), this, [this] {
                                                  inputWidget_->importCsvFromClipboard();
                                              });
        csvAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+V")));
        auto* newAction = fileMenu->addAction(icon(QStyle::SP_FileIcon), QStringLiteral("Neue leere Database…"),
                                              this, [this] {
            bool ok = false;
            const QString name = QInputDialog::getText(this, QStringLiteral("Neue Database"),
                                                       QStringLiteral("Name der Database:"), QLineEdit::Normal,
                                                       QStringLiteral("neu"), &ok);
            if (ok && !name.isEmpty())
                core_->resetDatabase(name);
        });
        newAction->setShortcut(QKeySequence::New);
        auto* demoAction = fileMenu->addAction(icon(QStyle::SP_ComputerIcon), QStringLiteral("Beispiel laden"),
                                               this, [this] { core_->loadDemoData(); });
        auto* saveAction = fileMenu->addAction(icon(QStyle::SP_DialogSaveButton),
                                               QStringLiteral("Snapshot speichern…"), this, [this] {
            const QString path = QFileDialog::getSaveFileName(
                this, QStringLiteral("Snapshot speichern"), QString(),
                QStringLiteral("InMemDB-Snapshot (*.inmemdb.json *.json);;Alle Dateien (*)"));
            if (!path.isEmpty()) {
                QString err;
                if (!core_->saveSnapshot(path, &err))
                    core_->log(QStringLiteral("Snapshot speichern fehlgeschlagen: %1").arg(err), true);
            }
        });
        auto* saveEncAction = fileMenu->addAction(icon(QStyle::SP_DialogSaveButton),
                                                  QStringLiteral("Snapshot verschlüsselt speichern…"), this, [this] {
            const QString path = QFileDialog::getSaveFileName(
                this, QStringLiteral("Verschlüsselten Snapshot speichern"), QString(),
                QStringLiteral("Verschlüsselter Snapshot (*.enc);;Alle Dateien (*)"));
            if (path.isEmpty())
                return;
            bool ok = false;
            const QString pw = QInputDialog::getText(this, QStringLiteral("Verschlüsselter Snapshot"),
                                                     QStringLiteral("Passwort:"), QLineEdit::Password, QString(), &ok);
            if (!ok || pw.isEmpty())
                return;
            QString err;
            if (!core_->saveSnapshotEncrypted(path, pw, &err))
                core_->log(QStringLiteral("Verschlüsselten Snapshot speichern fehlgeschlagen: %1").arg(err), true);
        });
        fileMenu->addSeparator();
        auto* quitAction = fileMenu->addAction(QStringLiteral("Beenden"), this, &QWidget::close);
        quitAction->setShortcut(QKeySequence::Quit);

        auto* viewMenu = menuBar()->addMenu(QStringLiteral("&Ansicht"));
        viewMenu->addAction(inputDock_->toggleViewAction());
        viewMenu->addAction(outputDock_->toggleViewAction());

        auto* analyzeAction = new QAction(icon(QStyle::SP_MediaPlay), QStringLiteral("Analysieren"), this);
        analyzeAction->setShortcut(QKeySequence(QStringLiteral("F5")));
        connect(analyzeAction, &QAction::triggered, analysisWidget_, &AnalysisWidget::runAnalysis);
        addAction(analyzeAction);

        auto* toolBar = addToolBar(QStringLiteral("Daten"));
        toolBar->setMovable(false);
        toolBar->addAction(openAction);
        toolBar->addAction(openSnapAction);
        toolBar->addAction(csvAction);
        toolBar->addSeparator();
        toolBar->addAction(newAction);
        toolBar->addAction(demoAction);
        toolBar->addAction(saveAction);
        toolBar->addSeparator();
        toolBar->addAction(analyzeAction);
    }

    ViewerCore* core_;
    DataInputWidget* inputWidget_ = nullptr;
    DataViewWidget* viewWidget_ = nullptr;
    AnalysisWidget* analysisWidget_ = nullptr;
    OutputPanel* outputPanel_ = nullptr;
    DatabaseDesignWidget* designWidget_ = nullptr;
    QDockWidget* inputDock_ = nullptr;
    QDockWidget* outputDock_ = nullptr;
    int errorCount_ = 0;
};
} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    applyViewerTheme(&app); // UI-01: zentrales Theme (Fusion + helle Palette + Stylesheet)
    const bool offscreen = QGuiApplication::platformName() == QLatin1String("offscreen");

    ViewerCore core;
    MainWindow window(&core);
    core.loadDemoData();
    window.resize(1180, 820);
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
