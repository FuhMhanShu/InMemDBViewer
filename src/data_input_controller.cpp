#include "data_input_controller.hpp"
#include "import_model.hpp"
#include "memdbviewer/viewer_core.hpp"

#include <QDir>
#include <QFileDialog>
#include <QGuiApplication>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QWidget>

namespace memdb::viewer::detail {

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

} // namespace memdb::viewer::detail
