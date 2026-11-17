#include "data_input_controller.hpp"
#include "import_model.hpp"
#include "memdbviewer/viewer_core.hpp"

#include <memdb/import_csv.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGuiApplication>
#include <QClipboard>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QWidget>

namespace memdb::viewer::detail {

namespace {
/// @brief Dialog fuer CSV-Import aus der Zwischenablage: Collection-Name, Trennzeichen,
///        Quote, Header, Typ-Inferenz + gekuerzte Vorschau des Textes.
class CsvClipboardDialog : public QDialog {
public:
    explicit CsvClipboardDialog(const QString& csvText, QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle(QStringLiteral("CSV aus Zwischenablage importieren"));
        auto* layout = new QVBoxLayout(this);
        auto* form = new QFormLayout();
        tableEdit = new QLineEdit(QStringLiteral("clipboard"), this);
        tableEdit->setToolTip(QStringLiteral("Name der neuen Collection"));
        form->addRow(QStringLiteral("Collection-Name:"), tableEdit);
        delimBox = new QComboBox(this);
        delimBox->setEditable(true);
        delimBox->addItem(QStringLiteral(",  (Komma)"), QChar(','));
        delimBox->addItem(QStringLiteral(";  (Semikolon)"), QChar(';'));
        delimBox->addItem(QStringLiteral("Tabulator"), QChar('\t'));
        delimBox->addItem(QStringLiteral("|  (Pipe)"), QChar('|'));
        delimBox->setToolTip(QStringLiteral("Feld-Trennzeichen; eigenes Zeichen moeglich"));
        form->addRow(QStringLiteral("Trennzeichen:"), delimBox);
        quoteEdit = new QLineEdit(QStringLiteral("\""), this);
        quoteEdit->setMaxLength(1);
        form->addRow(QStringLiteral("Quote-Zeichen:"), quoteEdit);
        headerBox = new QCheckBox(QStringLiteral("Erste Zeile enthaelt Spaltennamen"), this);
        headerBox->setChecked(true);
        form->addRow(headerBox);
        inferBox = new QCheckBox(QStringLiteral("Typen ableiten (Int/Double, sonst Text)"), this);
        form->addRow(inferBox);
        layout->addLayout(form);
        QString preview = csvText;
        if (preview.size() > 2000)
            preview = preview.left(2000) + QStringLiteral("\u2026");
        auto* previewLabel = new QLabel(QStringLiteral("Vorschau (gekuerzt):"), this);
        previewEdit = new QPlainTextEdit(this);
        previewEdit->setReadOnly(true);
        previewEdit->setMaximumHeight(120);
        previewEdit->setPlainText(preview);
        layout->addWidget(previewLabel);
        layout->addWidget(previewEdit);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }
    QString tableName() const { return tableEdit->text().trimmed(); }
    memdb::CsvImportOptions options() const {
        memdb::CsvImportOptions o;
        const QVariant d = delimBox->currentData();
        QChar delim = d.isValid() ? d.toChar() : QChar(',');
        if (!d.isValid() && !delimBox->currentText().isEmpty())
            delim = delimBox->currentText().at(0);
        o.delimiter = delim.toLatin1();
        o.quote = quoteEdit->text().isEmpty() ? '"' : quoteEdit->text().at(0).toLatin1();
        o.hasHeader = headerBox->isChecked();
        o.inferTypes = inferBox->isChecked();
        return o;
    }
private:
    QLineEdit* tableEdit = nullptr;
    QComboBox* delimBox = nullptr;
    QLineEdit* quoteEdit = nullptr;
    QCheckBox* headerBox = nullptr;
    QCheckBox* inferBox = nullptr;
    QPlainTextEdit* previewEdit = nullptr;
};
} // namespace

DataInputController::DataInputController(ViewerCore* core, ImportModel* model, QObject* parent)
    : QObject(parent), core_(core), model_(model) {
    connect(core_, &ViewerCore::importStarted, this, [this](const QString& path) {
        model_->addResult(path, QStringLiteral("…"), true, QStringLiteral("laedt …"));
        emit statusMessage(QStringLiteral("Lade %1 …").arg(path));
        emit busyChanged(true);
    });
    connect(core_, &ViewerCore::importFinished, this, [this](const QString& path, bool ok, const QString& message) {
        model_->updateLatest(path, ok, message);
        emit statusMessage(ok ? message : QStringLiteral("Fehler: %1").arg(message));
        emit busyChanged(false);
    });
}

void DataInputController::openFileDialog(QWidget* parentWidget) {
    const QStringList paths = QFileDialog::getOpenFileNames(
        parentWidget, QStringLiteral("Daten oeffnen"), QString(), ViewerCore::fileDialogFilter());
    if (!paths.isEmpty())
        importPaths(paths, parentWidget);
}

void DataInputController::openFolderDialog(QWidget* parentWidget) {
    const QString dir = QFileDialog::getExistingDirectory(parentWidget, QStringLiteral("Ordner oeffnen"));
    if (dir.isEmpty())
        return;
    QStringList paths;
    const QStringList exts = ViewerCore::supportedExtensions();
    const QDir d(dir);
    const auto files = d.entryList(QDir::Files, QDir::Name);
    for (const QString& f : files) {
        const QString suffix = QFileInfo(f).suffix().toLower();
        for (const QString& e : exts) {
            if (e.mid(1) == suffix || (e == QLatin1String(".enc") && f.endsWith(QLatin1String(".enc")))) {
                paths << d.absoluteFilePath(f);
                break;
            }
        }
    }
    if (paths.isEmpty()) {
        emit statusMessage(QStringLiteral("Keine unterstuetzten Dateien in %1 gefunden.").arg(dir));
        return;
    }
    importPaths(paths, parentWidget);
}

void DataInputController::importPath(const QString& path, QWidget* dialogParent) {
    if (ViewerCore::isEncryptedSnapshotFile(path)) {
        const bool offscreen = QGuiApplication::platformName() == QLatin1String("offscreen");
        QString password;
        if (!offscreen) {
            bool entered = false;
            password = QInputDialog::getText(dialogParent, QStringLiteral("Verschluesselter Snapshot"),
                                             QStringLiteral("Passwort:"), QLineEdit::Password, QString(),
                                             &entered);
            if (!entered || password.isEmpty())
                return;
        } else {
            emit statusMessage(QStringLiteral("Verschluesselter Snapshot %1 – im Offscreen-Modus kein Passwort-Dialog.").arg(path));
            return;
        }
        core_->loadSnapshotEncryptedAsync(path, password);
        return;
    }
    // Snapshot-Endungen direkt als Snapshot (ersetzt Database), sonst Import in die laufende Database
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QLatin1String("json") || path.endsWith(QLatin1String(".inmemdb.json"), Qt::CaseInsensitive)) {
        // .json kann Daten-JSON ODER Snapshot sein; ViewerCore::importFile erkennt Snapshot am Inhalt
        core_->importFileAsync(path);
        return;
    }
    core_->importFileAsync(path);
}

void DataInputController::importPaths(const QStringList& paths, QWidget* dialogParent) {
    for (const QString& p : paths)
        importPath(p, dialogParent);
}

void DataInputController::importCsvFromClipboard(QWidget* dialogParent, const QString& clipboardText) {
    QString text = clipboardText;
    if (text.isEmpty()) {
        if (QClipboard* cb = QGuiApplication::clipboard())
            text = cb->text();
    }
    if (text.trimmed().isEmpty()) {
        emit statusMessage(QStringLiteral("Zwischenablage enthaelt keinen Text – nichts importiert."));
        return;
    }
    CsvClipboardDialog dlg(text, dialogParent);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const QString name = dlg.tableName();
    if (name.isEmpty()) {
        emit statusMessage(QStringLiteral("Kein Collection-Name angegeben – nichts importiert."));
        return;
    }
    QString err;
    if (!core_->importCsvText(name, text, dlg.options(), &err))
        emit statusMessage(QStringLiteral("CSV-Import fehlgeschlagen: %1").arg(err));
    // Erfolg: im Core geloggt (Ausgabe-Panel) + dataMutated
}

} // namespace memdb::viewer::detail
