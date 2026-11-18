#include "memdbviewer/data_input_widget.hpp"
#include "memdbviewer/viewer_core.hpp"

#include "data_input_controller.hpp"
#include "import_model.hpp"

#include <memdb/import.hpp>

#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QStyle>
#include <QToolButton>
#include "tool_button.hpp"
#include <QSpinBox>
#include <QSplitter>
#include <QUrl>
#include <QVBoxLayout>

namespace memdb::viewer {

struct DataInputWidget::Impl {
    ViewerCore* core = nullptr;
    detail::ImportModel* model = nullptr;
    detail::DataInputController* controller = nullptr;
    QLabel* formatsLabel = nullptr;
    QListView* historyView = nullptr;
    QPlainTextEdit* log = nullptr;
    QToolButton* openFileBtn = nullptr;
    QToolButton* openFolderBtn = nullptr;
    QToolButton* csvClipboardBtn = nullptr;
    QToolButton* optionsBtn = nullptr;
    QToolButton* clearBtn = nullptr;
    QFrame* dropFrame = nullptr;
};

QString DataInputWidget::supportedFormatsText() {
    // Quelle: Namen + Endungen aus der ImportRegistry (sondert nicht verfuegbare wie sqlite aus)
    static const std::vector<std::pair<QString, QStringList>> known = {
        {"csv", {".csv"}},
        {"json", {".json"}},
        {"jsonl", {".jsonl", ".ndjson"}},
        {"sqldump", {".sql", ".dump"}},
        {"bson", {".bson"}},
        {"sqlite", {".db", ".sqlite", ".sqlite3"}},
        {"snapshot", {".inmemdb.json", ".json (Snapshot)"}},
    };
    const QStringList names = ViewerCore::supportedSourceNames();
    QStringList parts;
    for (const auto& [name, exts] : known) {
        if (!names.contains(name))
            continue;
        parts << QStringLiteral("%1 (%2)").arg(name, exts.join(QStringLiteral(", ")));
    }
    parts << QStringLiteral("verschluesselter Snapshot (.enc, FA-19)");
    return parts.join(QStringLiteral(", "));
}

namespace {
/// @brief Optionen-Dialog fuer `memdb::ImportOptions` (FA-21).
class ImportOptionsDialog : public QDialog {
public:
    explicit ImportOptionsDialog(const memdb::ImportOptions& opts, QWidget* parent = nullptr)
        : QDialog(parent) {
        setWindowTitle(QStringLiteral("Import-Optionen"));
        auto* form = new QFormLayout(this);
        batchSpin = new QSpinBox(this);
        batchSpin->setRange(100, 1000000);
        batchSpin->setSingleStep(1000);
        batchSpin->setValue(static_cast<int>(opts.batchSize));
        form->addRow(QStringLiteral("Batch-Groesse:"), batchSpin);
        createIndexesBox = new QCheckBox(QStringLiteral("Indizes der Quelle anlegen"), this);
        createIndexesBox->setChecked(opts.createIndexes);
        form->addRow(createIndexesBox);
        schemaBox = new QCheckBox(QStringLiteral("Schema aus Spaltentypen generieren (FA-20)"), this);
        schemaBox->setChecked(opts.generateSchema);
        form->addRow(schemaBox);
        pkBox = new QCheckBox(QStringLiteral("Primärschlüssel als Feld speichern"), this);
        pkBox->setChecked(opts.keepPrimaryKeyField);
        form->addRow(pkBox);
        pkNameEdit = new QLineEdit(QString::fromStdString(opts.pkFieldName), this);
        form->addRow(QStringLiteral("PK-Feldname:"), pkNameEdit);
        fkBox = new QCheckBox(QStringLiteral("Fremdschlüssel als Referenzen importieren (FA-26)"), this);
        fkBox->setChecked(opts.importForeignKeys);
        form->addRow(fkBox);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        form->addRow(buttons);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }
    memdb::ImportOptions options() const {
        memdb::ImportOptions o;
        o.batchSize = static_cast<std::size_t>(batchSpin->value());
        o.createIndexes = createIndexesBox->isChecked();
        o.generateSchema = schemaBox->isChecked();
        o.keepPrimaryKeyField = pkBox->isChecked();
        o.pkFieldName = pkNameEdit->text().toStdString();
        o.importForeignKeys = fkBox->isChecked();
        return o;
    }
private:
    QSpinBox* batchSpin = nullptr;
    QCheckBox* createIndexesBox = nullptr;
    QCheckBox* schemaBox = nullptr;
    QCheckBox* pkBox = nullptr;
    QLineEdit* pkNameEdit = nullptr;
    QCheckBox* fkBox = nullptr;
};
} // namespace

DataInputWidget::DataInputWidget(QWidget* parent) : QWidget(parent), impl_(std::make_unique<Impl>()) {
    setAcceptDrops(true);
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(6, 6, 6, 6);

    // Kopfzeile: Titel + Aktionen
    auto* header = new QHBoxLayout();
    auto* title = new QLabel(QStringLiteral("Daten laden"), this);
    QFont bold = title->font();
    bold.setBold(true);
    title->setFont(bold);
    header->addWidget(title);
    header->addStretch();
    impl_->openFileBtn = detail::makeToolButton(this, QStyle::SP_DialogOpenButton, QStringLiteral("Datei(en) oeffnen…"));
    impl_->openFileBtn->setToolTip(QStringLiteral("Eine oder mehrere Dateien importieren (Strg+O)"));
    impl_->openFolderBtn = detail::makeToolButton(this, QStyle::SP_DirIcon, QStringLiteral("Ordner oeffnen…"));
    impl_->openFolderBtn->setToolTip(QStringLiteral("Alle unterstuetzten Dateien eines Ordners importieren"));
    impl_->csvClipboardBtn = detail::makeToolButton(this, QStyle::SP_FileDialogListView, QStringLiteral("CSV aus Zwischenablage…"));
    impl_->csvClipboardBtn->setToolTip(QStringLiteral("CSV-Text aus der Zwischenablage als neue Collection importieren (DE-04, mit Optionen-Dialog)"));
    impl_->optionsBtn = detail::makeToolButton(this, QStyle::SP_FileDialogInfoView, QStringLiteral("Optionen…"));
    impl_->optionsBtn->setToolTip(QStringLiteral("Import-Optionen (FA-21): Batch, Indizes, Schema, PK, Fremdschluessel"));
    impl_->clearBtn = detail::makeToolButton(this, QStyle::SP_TrashIcon, QStringLiteral("Verlauf leeren"));
    header->addWidget(impl_->openFileBtn);
    header->addWidget(impl_->openFolderBtn);
    header->addWidget(impl_->csvClipboardBtn);
    header->addWidget(impl_->optionsBtn);
    header->addWidget(impl_->clearBtn);
    outer->addLayout(header);

    // Drop-Bereich
    impl_->dropFrame = new QFrame(this);
    impl_->dropFrame->setFrameShape(QFrame::StyledPanel);
    impl_->dropFrame->setAcceptDrops(true);
    impl_->dropFrame->setMaximumHeight(56); // kompakt, Dock bleibt schlank
    impl_->dropFrame->setStyleSheet(
        QStringLiteral("QFrame { border: 1.5px dashed palette(mid); border-radius: 6px; }"));
    auto* dropLayout = new QVBoxLayout(impl_->dropFrame);
    auto* dropLabel = new QLabel(
        QStringLiteral("Dateien hierher ziehen (Drag & Drop) oder ueber die Knoepfe oben laden"), impl_->dropFrame);
    dropLabel->setAlignment(Qt::AlignCenter);
    dropLabel->setProperty("dim", true); // dezente Farbe ueber das zentrale Theme (QLabel[dim="true"])
    dropLayout->addWidget(dropLabel);
    outer->addWidget(impl_->dropFrame);

    // Unterstuetzte Formate
    impl_->formatsLabel = new QLabel(this);
    impl_->formatsLabel->setWordWrap(true);
    impl_->formatsLabel->setProperty("dim", true);
    impl_->formatsLabel->setText(QStringLiteral("Formate: %1").arg(supportedFormatsText()));
    outer->addWidget(impl_->formatsLabel);

    // Verlauf + Log
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    impl_->historyView = new QListView(splitter);
    impl_->historyView->setToolTip(QStringLiteral("Zuletzt geladene Dateien – Doppelklick laedt erneut"));
    auto* histLabel = new QLabel(QStringLiteral("Verlauf"), splitter);
    impl_->log = new QPlainTextEdit(splitter);
    impl_->log->setReadOnly(true);
    impl_->log->setMaximumBlockCount(200);
    auto* leftBox = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(leftBox);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(histLabel);
    leftLayout->addWidget(impl_->historyView);
    splitter->addWidget(leftBox);
    splitter->addWidget(impl_->log);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    splitter->setMaximumHeight(104); // kompakt: Dock oben bleibt schlank (UI-02)
    outer->addWidget(splitter, 1);

    impl_->model = new detail::ImportModel(this);
    impl_->historyView->setModel(impl_->model);

    connect(impl_->openFileBtn, &QToolButton::clicked, this, &DataInputWidget::openFileDialog);
    connect(impl_->openFolderBtn, &QToolButton::clicked, this, &DataInputWidget::openFolderDialog);
    connect(impl_->csvClipboardBtn, &QToolButton::clicked, this, &DataInputWidget::importCsvFromClipboard);
    connect(impl_->clearBtn, &QToolButton::clicked, impl_->model, &detail::ImportModel::clearHistory);
    connect(impl_->optionsBtn, &QToolButton::clicked, this, [this] {
        if (!impl_->core)
            return;
        ImportOptionsDialog dlg(impl_->core->importOptions(), this);
        if (dlg.exec() == QDialog::Accepted)
            impl_->core->setImportOptions(dlg.options());
    });
    connect(impl_->historyView, &QListView::doubleClicked, this, [this](const QModelIndex& idx) {
        if (!impl_->controller)
            return;
        const QString path = impl_->model->data(idx, detail::ImportModel::PathRole).toString();
        if (!path.isEmpty())
            impl_->controller->importPath(path, this);
    });
}

DataInputWidget::~DataInputWidget() = default;

void DataInputWidget::setCore(ViewerCore* core) {
    impl_->core = core;
    if (!core)
        return;
    if (!impl_->controller) {
        impl_->controller = new detail::DataInputController(core, impl_->model, this);
        connect(impl_->controller, &detail::DataInputController::statusMessage, this,
                [this](const QString& msg) {
                    impl_->log->appendPlainText(msg);
                    emit statusMessage(msg);
                });
        connect(impl_->controller, &detail::DataInputController::busyChanged, this,
                [this](bool busy) { setEnabled(!busy); });
    }
}

ViewerCore* DataInputWidget::core() const { return impl_->core; }

void DataInputWidget::openFileDialog() {
    if (impl_->controller)
        impl_->controller->openFileDialog(this);
}

void DataInputWidget::openFolderDialog() {
    if (impl_->controller)
        impl_->controller->openFolderDialog(this);
}

void DataInputWidget::importCsvFromClipboard() {
    if (impl_->controller)
        impl_->controller->importCsvFromClipboard(this);
}

void DataInputWidget::handleDroppedFiles(const QStringList& paths) {
    if (impl_->controller)
        impl_->controller->importPaths(paths, this);
}

void DataInputWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void DataInputWidget::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void DataInputWidget::dropEvent(QDropEvent* event) {
    QStringList paths;
    const auto urls = event->mimeData()->urls();
    for (const QUrl& url : urls)
        if (url.isLocalFile())
            paths << url.toLocalFile();
    if (!paths.isEmpty()) {
        handleDroppedFiles(paths);
        event->acceptProposedAction();
    }
}

} // namespace memdb::viewer
