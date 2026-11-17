#pragma once

#include <memdb/character.hpp>
#include <memdb/cross_influence.hpp>
#include <memdb/discovery.hpp>
#include <memdb/influence.hpp>
#include <memdb/numeric_analysis.hpp>

#include <QAbstractTableModel>
#include <QString>

#include <vector>

namespace memdb::viewer::detail {

/// @brief Uebersicht pro Collection (Analyse-Tab): Dokumente, Datencharakter (FA-29),
///        numerischer Anteil, Top-Treiber (FA-31), Ausreisser-Anzahl (v0.40).
struct CollectionOverviewRow {
    QString collection;
    std::size_t docs = 0;
    QString category;
    double categoryConfidence = 0.0;
    QString rationale;
    double numericRatio = 0.0;
    QString topDriver;
    std::size_t outlierFields = 0;
    std::size_t totalOutliers = 0;
};

class CollectionOverviewModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column {
        CollectionColumn = 0,
        DocsColumn,
        CategoryColumn,
        ConfidenceColumn,
        NumericRatioColumn,
        TopDriverColumn,
        OutlierFieldsColumn,
        OutliersColumn,
        RationaleColumn,
        ColumnCount
    };
    explicit CollectionOverviewModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}
    void setRows(const std::vector<CollectionOverviewRow>& rows) {
        beginResetModel();
        rows_ = rows;
        endResetModel();
    }
    const std::vector<CollectionOverviewRow>& rows() const { return rows_; }
    int rowCount(const QModelIndex& p = QModelIndex()) const override { return p.isValid() ? 0 : static_cast<int>(rows_.size()); }
    int columnCount(const QModelIndex& p = QModelIndex()) const override { return p.isValid() ? 0 : ColumnCount; }
    QVariant headerData(int s, Qt::Orientation o, int role = Qt::DisplayRole) const override;
    QVariant data(const QModelIndex& i, int role = Qt::DisplayRole) const override;
private:
    std::vector<CollectionOverviewRow> rows_;
};

/// @brief Numerische Feld-Statistiken (FA-30) ueber alle Collections; inkl. Histogramm-Daten.
class FieldStatsModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column {
        CollectionColumn = 0,
        FieldColumn,
        CountColumn,
        MinColumn,
        MaxColumn,
        MeanColumn,
        StddevColumn,
        MedianColumn,
        SkewColumn,
        DiscreteColumn,
        GridColumn,
        BenfordColumn,
        NoteColumn,
        ColumnCount
    };
    explicit FieldStatsModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}
    void setFields(const std::vector<memdb::NumericFieldAnalysis>& fields) {
        beginResetModel();
        fields_ = fields;
        endResetModel();
    }
    const std::vector<memdb::NumericFieldAnalysis>& fields() const { return fields_; }
    const memdb::NumericFieldAnalysis* fieldAt(int row) const {
        return (row >= 0 && row < static_cast<int>(fields_.size())) ? &fields_[static_cast<std::size_t>(row)]
                                                                     : nullptr;
    }
    int rowCount(const QModelIndex& p = QModelIndex()) const override { return p.isValid() ? 0 : static_cast<int>(fields_.size()); }
    int columnCount(const QModelIndex& p = QModelIndex()) const override { return p.isValid() ? 0 : ColumnCount; }
    QVariant headerData(int s, Qt::Orientation o, int role = Qt::DisplayRole) const override;
    QVariant data(const QModelIndex& i, int role = Qt::DisplayRole) const override;
private:
    std::vector<memdb::NumericFieldAnalysis> fields_;
};

/// @brief Einfluss-Analyse (FA-31 intra-Collection + FA-32 cross-Collection).
struct InfluenceRow {
    QString scope;      ///< Collection bzw. "A -> B (relation)"
    QString field;      ///< bei cross: "coll.feld"
    QString role;       ///< driver/dependent/mediator/independent
    double score = 0.0;
    double maxPearson = 0.0;
    QString partner;
    QString rationale;
};

class InfluenceModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column { ScopeColumn = 0, FieldColumn, RoleColumn, ScoreColumn, MaxPearsonColumn, PartnerColumn, RationaleColumn, ColumnCount };
    explicit InfluenceModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}
    void setRows(const std::vector<InfluenceRow>& rows) {
        beginResetModel();
        rows_ = rows;
        endResetModel();
    }
    const std::vector<InfluenceRow>& rows() const { return rows_; }
    int rowCount(const QModelIndex& p = QModelIndex()) const override { return p.isValid() ? 0 : static_cast<int>(rows_.size()); }
    int columnCount(const QModelIndex& p = QModelIndex()) const override { return p.isValid() ? 0 : ColumnCount; }
    QVariant headerData(int s, Qt::Orientation o, int role = Qt::DisplayRole) const override;
    QVariant data(const QModelIndex& i, int role = Qt::DisplayRole) const override;
private:
    std::vector<InfluenceRow> rows_;
};

/// @brief Ausreisser (v0.40): pro numerischem Feld z-Score/IQR-Befunde.
struct OutlierRow {
    QString collection, field;
    std::size_t count = 0, outliers = 0;
    double rate = 0.0, mean = 0.0, stddev = 0.0;
    QString method;
    QString values;
};

class OutlierModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column { CollectionColumn = 0, FieldColumn, CountColumn, OutliersColumn, RateColumn, MethodColumn, MeanColumn, StddevColumn, ValuesColumn, ColumnCount };
    explicit OutlierModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}
    void setRows(const std::vector<OutlierRow>& rows) {
        beginResetModel();
        rows_ = rows;
        endResetModel();
    }
    const std::vector<OutlierRow>& rows() const { return rows_; }
    int rowCount(const QModelIndex& p = QModelIndex()) const override { return p.isValid() ? 0 : static_cast<int>(rows_.size()); }
    int columnCount(const QModelIndex& p = QModelIndex()) const override { return p.isValid() ? 0 : ColumnCount; }
    QVariant headerData(int s, Qt::Orientation o, int role = Qt::DisplayRole) const override;
    QVariant data(const QModelIndex& i, int role = Qt::DisplayRole) const override;
private:
    std::vector<OutlierRow> rows_;
};

} // namespace memdb::viewer::detail
