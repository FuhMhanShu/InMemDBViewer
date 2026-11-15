#pragma once

#include <memdb/database.hpp>

#include <QAbstractItemModel>
#include <QString>

#include <memory>
#include <vector>

namespace memdb::viewer {

/// @brief Model (MVC): Baum ueber einer memdb::Database (FA-03).
///
/// Top-Level-Zeilen = Collections (Spalte Name; Details: Dokumentanzahl,
/// Schema-Status, Indexanzahl). Kind-Zeilen = Schema-Felder (FA-20) bzw.
/// index-only Felder (FA-10) mit Typ, nullable/notNull und Index-Markierung.
class DatabaseModel : public QAbstractItemModel {
    Q_OBJECT
public:
    enum Column { NameColumn = 0, DetailsColumn = 1, ColumnCount = 2 };

    explicit DatabaseModel(QObject* parent = nullptr);

    /// @brief Zu modellierende Database setzen (nicht-owning; muss das Model ueberdauern).
    void setDatabase(memdb::Database* db);
    memdb::Database* database() const { return db_; }

    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

public slots:
    /// @brief Baum komplett neu aus der Database aufbauen (z. B. nach Observer-Event, FA-08).
    void refresh();

private:
    struct Node {
        enum class Kind { Root, Collection, Field };
        Kind kind = Kind::Root;
        QString name;
        QString details;
        Node* parent = nullptr;
        std::vector<std::unique_ptr<Node>> children;
    };

    Node* nodeFor(const QModelIndex& index) const;
    int rowOf(const Node* node) const;
    void rebuild();

    memdb::Database* db_ = nullptr;
    std::unique_ptr<Node> root_;
};

} // namespace memdb::viewer
