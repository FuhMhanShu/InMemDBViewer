#pragma once

#include <QWidget>
#include <QString>

namespace memdb::viewer {
class ViewerCore;
namespace detail {
class DesignController;
struct FieldDef;
}
} // namespace memdb::viewer

namespace memdb::viewer {

/// @brief Gekapseltes MVC-Widget „Datenbank-Design“: Datenbank neu anlegen bzw.
///        geladene bearbeiten – Collections verwalten (anlegen/löschen/leeren),
///        **Schema** pro Collection definieren (FA-20: Feldpfad, Typ, nullable,
///        Unter-Schema, Array-Elementtyp), **Indizes** anlegen/löschen (FA-10,
///        unique wählbar) und **Beziehungen/Referenzen** definieren (FA-18:
///        von/nach Collection+Dokument, Relationsname, Dangling-Anzeige/-Prune).
///
/// Alle Operationen laufen über `ViewerCore`, der sie verifiziert (Exceptions aus
/// InMemDB werden gefangen und als Fehler in die Ausgabe geloggt) und `dataMutated()`
/// anstößt. Interner `detail::DesignController` vermittelt View ↔ Core.
///
/// Verwendung: `auto* design = new DatabaseDesignWidget; design->setCore(core);`
class DatabaseDesignWidget : public QWidget {
    Q_OBJECT
public:
    explicit DatabaseDesignWidget(QWidget* parent = nullptr);
    ~DatabaseDesignWidget() override;

    void setCore(ViewerCore* core);
    ViewerCore* core() const;

    /// @brief aktuell im Widget selektierte Collection (für Tests/Diagnose).
    QString currentCollection() const;

    // Introspection fuer Tests
    int collectionItemCount() const;
    int schemaRowCount() const;
    int indexItemCount() const;
    int referenceRowCount() const;

public slots:
    void refresh();
    /// @brief Collection in der Liste selektieren (zeigt Schema/Indizes/Referenzen).
    void selectCollection(const QString& name);

signals:
    void statusMessage(const QString& message);

private:
    void loadCollectionDetail(const QString& name);
    void addSchemaRow(const detail::FieldDef& d);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace memdb::viewer
