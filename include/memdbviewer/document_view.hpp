#pragma once

#include <memdb/collection.hpp>
#include <memdb/qt/memdb_qt_model.hpp>

#include <QWidget>

#include <optional>

class QLineEdit;
class QLabel;
class QTableView;

namespace memdb::viewer {

/// @brief View (MVC): Dokument-Tabelle einer Collection (FA-07) auf Basis von
///        memdb::qt::MemDbTableModel (v0.23, inkl. Auto-Refresh per Observer)
///        mit DSL-Query-Filter (FA-10) und Query-Diagnose (Index vs. Vollscan).
class DocumentView : public QWidget {
    Q_OBJECT
public:
    explicit DocumentView(QWidget* parent = nullptr);

    /// @brief Collection in der Tabelle anzeigen (Handle-Semantik, nicht-owning).
    void showCollection(memdb::Collection collection);
    memdb::qt::MemDbTableModel* model() const { return model_; }

private slots:
    void applyQuery();

private:
    void updateStatus();

    std::optional<memdb::Collection> coll_;
    memdb::qt::MemDbTableModel* model_ = nullptr;
    QTableView* table_ = nullptr;
    QLineEdit* queryEdit_ = nullptr;
    QLabel* status_ = nullptr;
};

} // namespace memdb::viewer
