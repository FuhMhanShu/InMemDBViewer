#pragma once

#include <memdb/database.hpp>

#include <QAbstractTableModel>
#include <QStringList>

namespace memdb::viewer::detail {

/// @brief Model (MVC) der Datenbank-Uebersicht: Top-Level-Zeilen = Collections
///        (Name, Dokumentanzahl, Schema-Status, Indexanzahl). Interne Collections
///        (Prefix "__", z. B. "__references") werden ausgeblendet.
class DatabaseModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column { NameColumn = 0, DocsColumn, SchemaColumn, IndexesColumn, ColumnCount };

    explicit DatabaseModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

    void setDatabase(memdb::Database* db) {
        beginResetModel();
        db_ = db;
        refresh();
        endResetModel();
    }
    memdb::Database* database() const { return db_; }

    void refresh() {
        names_.clear();
        if (db_) {
            for (const auto& n : db_->collectionNames()) {
                if (n.rfind("__", 0) == 0)
                    continue;
                names_ << QString::fromStdString(n);
            }
        }
        if (!names_.isEmpty())
            emit dataChanged(index(0, 0), index(rowCount() - 1, ColumnCount - 1));
    }

    QString collectionAt(int row) const {
        return (row >= 0 && row < names_.size()) ? names_.at(row) : QString();
    }
    QModelIndex indexOf(const QString& name) const {
        const int row = names_.indexOf(name);
        return row >= 0 ? index(row, 0) : QModelIndex();
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : names_.size();
    }
    int columnCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : ColumnCount;
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
        if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
            return {};
        switch (section) {
        case NameColumn: return QStringLiteral("Collection");
        case DocsColumn: return QStringLiteral("Dokumente");
        case SchemaColumn: return QStringLiteral("Schema");
        case IndexesColumn: return QStringLiteral("Indizes");
        default: return {};
        }
    }
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || !db_ || index.row() >= names_.size())
            return {};
        if (role != Qt::DisplayRole && role != Qt::ToolTipRole)
            return {};
        const std::string name = names_.at(index.row()).toStdString();
        if (!db_->hasCollection(name))
            return {};
        auto coll = db_->collection(name);
        switch (index.column()) {
        case NameColumn: return names_.at(index.row());
        case DocsColumn: return QVariant::fromValue<qulonglong>(coll.size());
        case SchemaColumn: {
            const bool has = coll.hasSchema();
            const auto schema = coll.schema();
            if (role == Qt::ToolTipRole && has && schema)
                return QStringLiteral("%1 Felder").arg(schema->size());
            return has ? QStringLiteral("ja") : QStringLiteral("nein");
        }
        case IndexesColumn: return QVariant::fromValue<int>(static_cast<int>(coll.indexes().size()));
        default: return {};
        }
    }

private:
    memdb::Database* db_ = nullptr;
    QStringList names_;
};

} // namespace memdb::viewer::detail
