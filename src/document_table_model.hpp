#pragma once

#include <memdb/collection.hpp>

#include <QAbstractTableModel>
#include <QStringList>

#include <optional>
#include <vector>

namespace memdb::viewer::detail {

/// @brief Model (MVC) der tabellarischen Datenansicht: Zeilen = Dokumente einer
///        Collection (virtualisiert: nur IDs via `findIds`, Zellen on demand via
///        `findById` – keine Voll-Kopien), Spalten = "id" + Feldpfade (Top-Level
///        plus eine Ebene punkt-notierte Unterfelder). Optionaler DSL-Filter
///        (`Collection::find(dsl)`) inkl. Index-Diagnose.
class DocumentTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Roles { IdRole = Qt::UserRole + 1, TypeRole };

    explicit DocumentTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

    void setCollection(std::optional<memdb::Collection> coll);
    bool hasCollection() const { return coll_.has_value(); }
    QString collectionName() const;

    /// @brief DSL-Filter anwenden; false + error bei QueryError.
    bool applyQuery(const QString& dsl, QString* error = nullptr);
    /// @brief Fertiges Query-Ergebnis uebernehmen (z. B. asynchron im Worker-Thread gerechnet, UI-09).
    void setQueryIds(const QString& dsl, std::vector<memdb::DocumentId> ids, bool usedIndex,
                     std::size_t candidates);
    void clearQuery();
    QString query() const { return query_; }

    /// @brief Index-Diagnose der letzten Query (FA-10/v0.22).
    bool lastQueryUsedIndex() const { return lastUsedIndex_; }
    std::size_t lastQueryCandidateCount() const { return lastCandidates_; }

    void refresh();

    quint64 idAt(int row) const;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : static_cast<int>(ids_.size());
    }
    int columnCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : static_cast<int>(columns_.size());
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    /// @brief Zellen editierbar (ausser id-Spalte); `setData` loest nur `editRequested`
    ///        aus – die Mutation uebernimmt der Controller verifiziert ueber den ViewerCore (DE-05).
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

signals:
    /// @brief Zellen-Edit angefordert; id + Feldpfad + neuer Wert als Text.
    void editRequested(quint64 id, const QString& fieldPath, const QString& value);

private:
    void doRefresh(); // ohne begin/endResetModel (intern)
    void rebuildColumns();

    std::optional<memdb::Collection> coll_;
    QString query_;
    std::vector<memdb::DocumentId> ids_;
    QStringList columns_;
    bool lastUsedIndex_ = false;
    std::size_t lastCandidates_ = 0;
};

} // namespace memdb::viewer::detail
