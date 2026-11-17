#pragma once

#include <memdb/discovery.hpp>

#include <QAbstractTableModel>
#include <QStringList>

#include <vector>

namespace memdb::viewer::detail {

/// @brief Model (MVC) der Discovery-Relationen: eine Zeile pro `DiscoveredRelation`
///        (Art, von/nach, Support, Confidence, Kardinalitaet, Sample, Notiz).
class DiscoveryModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column {
        KindColumn = 0,
        FromColumn,
        ToColumn,
        SupportColumn,
        ConfidenceColumn,
        CardinalityColumn,
        SampleColumn,
        NoteColumn,
        ColumnCount
    };

    explicit DiscoveryModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

    void setRelations(const std::vector<memdb::DiscoveredRelation>& rels) {
        beginResetModel();
        rels_ = rels;
        endResetModel();
    }
    const std::vector<memdb::DiscoveredRelation>& relations() const { return rels_; }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : static_cast<int>(rels_.size());
    }
    int columnCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : ColumnCount;
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
        if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
            return {};
        switch (section) {
        case KindColumn: return QStringLiteral("Art");
        case FromColumn: return QStringLiteral("Von (Collection.Feld)");
        case ToColumn: return QStringLiteral("Nach (Collection.Feld)");
        case SupportColumn: return QStringLiteral("Support");
        case ConfidenceColumn: return QStringLiteral("Confidence");
        case CardinalityColumn: return QStringLiteral("Kardinalitaet");
        case SampleColumn: return QStringLiteral("Sample");
        case NoteColumn: return QStringLiteral("Notiz");
        default: return {};
        }
    }
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() >= static_cast<int>(rels_.size()))
            return {};
        if (role != Qt::DisplayRole && role != Qt::ToolTipRole)
            return {};
        const auto& r = rels_[static_cast<std::size_t>(index.row())];
        const QString from = QString::fromStdString(r.fromCollection + "." + r.field);
        const QString to = QString::fromStdString(r.toCollection + "." + r.toField);
        switch (index.column()) {
        case KindColumn: {
            switch (r.kind) {
            case memdb::RelationKind::ForeignKey: return QStringLiteral("FK");
            case memdb::RelationKind::FunctionalDependency: return QStringLiteral("FD");
            case memdb::RelationKind::Correlation: return QStringLiteral("Korrelation");
            case memdb::RelationKind::NearDuplicate: return QStringLiteral("Near-Dupe");
            }
            return {};
        }
        case FromColumn: return from;
        case ToColumn: return to;
        case SupportColumn: return r.support;
        case ConfidenceColumn: return r.confidence;
        case CardinalityColumn: return QString::fromStdString(r.cardinality);
        case SampleColumn: return QVariant::fromValue<qulonglong>(r.sampleSize);
        case NoteColumn: return QString::fromStdString(r.note);
        default: return {};
        }
    }

private:
    std::vector<memdb::DiscoveredRelation> rels_;
};

/// @brief Generisches Listen-Model (MVC) fuer Nebenbefunde der Discovery:
///        Orphans, Merge-Vorschlaege, Candidate Keys, Pfade, Profile – jeweils
///        als formatierte Zeile(n) pro Eintrag.
class TextListModel : public QAbstractListModel {
    Q_OBJECT
public:
    explicit TextListModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}
    void setEntries(const QStringList& entries) {
        beginResetModel();
        entries_ = entries;
        endResetModel();
    }
    const QStringList& entries() const { return entries_; }
    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : entries_.size();
    }
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() >= entries_.size() || (role != Qt::DisplayRole && role != Qt::ToolTipRole))
            return {};
        return entries_.at(index.row());
    }
private:
    QStringList entries_;
};

} // namespace memdb::viewer::detail
