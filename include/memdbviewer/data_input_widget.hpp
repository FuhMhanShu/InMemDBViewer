#pragma once

#include <QWidget>
#include <QStringList>

namespace memdb::viewer {
class ViewerCore;
namespace detail {
class DataInputController;
class ImportModel;
}
} // namespace memdb::viewer

namespace memdb::viewer {

/// @brief Gekapseltes MVC-Widget zur Dateneingabe (DL-01…DL-07): Datei-Dialog,
///        Drag & Drop (auch mehrfach/Ordner), CSV aus der Zwischenablage (DE-04,
///        mit Optionen-Dialog), Liste der unterstuetzten Formate
///        (ImportRegistry FA-21: CSV, JSON/JSONL, SQL-Dump, BSON, SQLite falls
///        verfuegbar, InMemDB-Snapshot inkl. verschluesselt FA-19), Import-Optionen,
///        Verlauf und Status. Haengt an einem `ViewerCore` (`setCore`).
///
/// Verwendung: `auto* input = new DataInputWidget; input->setCore(core);`
/// Drag & Drop auf dem Hauptfenster kann via `handleDroppedFiles()` durchgereicht werden.
class DataInputWidget : public QWidget {
    Q_OBJECT
public:
    explicit DataInputWidget(QWidget* parent = nullptr);
    ~DataInputWidget() override;

    void setCore(ViewerCore* core);
    ViewerCore* core() const;

    /// @brief Verarbeitet abgelegte lokale Dateien (Drag & Drop); auch vom Hauptfenster nutzbar.
    void handleDroppedFiles(const QStringList& paths);

    /// @brief Liste der unterstuetzten Formate (z. B. "csv (.csv), json (.json, .jsonl, .ndjson), …").
    static QString supportedFormatsText();

public slots:
    void openFileDialog();
    void openFolderDialog();
    /// @brief CSV-Text aus der Zwischenablage importieren (DE-04): Optionen-Dialog,
    ///        dann verifizierter Import ueber `ViewerCore::importCsvText`.
    void importCsvFromClipboard();

signals:
    /// @brief Statusmeldung (Ergebnis eines Ladens / Fehler) – z. B. fuer die Statusleiste.
    void statusMessage(const QString& message);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace memdb::viewer
