#include "document_tree_model.hpp"

#include <memdb/schema.hpp>
#include <memdb/value.hpp>

#include <functional>
#include <utility>

namespace memdb::viewer::detail {

DocumentTreeModel::DocumentTreeModel(QObject* parent) : QAbstractItemModel(parent) {
    root_ = std::make_unique<Node>();
}

void DocumentTreeModel::setDocument(std::optional<memdb::Document> doc) {
    beginResetModel();
    root_ = std::make_unique<Node>();
    if (doc) {
        root_->name = QStringLiteral("Dokument #%1").arg(doc->id().value());
        root_->type = QStringLiteral("document");
        doc->forEachField([this](const std::string& key, const memdb::Value& val) {
            root_->children.push_back(buildNode(QString::fromStdString(key), val));
        });
    }
    endResetModel();
}

void DocumentTreeModel::clearDocument() { setDocument(std::nullopt); }

int DocumentTreeModel::rowCount(const QModelIndex& parent) const {
    if (!parent.isValid())
        return static_cast<int>(root_->children.size());
    const Node* n = nodeFromIndex(parent);
    return n ? static_cast<int>(n->children.size()) : 0;
}

int DocumentTreeModel::columnCount(const QModelIndex&) const { return ColumnCount; }

QVariant DocumentTreeModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return {};
    switch (section) {
    case FieldColumn: return QStringLiteral("Feld");
    case TypeColumn: return QStringLiteral("Typ");
    case ValueColumn: return QStringLiteral("Wert");
    default: return {};
    }
}

QVariant DocumentTreeModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid())
        return {};
    const Node* n = nodeFromIndex(index);
    if (!n)
        return {};
    if (role != Qt::DisplayRole && role != Qt::ToolTipRole)
        return {};
    switch (index.column()) {
    case FieldColumn: return n->name;
    case TypeColumn: return n->type;
    case ValueColumn: {
        QString v = n->value;
        if (role == Qt::ToolTipRole && v.size() > 200)
            return v;
        if (v.size() > 200)
            v = v.left(197) + QStringLiteral("…");
        return v;
    }
    default: return {};
    }
}

QModelIndex DocumentTreeModel::index(int row, int column, const QModelIndex& parent) const {
    if (!hasIndex(row, column, parent))
        return {};
    Node* parentNode = parent.isValid() ? nodeFromIndex(parent) : root_.get();
    if (!parentNode || row >= static_cast<int>(parentNode->children.size()))
        return {};
    return createIndex(row, column, parentNode->children[static_cast<std::size_t>(row)].get());
}

QModelIndex DocumentTreeModel::parent(const QModelIndex& index) const {
    if (!index.isValid())
        return {};
    Node* n = nodeFromIndex(index);
    if (!n || n == root_.get())
        return {};
    // (Eltern, Zeile) des Knotens im Baum suchen
    std::function<std::pair<Node*, int>(Node*, Node*)> find = [&](Node* cur, Node* target) -> std::pair<Node*, int> {
        for (int i = 0; i < static_cast<int>(cur->children.size()); ++i) {
            if (cur->children[static_cast<std::size_t>(i)].get() == target)
                return {cur, i};
            auto r = find(cur->children[static_cast<std::size_t>(i)].get(), target);
            if (r.first)
                return r;
        }
        return {nullptr, -1};
    };
    const auto [p, row] = find(root_.get(), n);
    if (!p || p == root_.get())
        return {};
    const auto [gp, prow] = find(root_.get(), p);
    if (!gp)
        return {};
    return createIndex(prow, 0, p);
}

DocumentTreeModel::Node* DocumentTreeModel::nodeFromIndex(const QModelIndex& index) {
    return index.isValid() ? static_cast<Node*>(index.internalPointer()) : nullptr;
}

std::unique_ptr<DocumentTreeModel::Node> DocumentTreeModel::buildNode(const QString& name,
                                                                      const memdb::Value& v) {
    auto n = std::make_unique<Node>();
    n->name = name;
    n->type = QString::fromStdString(memdb::typeName(v.type()));
    switch (v.type()) {
    case memdb::Value::Type::Document: {
        if (auto d = v.asDocument()) {
            d->forEachField([&](const std::string& key, const memdb::Value& sub) {
                n->children.push_back(buildNode(QString::fromStdString(key), sub));
            });
            n->value = QStringLiteral("{ %1 Felder }").arg(d->fieldCount());
        }
        break;
    }
    case memdb::Value::Type::Array: {
        const auto& arr = v.asArray();
        for (std::size_t i = 0; i < arr.size(); ++i)
            n->children.push_back(buildNode(QStringLiteral("[%1]").arg(i), arr[i]));
        n->value = QStringLiteral("[ %1 Elemente ]").arg(arr.size());
        break;
    }
    case memdb::Value::Type::String:
        n->value = QString::fromStdString(v.asString());
        break;
    case memdb::Value::Type::Binary: {
        const auto& b = v.asBinary();
        n->value = QStringLiteral("<binary %1 Bytes>").arg(b.size());
        break;
    }
    default:
        n->value = QString::fromStdString(memdb::toString(v));
        break;
    }
    return n;
}

} // namespace memdb::viewer::detail
