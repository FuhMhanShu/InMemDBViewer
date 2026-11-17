#include "analysis_model.hpp"

#include <memdb/value.hpp>

namespace memdb::viewer::detail {

static QString num(double v, int prec = 3) { return QString::number(v, 'f', prec); }

QVariant CollectionOverviewModel::headerData(int s, Qt::Orientation o, int role) const {
    if (role != Qt::DisplayRole || o != Qt::Horizontal)
        return {};
    switch (s) {
    case CollectionColumn: return QStringLiteral("Collection");
    case DocsColumn: return QStringLiteral("Dokumente");
    case CategoryColumn: return QStringLiteral("Kategorie (FA-29)");
    case ConfidenceColumn: return QStringLiteral("Konfidenz");
    case NumericRatioColumn: return QStringLiteral("numerisch");
    case TopDriverColumn: return QStringLiteral("Top-Treiber (FA-31)");
    case OutlierFieldsColumn: return QStringLiteral("Ausreißer-Felder");
    case OutliersColumn: return QStringLiteral("Ausreißer");
    case RationaleColumn: return QStringLiteral("Begründung");
    default: return {};
    }
}

QVariant CollectionOverviewModel::data(const QModelIndex& i, int role) const {
    if (!i.isValid() || i.row() >= static_cast<int>(rows_.size()) || (role != Qt::DisplayRole && role != Qt::ToolTipRole))
        return {};
    const auto& r = rows_[static_cast<std::size_t>(i.row())];
    switch (i.column()) {
    case CollectionColumn: return r.collection;
    case DocsColumn: return QVariant::fromValue<qulonglong>(r.docs);
    case CategoryColumn: return r.category;
    case ConfidenceColumn: return num(r.categoryConfidence);
    case NumericRatioColumn: return num(r.numericRatio);
    case TopDriverColumn: return r.topDriver;
    case OutlierFieldsColumn: return QVariant::fromValue<qulonglong>(r.outlierFields);
    case OutliersColumn: return QVariant::fromValue<qulonglong>(r.totalOutliers);
    case RationaleColumn: return r.rationale;
    default: return {};
    }
}

QVariant FieldStatsModel::headerData(int s, Qt::Orientation o, int role) const {
    if (role != Qt::DisplayRole || o != Qt::Horizontal)
        return {};
    switch (s) {
    case CollectionColumn: return QStringLiteral("Collection");
    case FieldColumn: return QStringLiteral("Feld");
    case CountColumn: return QStringLiteral("n");
    case MinColumn: return QStringLiteral("Min");
    case MaxColumn: return QStringLiteral("Max");
    case MeanColumn: return QStringLiteral("Mittel");
    case StddevColumn: return QStringLiteral("Std");
    case MedianColumn: return QStringLiteral("Median");
    case SkewColumn: return QStringLiteral("Schiefe");
    case DiscreteColumn: return QStringLiteral("diskret");
    case GridColumn: return QStringLiteral("Raster");
    case BenfordColumn: return QStringLiteral("Benford");
    case NoteColumn: return QStringLiteral("Notiz");
    default: return {};
    }
}

QVariant FieldStatsModel::data(const QModelIndex& i, int role) const {
    if (!i.isValid() || i.row() >= static_cast<int>(fields_.size()) || (role != Qt::DisplayRole && role != Qt::ToolTipRole))
        return {};
    const auto& f = fields_[static_cast<std::size_t>(i.row())];
    switch (i.column()) {
    case CollectionColumn: return QString::fromStdString(f.collection);
    case FieldColumn: return QString::fromStdString(f.field);
    case CountColumn: return QVariant::fromValue<qulonglong>(f.count);
    case MinColumn: return num(f.min);
    case MaxColumn: return num(f.max);
    case MeanColumn: return num(f.mean);
    case StddevColumn: return num(f.stddev);
    case MedianColumn: return num(f.median);
    case SkewColumn: return num(f.skewness);
    case DiscreteColumn: return f.discrete ? QStringLiteral("ja") : QStringLiteral("nein");
    case GridColumn: return f.hasGrid ? num(f.grid, 4) : QString();
    case BenfordColumn:
        return f.benfordApplicable ? (f.benfordSuspicious ? QStringLiteral("auffällig") : QStringLiteral("ok"))
                                   : QStringLiteral("–");
    case NoteColumn: return QString::fromStdString(f.note);
    default: return {};
    }
}

QVariant InfluenceModel::headerData(int s, Qt::Orientation o, int role) const {
    if (role != Qt::DisplayRole || o != Qt::Horizontal)
        return {};
    switch (s) {
    case ScopeColumn: return QStringLiteral("Scope");
    case FieldColumn: return QStringLiteral("Feld");
    case RoleColumn: return QStringLiteral("Rolle");
    case ScoreColumn: return QStringLiteral("Einfluss");
    case MaxPearsonColumn: return QStringLiteral("max |r|");
    case PartnerColumn: return QStringLiteral("stärkster Partner");
    case RationaleColumn: return QStringLiteral("Begründung");
    default: return {};
    }
}

QVariant InfluenceModel::data(const QModelIndex& i, int role) const {
    if (!i.isValid() || i.row() >= static_cast<int>(rows_.size()) || (role != Qt::DisplayRole && role != Qt::ToolTipRole))
        return {};
    const auto& r = rows_[static_cast<std::size_t>(i.row())];
    switch (i.column()) {
    case ScopeColumn: return r.scope;
    case FieldColumn: return r.field;
    case RoleColumn: return r.role;
    case ScoreColumn: return num(r.score);
    case MaxPearsonColumn: return num(r.maxPearson);
    case PartnerColumn: return r.partner;
    case RationaleColumn: return r.rationale;
    default: return {};
    }
}

QVariant OutlierModel::headerData(int s, Qt::Orientation o, int role) const {
    if (role != Qt::DisplayRole || o != Qt::Horizontal)
        return {};
    switch (s) {
    case CollectionColumn: return QStringLiteral("Collection");
    case FieldColumn: return QStringLiteral("Feld");
    case CountColumn: return QStringLiteral("n");
    case OutliersColumn: return QStringLiteral("Ausreißer");
    case RateColumn: return QStringLiteral("Rate");
    case MethodColumn: return QStringLiteral("Methode");
    case MeanColumn: return QStringLiteral("Mittel");
    case StddevColumn: return QStringLiteral("Std");
    case ValuesColumn: return QStringLiteral("Werte");
    default: return {};
    }
}

QVariant OutlierModel::data(const QModelIndex& i, int role) const {
    if (!i.isValid() || i.row() >= static_cast<int>(rows_.size()) || (role != Qt::DisplayRole && role != Qt::ToolTipRole))
        return {};
    const auto& r = rows_[static_cast<std::size_t>(i.row())];
    switch (i.column()) {
    case CollectionColumn: return r.collection;
    case FieldColumn: return r.field;
    case CountColumn: return QVariant::fromValue<qulonglong>(r.count);
    case OutliersColumn: return QVariant::fromValue<qulonglong>(r.outliers);
    case RateColumn: return num(r.rate);
    case MethodColumn: return r.method;
    case MeanColumn: return num(r.mean);
    case StddevColumn: return num(r.stddev);
    case ValuesColumn: return r.values;
    default: return {};
    }
}

} // namespace memdb::viewer::detail
