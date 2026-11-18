#include "memdbviewer/data_view_widget.hpp"
#include "memdbviewer/viewer_core.hpp"

#include "data_view_controller.hpp"
#include "database_model.hpp"
#include "document_table_model.hpp"
#include "document_tree_model.hpp"
#include "graph_scene.hpp"

#include <memdb/aggregate.hpp>
#include <memdb/memdb.hpp>

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QGuiApplication>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QStyle>
#include <QToolButton>
#include "tool_button.hpp"

#include <thread>
#include <QSplitter>
#include <QTabWidget>
#include <QTableView>
#include <QTextBrowser>
#include <QTimer>
#include <QTreeView>
#include <QVBoxLayout>

namespace memdb::viewer {

struct DataViewWidget::Impl {
    ViewerCore* core = nullptr;
    detail::DatabaseModel* dbModel = nullptr;
    detail::DocumentTableModel* tableModel = nullptr;
    detail::DocumentTreeModel* treeModel = nullptr;
    detail::GraphScene* scene = nullptr;
    detail::DataViewController* controller = nullptr;

    QTreeView* dbTree = nullptr;
    detail::GraphView* graphView = nullptr;
    QTableView* tableView = nullptr;
    QTreeView* docTree = nullptr;
    QLineEdit* queryEdit = nullptr;
    QLabel* diagLabel = nullptr;
    QTextBrowser* metricsBox = nullptr;
    QComboBox* aggOpBox = nullptr;
    QComboBox* aggFieldBox = nullptr;
    QLabel* aggResultLabel = nullptr;
};

DataViewWidget::DataViewWidget(QWidget* parent) : QWidget(parent), impl_(std::make_unique<Impl>()) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(6, 6, 6, 6);

    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // Linke Seite: Datenbank-Uebersicht + Metriken
    auto* leftBox = new QGroupBox(QStringLiteral("Database"), splitter);
    auto* leftLayout = new QVBoxLayout(leftBox);
    impl_->dbModel = new detail::DatabaseModel(this);
    impl_->dbTree = new QTreeView(leftBox);
    impl_->dbTree->setModel(impl_->dbModel);
    impl_->dbTree->setHeaderHidden(false);
    impl_->dbTree->header()->setStretchLastSection(true);
    impl_->dbTree->setSelectionMode(QAbstractItemView::SingleSelection);
    impl_->dbTree->setRootIsDecorated(false);
    leftLayout->addWidget(impl_->dbTree, 3);
    auto* metricsBox = new QGroupBox(QStringLiteral("Metriken / Hybrid"), leftBox);
    auto* metricsLayout = new QVBoxLayout(metricsBox);
    impl_->metricsBox = new QTextBrowser(metricsBox);
    impl_->metricsBox->setOpenExternalLinks(false);
    metricsLayout->addWidget(impl_->metricsBox);
    leftLayout->addWidget(metricsBox, 2);
    splitter->addWidget(leftBox);

    // Rechte Seite: Tabs Struktur / Tabelle / Dokument
    auto* tabs = new QTabWidget(splitter);

    // Tab 1: Struktur (QGraphicsFramework)
    auto* structWidget = new QWidget(tabs);
    auto* structLayout = new QVBoxLayout(structWidget);
    structLayout->setContentsMargins(0, 0, 0, 0);
    auto* structBar = new QHBoxLayout();
    auto* pngBtn = detail::makeToolButton(structWidget, QStyle::SP_DialogSaveButton, QStringLiteral("PNG exportieren…"));
    auto* svgBtn = detail::makeToolButton(structWidget, QStyle::SP_DialogSaveButton, QStringLiteral("SVG exportieren…"));
    auto* mmdBtn = detail::makeToolButton(structWidget, QStyle::SP_DialogSaveButton, QStringLiteral("Mermaid exportieren…"));
    auto* summaryLabel = new QLabel(structWidget);
    structBar->addWidget(pngBtn);
    structBar->addWidget(svgBtn);
    structBar->addWidget(mmdBtn);
    structBar->addStretch();
    structBar->addWidget(summaryLabel);
    structLayout->addLayout(structBar);
    impl_->scene = new detail::GraphScene(structWidget);
    impl_->graphView = new detail::GraphView(structWidget);
    impl_->graphView->setGraphScene(impl_->scene);
    structLayout->addWidget(impl_->graphView, 1);
    tabs->addTab(structWidget, QStringLiteral("Struktur"));

    // Tab 2: Tabelle
    auto* tableWidget = new QWidget(tabs);
    auto* tableLayout = new QVBoxLayout(tableWidget);
    tableLayout->setContentsMargins(0, 0, 0, 0);
    auto* filterBar = new QHBoxLayout();
    filterBar->addWidget(new QLabel(QStringLiteral("Filter (DSL):"), tableWidget));
    impl_->queryEdit = new QLineEdit(tableWidget);
    impl_->queryEdit->setPlaceholderText(QStringLiteral("z. B. age >= 18 && name.contains('a')"));
    impl_->queryEdit->setClearButtonEnabled(true);
    auto* applyBtn = detail::makeToolButton(tableWidget, QStyle::SP_DialogApplyButton, QStringLiteral("Anwenden"));
    auto* resetBtn = detail::makeToolButton(tableWidget, QStyle::SP_DialogResetButton, QStringLiteral("Zurücksetzen"));
    impl_->diagLabel = new QLabel(tableWidget);
    filterBar->addWidget(impl_->queryEdit, 1);
    filterBar->addWidget(applyBtn);
    filterBar->addWidget(resetBtn);
    filterBar->addWidget(impl_->diagLabel, 1);
    tableLayout->addLayout(filterBar);
    // Bearbeiten-Leiste (DE-05): neues/loeschen Dokument + Hinweis auf Zellen-Edit
    auto* docBar = new QHBoxLayout();
    docBar->addWidget(new QLabel(QStringLiteral("Bearbeiten:"), tableWidget));
    auto* newDocBtn = detail::makeToolButton(tableWidget, QStyle::SP_FileIcon, QStringLiteral("Dokument neu…"));
    newDocBtn->setToolTip(QStringLiteral("Neues Dokument als JSON-Objekt in der aktuellen Collection anlegen (wird geprueft)"));
    auto* delDocBtn = detail::makeToolButton(tableWidget, QStyle::SP_TrashIcon, QStringLiteral("Dokument löschen"));
    delDocBtn->setToolTip(QStringLiteral("Ausgewaehlte Zeile (Dokument) loeschen – mit Sicherheitsabfrage"));
    auto* editHint = new QLabel(
        QStringLiteral("Doppelklick auf eine Zelle ändert den Wert (vom Programm geprüft und geloggt)."), tableWidget);
    editHint->setProperty("dim", true); // dezente Farbe ueber das zentrale Theme
    docBar->addWidget(newDocBtn);
    docBar->addWidget(delDocBtn);
    docBar->addStretch();
    docBar->addWidget(editHint);
    tableLayout->addLayout(docBar);
    impl_->tableModel = new detail::DocumentTableModel(this);
    impl_->tableView = new QTableView(tableWidget);
    impl_->tableView->setModel(impl_->tableModel);
    impl_->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    impl_->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    impl_->tableView->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    impl_->tableView->horizontalHeader()->setStretchLastSection(true);
    tableLayout->addWidget(impl_->tableView, 1);
    connect(newDocBtn, &QToolButton::clicked, this, &DataViewWidget::insertDocumentDialog);
    connect(delDocBtn, &QToolButton::clicked, this, &DataViewWidget::deleteSelectedDocument);
    auto* aggBar = new QHBoxLayout();
    aggBar->addWidget(new QLabel(QStringLiteral("Aggregat:"), tableWidget));
    impl_->aggOpBox = new QComboBox(tableWidget);
    impl_->aggOpBox->addItems({QStringLiteral("count"), QStringLiteral("min"), QStringLiteral("max"),
                               QStringLiteral("sum"), QStringLiteral("avg")});
    impl_->aggFieldBox = new QComboBox(tableWidget);
    impl_->aggFieldBox->setEditable(true);
    impl_->aggResultLabel = new QLabel(tableWidget);
    auto* aggBtn = detail::makeToolButton(tableWidget, QStyle::SP_MediaPlay, QStringLiteral("Berechnen"));
    aggBar->addWidget(impl_->aggOpBox);
    aggBar->addWidget(impl_->aggFieldBox, 1);
    aggBar->addWidget(aggBtn);
    aggBar->addWidget(impl_->aggResultLabel, 1);
    tableLayout->addLayout(aggBar);
    tabs->addTab(tableWidget, QStringLiteral("Tabelle"));

    // Tab 3: Dokument-Detail
    auto* docWidget = new QWidget(tabs);
    auto* docLayout = new QVBoxLayout(docWidget);
    docLayout->setContentsMargins(0, 0, 0, 0);
    impl_->treeModel = new detail::DocumentTreeModel(this);
    impl_->docTree = new QTreeView(docWidget);
    impl_->docTree->setModel(impl_->treeModel);
    impl_->docTree->header()->setStretchLastSection(true);
    docLayout->addWidget(impl_->docTree);
    tabs->addTab(docWidget, QStringLiteral("Dokument"));

    splitter->addWidget(tabs);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    outer->addWidget(splitter);

    // Controller (MVC) verdrahten
    impl_->controller = new detail::DataViewController(nullptr, impl_->dbModel, impl_->tableModel,
                                                       impl_->treeModel, impl_->scene, this);
    connect(impl_->controller, &detail::DataViewController::statusMessage, this, &DataViewWidget::statusMessage);
    connect(impl_->controller, &detail::DataViewController::collectionSelected, this,
            &DataViewWidget::collectionSelected);
    connect(impl_->controller, &detail::DataViewController::documentSelected, this,
            &DataViewWidget::documentSelected);
    connect(impl_->controller, &detail::DataViewController::metricsTextChanged, impl_->metricsBox,
            &QTextBrowser::setHtml);
    // UI-09: Ergebnis der asynchronen Query -> Diagnose/ Aggregate-Felder aktualisieren
    connect(impl_->controller, &detail::DataViewController::queryApplied, this, [this](int, bool, std::size_t) {
        impl_->diagLabel->setText(queryDiagnostics());
        impl_->aggFieldBox->clear();
        for (int c = 1; c < impl_->tableModel->columnCount(); ++c)
            impl_->aggFieldBox->addItem(impl_->tableModel->headerData(c, Qt::Horizontal).toString());
    });

    // Interaktionen
    connect(impl_->dbTree->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex& current, const QModelIndex&) {
                if (current.isValid())
                    impl_->controller->selectCollection(current.data().toString());
            });
    connect(impl_->scene, &detail::GraphScene::collectionClicked, this, [this](const QString& name) {
        impl_->controller->selectCollection(name);
        if (const QModelIndex idx = impl_->dbModel->indexOf(name); idx.isValid())
            impl_->dbTree->selectionModel()->select(idx, QItemSelectionModel::ClearAndSelect |
                                                             QItemSelectionModel::Current);
    });
    connect(impl_->tableView->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex& current, const QModelIndex&) {
                if (current.isValid())
                    impl_->controller->showDocument(
                        impl_->tableModel->data(impl_->tableModel->index(current.row(), 0),
                                                detail::DocumentTableModel::IdRole)
                            .toULongLong());
            });
    connect(applyBtn, &QToolButton::clicked, this, [this] { applyQuery(impl_->queryEdit->text()); });
    connect(impl_->queryEdit, &QLineEdit::returnPressed, this, [this] { applyQuery(impl_->queryEdit->text()); });
    connect(resetBtn, &QToolButton::clicked, this, &DataViewWidget::resetQuery);
    connect(aggBtn, &QToolButton::clicked, this, [this] {
        if (!impl_->core || impl_->controller->currentCollection().isEmpty())
            return;
        auto coll = impl_->core->database().collection(impl_->controller->currentCollection().toStdString());
        const QString op = impl_->aggOpBox->currentText();
        const QString field = impl_->aggFieldBox->currentText();
        memdb::AggOp aggOp = memdb::AggOp::Count;
        if (op == QLatin1String("min")) aggOp = memdb::AggOp::Min;
        else if (op == QLatin1String("max")) aggOp = memdb::AggOp::Max;
        else if (op == QLatin1String("sum")) aggOp = memdb::AggOp::Sum;
        else if (op == QLatin1String("avg")) aggOp = memdb::AggOp::Avg;
        memdb::Expression q;
        if (!impl_->queryEdit->text().isEmpty()) {
            try {
                q = memdb::parseQuery(impl_->queryEdit->text().toStdString());
            } catch (const std::exception& e) {
                impl_->aggResultLabel->setText(QStringLiteral("Query-Fehler: %1").arg(QString::fromUtf8(e.what())));
                return;
            }
        }
        // UI-09: Aggregation im Worker-Thread – UI blockiert nicht
        impl_->aggResultLabel->setText(QStringLiteral("\u2026"));
        std::thread([this, coll, aggOp, field, q] {
            QString result;
            try {
                auto res = coll.aggregate(aggOp, field.toStdString(), q);
                result = res ? QString::fromStdString(memdb::toString(*res)) : QStringLiteral("(leer)");
            } catch (const std::exception& e) {
                result = QStringLiteral("Fehler: %1").arg(QString::fromUtf8(e.what()));
            }
            QMetaObject::invokeMethod(this, [this, result] { impl_->aggResultLabel->setText(result); },
                                      Qt::QueuedConnection);
        }).detach();
    });
    connect(pngBtn, &QToolButton::clicked, this, [this] {
        const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Struktur als PNG"), QString(),
                                                          QStringLiteral("PNG (*.png)"));
        if (!path.isEmpty()) {
            const bool ok = impl_->scene->exportPng(path);
            emit statusMessage(ok ? QStringLiteral("PNG exportiert: %1").arg(path)
                                  : QStringLiteral("PNG-Export fehlgeschlagen."));
        }
    });
    connect(svgBtn, &QToolButton::clicked, this, [this] {
        const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Struktur als SVG"), QString(),
                                                          QStringLiteral("SVG (*.svg)"));
        if (!path.isEmpty()) {
            impl_->scene->exportSvg(path);
            emit statusMessage(QStringLiteral("SVG exportiert: %1").arg(path));
        }
    });
    connect(mmdBtn, &QToolButton::clicked, this, [this] {
        const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Mermaid exportieren"), QString(),
                                                          QStringLiteral("Mermaid (*.mmd *.md)"));
        if (!path.isEmpty()) {
            QFile f(path);
            if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                f.write(impl_->scene->mermaid().toUtf8());
                emit statusMessage(QStringLiteral("Mermaid exportiert: %1").arg(path));
            }
        }
    });
}

DataViewWidget::~DataViewWidget() = default;

void DataViewWidget::setCore(ViewerCore* core) {
    impl_->core = core;
    if (core)
        impl_->controller->setCore(core);
}

ViewerCore* DataViewWidget::core() const { return impl_->core; }

QString DataViewWidget::currentCollection() const { return impl_->controller->currentCollection(); }

int DataViewWidget::graphNodeCount() const { return impl_->scene->nodeCount(); }
int DataViewWidget::graphEdgeCount() const { return impl_->scene->edgeCount(); }
int DataViewWidget::tableRowCount() const { return impl_->tableModel->rowCount(); }
int DataViewWidget::tableColumnCount() const { return impl_->tableModel->columnCount(); }

QString DataViewWidget::queryDiagnostics() const {
    return QStringLiteral("Index: %1, Kandidaten: %2")
        .arg(impl_->tableModel->lastQueryUsedIndex() ? QStringLiteral("ja") : QStringLiteral("nein"))
        .arg(impl_->tableModel->lastQueryCandidateCount());
}

void DataViewWidget::applyQuery(const QString& dsl) {
    if (!impl_->core || impl_->controller->currentCollection().isEmpty())
        return;
    // DSL synchron validieren (billig) -> Fehler sofort anzeigen
    try {
        memdb::parseQuery(dsl.toStdString());
    } catch (const std::exception& e) {
        const QString msg = QString::fromUtf8(e.what());
        impl_->diagLabel->setText(QStringLiteral("Fehler: %1").arg(msg));
        emit statusMessage(QStringLiteral("Query-Fehler: %1").arg(msg));
        return;
    }
    // UI-09: findIds im Worker-Thread – UI blockiert nicht; Ergebnis via queryApplied
    impl_->controller->applyQueryAsync(dsl);
}

void DataViewWidget::resetQuery() {
    impl_->queryEdit->clear();
    impl_->controller->clearQuery();
    impl_->diagLabel->setText(queryDiagnostics());
}

void DataViewWidget::refresh() {
    if (impl_->controller)
        impl_->controller->refreshAll();
}

bool DataViewWidget::exportStructurePng(const QString& path) { return impl_->scene->exportPng(path); }

void DataViewWidget::insertDocumentDialog() {
    if (!impl_->core || impl_->controller->currentCollection().isEmpty()) {
        emit statusMessage(QStringLiteral("Bitte zuerst eine Collection auswählen."));
        return;
    }
    const QString coll = impl_->controller->currentCollection();
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Neues Dokument in „%1“").arg(coll));
    auto* layout = new QVBoxLayout(&dlg);
    layout->addWidget(new QLabel(QStringLiteral("Dokument als JSON-Objekt (Feld: Wert):"), &dlg));
    auto* edit = new QPlainTextEdit(&dlg);
    edit->setPlaceholderText(QStringLiteral("{\"name\": \"Ada\", \"age\": 36}"));
    layout->addWidget(edit, 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const QString json = edit->toPlainText().trimmed();
    if (json.isEmpty())
        return;
    QString err;
    quint64 newId = 0;
    if (!impl_->controller->insertDocument(json, &newId, &err))
        return; // Fehler ist bereits im Ausgabe-Log (rot) + Status
    // neue Zeile selektieren, sobald die Tabelle aktualisiert ist (queued collectionChanged)
    QTimer::singleShot(0, this, [this, newId] {
        for (int r = 0; r < impl_->tableModel->rowCount(); ++r) {
            if (impl_->tableModel->idAt(r) == newId) {
                impl_->tableView->selectRow(r);
                break;
            }
        }
    });
}

void DataViewWidget::deleteSelectedDocument() {
    if (!impl_->core || impl_->controller->currentCollection().isEmpty()) {
        emit statusMessage(QStringLiteral("Bitte zuerst eine Collection auswählen."));
        return;
    }
    const QModelIndex cur = impl_->tableView->currentIndex();
    if (!cur.isValid()) {
        emit statusMessage(QStringLiteral("Bitte zuerst eine Zeile (Dokument) in der Tabelle auswählen."));
        return;
    }
    const quint64 id = impl_->tableModel->idAt(cur.row());
    const QString coll = impl_->controller->currentCollection();
    const bool offscreen = QGuiApplication::platformName() == QLatin1String("offscreen");
    if (!offscreen) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Dokument löschen"),
            QStringLiteral("Dokument #%1 aus „%2“ wirklich löschen?").arg(id).arg(coll));
        if (answer != QMessageBox::Yes)
            return;
    }
    QString err;
    impl_->controller->removeDocument(id, &err); // Fehler -> Ausgabe-Log + Status
}

} // namespace memdb::viewer
