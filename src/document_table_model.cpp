#include "document_table_model.hpp"

#include <memdb/exceptions.hpp>
#include <memdb/query_dsl.hpp>

#include <QSet>

namespace {
/// @brief Anzeige-Text eines Werts: Strings ungequotet, Binary gekuerzt.
QString displayValue(const memdb::Value& v) {
    switch (v.type()) {
    case memdb::Value::Type::String:
        return QString::fromStdString(v.asString());
    case memdb::Value::Type::Binary: {
        const auto& b = v.asBinary();
        return QStringLiteral("<binary %1 Bytes>").arg(b.size());
    }
    default: {
        QString s = QString::fromStdString(memdb::toString(v));
        if (s.size() > 120)
            s = s.left(117) + QStringLiteral("…");
        return s;
    }
    }
}
} // namespace

namespace memdb::viewer::detail {

void DocumentTableModel::setCollection(std::optional<memdb::Collection> coll) {
    beginResetModel();
    coll_ = std::move(coll);
    query_.clear();
    ids_.clear();
    columns_.clear();
    lastUsedIndex_ = false;
    lastCandidates_ = 0;
    doRefresh();
    endResetModel();
}

QString DocumentTableModel::collectionName() const {
    return coll_ ? QString::fromStdString(coll_->name()) : QString();
}

bool DocumentTableModel::applyQuery(const QString& dsl, QString* error) {
    if (!coll_) {
        if (error)
            *error = QStringLiteral("Keine Collection ausgewaehlt.");
        return false;
    }
    try {
        memdb::parseQuery(dsl.toStdString()); // validieren, bevor query_ uebernommen wird
    } catch (const std::exception& e) {
        if (error)
            *error = QString::fromUtf8(e.what());
        return false;
    }
    query_ = dsl;
    refresh();
    return true;
}

void DocumentTableModel::clearQuery() {
    query_.clear();
    refresh();
}

void DocumentTableModel::refresh() {
    beginResetModel();
    doRefresh();
    endResetModel();
}

void DocumentTableModel::doRefresh() {
    if (!coll_)
        return;
    try {
        if (query_.isEmpty())
            ids_ = coll_->findIds();
        else
            ids_ = coll_->findIds(query_.toStdString());
        lastUsedIndex_ = coll_->metrics().lastQueryUsedIndex;
        lastCandidates_ = coll_->lastQueryCandidateCount();
    } catch (const std::exception&) {
        ids_.clear();
        lastUsedIndex_ = false;
        lastCandidates_ = 0;
    }
    rebuildColumns();
}

quint64 DocumentTableModel::idAt(int row) const {
    return (row >= 0 && row < static_cast<int>(ids_.size())) ? ids_[static_cast<std::size_t>(row)].value()
                                                              : 0;
}

QVariant DocumentTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal || section >= columns_.size())
        return {};
    return columns_.at(section);
}

QVariant DocumentTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || !coll_ || index.row() >= static_cast<int>(ids_.size()) ||
        index.column() >= columns_.size())
        return {};
    const std::string path = columns_.at(index.column()).toStdString();
    if (role == IdRole)
        return QVariant::fromValue<qulonglong>(ids_[static_cast<std::size_t>(index.row())].value());
    const auto doc = coll_->findById(ids_[static_cast<std::size_t>(index.row())]);
    const memdb::Value* v = nullptr;
    if (doc) {
        if (path == "id")
            return QVariant::fromValue<qulonglong>(doc->id().value());
        v = doc->findPath(path);
    }
    if (!v)
        return role == Qt::DisplayRole ? QStringLiteral("–") : QVariant();
    if (role == TypeRole)
        return QString::fromStdString(memdb::typeName(v->type()));
    if (role != Qt::DisplayRole && role != Qt::ToolTipRole)
        return {};
    return displayValue(*v);
}

void DocumentTableModel::rebuildColumns() {
    columns_.clear();
    columns_ << QStringLiteral("id");
    if (!coll_)
        return;
    // Spalten: Schema-Feldpfade, sonst Vereinigung der Feldnamen (Sample, gedeckelt)
    QSet<QString> fields;
    if (const auto schema = coll_->schema()) {
        for (const auto& [path, fs] : schema->fields())
            fields.insert(QString::fromStdString(path));
    } else {
        std::size_t seen = 0;
        coll_->forEach(memdb::Expression{}, [&](const memdb::Document& doc) {
            if (++seen > 500)
                return;
            doc.forEachField([&](const std::string& key, const memdb::Value& val) {
                fields.insert(QString::fromStdString(key));
                if (val.isDocument() && val.asDocument()) {
                    val.asDocument()->forEachField([&](const std::string& sub, const memdb::Value&) {
                        fields.insert(QString::fromStdString(key) + QStringLiteral(".") +
                                       QString::fromStdString(sub));
                    });
                }
            });
        });
    }
    const QStringList sorted = QSet<QString>(fields).values();
    for (const QString& f : sorted) {
        if (columns_.size() >= 65)
            break;
        columns_ << f;
    }
}

} // namespace memdb::viewer::detail
