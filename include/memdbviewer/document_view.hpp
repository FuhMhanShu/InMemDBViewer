#pragma once

#include <memdb/aggregate.hpp>
#include <memdb/collection.hpp>
#include <memdb/query.hpp>

#include <QWidget>

#include <optional>
#include <vector>

class QComboBox;
class QLineEdit;
class QLabel;
class QPushButton;
class QTableView;

namespace memdb::qt {
class MemDbTableModel;
}

namespace memdb::viewer {

/// @brief View (MVC): Dokument-Tabelle einer Collection (FA-07) auf Basis von
///        memdb::qt::MemDbTableModel (v0.23, inkl. Auto-Refresh per Observer)
///        mit DSL-Query-Filter (FA-10), CRUD-Editing (FA-14: Zellen editierbar,
///        Neu/Löschen) und Aggregat-Auswertung (FA-17).
class DocumentView : public QWidget {
    Q_OBJECT
public:
    explicit DocumentView(QWidget* parent = nullptr);

    /// @brief Collection in der Tabelle anzeigen (Handle-Semantik, nicht-owning).
    void showCollection(memdb::Collection collection);
    memdb::qt::MemDbTableModel* model() const { return model_; }

    /// @brief Leeres Dokument in die aktuelle Collection einfuegen (FA-14); false ohne Collection.
    bool insertEmptyDocument();
    /// @brief Ausgewaehlte Zeile loeschen (FA-14); false ohne Auswahl.
    bool deleteSelectedDocument();
    /// @brief Zeile selektieren (z. B. fuer Demo/Tests).
    void selectRow(int row);
    /// @brief Aggregat ueber die Collection ausfuehren, optional gefiltert durch den
    ///        aktuellen DSL-Query (FA-17); Rueckgabe: Ergebnis als Text.
    QString applyAggregate(memdb::AggOp op, const QString& fieldPath);
    /// @brief Aktueller Statuszeilen-Text (u. a. fuer Tests).
    QString statusText() const;

signals:
    /// @brief Zeile in der Tabelle wurde angeklickt (FA-07: Detailansicht im Baum).
    void documentSelected(quint64 documentId);

public slots:
    /// @brief DSL-Query-Filter anwenden (FA-10).
    void applyQuery();
    /// @brief Aggregat aus den UI-Feldern (Combo + Feld-Edit) ausfuehren (FA-17).
    void onAggregate();
    /// @brief Tabellen-Klick: documentSelected emittieren.
    void onCellClicked(const QModelIndex& index);

private:
    void updateStatus();
    memdb::Expression currentQuery() const;
    std::vector<memdb::DocumentId> currentIds() const;

    std::optional<memdb::Collection> coll_;
    memdb::qt::MemDbTableModel* model_ = nullptr;
    QTableView* table_ = nullptr;
    QLineEdit* queryEdit_ = nullptr;
    QPushButton* insertBtn_ = nullptr;
    QPushButton* deleteBtn_ = nullptr;
    QComboBox* aggOpCombo_ = nullptr;
    QLineEdit* aggFieldEdit_ = nullptr;
    QLabel* status_ = nullptr;
};

} // namespace memdb::viewer
