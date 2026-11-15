#include "memdbviewer/database_model.hpp"

#include <memdb/collection.hpp>
#include <memdb/index.hpp>
#include <memdb/schema.hpp>

namespace memdb::viewer {

DatabaseModel::DatabaseModel(QObject* parent)
    : QAbstractItemModel(parent), root_(std::make_unique<Node>()) {}

void DatabaseModel::setDatabase(memdb::Database* db) {
    db_ = db;
    refresh();
}

void DatabaseModel::refresh() {
    beginResetModel();
    rebuild();
    endResetModel();
}

void DatabaseModel::rebuild() {
    root_ = std::make_unique<Node>();
    if (!db_)
        return;

    for (const std::string& name : db_->collectionNames()) {
        Collection coll = db_->collection(name);
        const bool hasSchema = coll.hasSchema();
        const std::vector<Index> indexes = coll.indexes();

        auto node = std::make_unique<Node>();
        node->kind = Node::Kind::Collection;
        node->name = QString::fromStdString(name);
        node->parent = root_.get();
        node->details = QStringLiteral("%1 Dokument(e) | Schema: %2 | %3 Index(e)")
                            .arg(coll.size())
                            .arg(hasSchema ? QStringLiteral("ja") : QStringLiteral("nein"))
                            .arg(indexes.size());

        auto addField = [&](const std::string& path, const QString& type, bool nullable,
                            const QString& indexFlag) {
            auto f = std::make_unique<Node>();
            f->kind = Node::Kind::Field;
            f->name = QString::fromStdString(path);
            QString d = type;
            d += nullable ? QStringLiteral(", nullable") : QStringLiteral(", notNull");
            if (!indexFlag.isEmpty())
                d += QStringLiteral(", ") + indexFlag;
            f->details = d;
            f->parent = node.get();
            node->children.push_back(std::move(f));
        };

        const std::optional<Schema> schema = coll.schema();
        if (schema) {
            for (const auto& [path, field] : schema->fields()) {
                QString indexFlag;
                for (const Index& idx : indexes)
                    if (idx.field() == path)
                        indexFlag = idx.isUnique() ? QStringLiteral("unique index")
                                                   : QStringLiteral("index");
                addField(path, QString::fromStdString(typeName(field.type())), field.nullable(),
                         indexFlag);
            }
        }
        // index-only Felder (Index auf Feld ohne Schema-Eintrag)
        for (const Index& idx : indexes) {
            if (schema && schema->contains(idx.field()))
                continue;
            addField(idx.field(), QStringLiteral("?"), true,
                     idx.isUnique() ? QStringLiteral("unique index") : QStringLiteral("index"));
        }

        root_->children.push_back(std::move(node));
    }
}

QModelIndex DatabaseModel::index(int row, int column, const QModelIndex& parent) const {
    if (!hasIndex(row, column, parent))
        return {};
    Node* p = nodeFor(parent);
    if (row < 0 || row >= static_cast<int>(p->children.size()) || column < 0 || column >= ColumnCount)
        return {};
    return createIndex(row, column, p->children[static_cast<std::size_t>(row)].get());
}

QModelIndex DatabaseModel::parent(const QModelIndex& child) const {
    if (!child.isValid())
        return {};
    Node* n = nodeFor(child);
    Node* p = n->parent;
    if (!p || p == root_.get())
        return {};
    return createIndex(rowOf(p), 0, p);
}

int DatabaseModel::rowCount(const QModelIndex& parent) const {
    if (parent.column() > 0)
        return 0;
    return static_cast<int>(nodeFor(parent)->children.size());
}

int DatabaseModel::columnCount(const QModelIndex&) const {
    return ColumnCount;
}

QVariant DatabaseModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid())
        return {};
    Node* n = nodeFor(index);
    if (role == Qt::DisplayRole || role == Qt::EditRole)
        return index.column() == NameColumn ? QVariant(n->name) : QVariant(n->details);
    if (role == Qt::ToolTipRole)
        return n->details;
    return {};
}

QVariant DatabaseModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return {};
    return section == NameColumn ? QVariant(QStringLiteral("Name"))
                                 : QVariant(QStringLiteral("Details"));
}

DatabaseModel::Node* DatabaseModel::nodeFor(const QModelIndex& index) const {
    return index.isValid() ? static_cast<Node*>(index.internalPointer()) : root_.get();
}

int DatabaseModel::rowOf(const Node* node) const {
    if (!node->parent)
        return 0;
    const auto& siblings = node->parent->children;
    for (std::size_t i = 0; i < siblings.size(); ++i)
        if (siblings[i].get() == node)
            return static_cast<int>(i);
    return 0;
}

} // namespace memdb::viewer
