#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace memdb::viewer {
class ViewerCore;
namespace detail {
class ImportModel;
}
} // namespace memdb::viewer

namespace memdb::viewer::detail {

/// @brief Controller (MVC) des DataInputWidgets: vermittelt zwischen Widget (View)
///        und ViewerCore/ImportModel (Model) – kennt keine konkreten Widgets.
class DataInputController : public QObject {
    Q_OBJECT
public:
    explicit DataInputController(ViewerCore* core, ImportModel* model, QObject* parent = nullptr);

    /// @brief Datei(en) ueber Datei-Dialog auswaehlen und laden (im GUI-Thread des parent-Widgets).
    void openFileDialog(QWidget* parentWidget);
    /// @brief Ordner auswaehlen; alle unterstuetzten Dateien darin werden geladen.
    void openFolderDialog(QWidget* parentWidget);

    /// @brief Liste von Datei-Pfaden laden (Dialog/Drag&Drop); erkennt verschluesselte
    ///        Snapshots und fragt das Passwort ab (im Offscreen-Modus wird abgebrochen).
    void importPaths(const QStringList& paths, QWidget* dialogParent);

    /// @brief Einzelne Datei laden (auto-Quelle); verschluesselt -> Passwort-Dialog.
    void importPath(const QString& path, QWidget* dialogParent);

    ImportModel* model() const { return model_; }

signals:
    void statusMessage(const QString& message);
    void busyChanged(bool busy);

private:
    ViewerCore* core_;
    ImportModel* model_;
};

} // namespace memdb::viewer::detail
