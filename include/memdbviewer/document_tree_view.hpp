#pragma once

#include <memdb/document.hpp>

#include <QTreeView>

class QStandardItemModel;

namespace memdb::viewer {

/// @brief View (MVC): Detail-/Baumansicht eines Dokuments (FA-07): Felder als Zeilen
///        (Spalten Feld/Typ/Wert), verschachtelte Dokumente und Arrays rekursiv aufklappbar.
class DocumentTreeView : public QTreeView {
    Q_OBJECT
public:
    explicit DocumentTreeView(QWidget* parent = nullptr);

    /// @brief Dokument im Baum anzeigen (Kopie; ID und Feldanzahl in der Wurzel).
    void showDocument(const memdb::Document& doc);
    /// @brief Baum leeren.
    void clearDocument();

private:
    void appendValue(class QStandardItem* parent, const QString& key, const memdb::Value& value);

    QStandardItemModel* model_ = nullptr;
};

} // namespace memdb::viewer
