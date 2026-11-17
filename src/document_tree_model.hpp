#pragma once

#include <memdb/document.hpp>

#include <QAbstractItemModel>

#include <memory>
#include <optional>
#include <vector>

namespace memdb::viewer::detail {

/// @brief Model (MVC) der Dokument-Detailansicht: rekursiver Baum ueber ein
///        `memdb::Document` – Spalten Feld / Typ / Wert; verschachtelte Dokumente
///        und Arrays als Kind-Knoten.
class DocumentTreeModel : public QAbstractItemModel {
    Q_OBJECT
public:
    enum Column { FieldColumn = 0, TypeColumn, ValueColumn, ColumnCount };

    explicit DocumentTreeModel(QObject* parent = nullptr);

    void setDocument(std::optional<memdb::Document> doc);
    void clearDocument();
    bool hasDocument() const { return root_ != nullptr; }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index) const override;

private:
    struct Node {
        QString name;
        QString type;
        QString value;
        std::vector<std::unique_ptr<Node>> children;
    };
    static std::unique_ptr<Node> buildNode(const QString& name, const memdb::Value& v);
    static Node* nodeFromIndex(const QModelIndex& index);

    std::unique_ptr<Node> root_;
};

} // namespace memdb::viewer::detail
