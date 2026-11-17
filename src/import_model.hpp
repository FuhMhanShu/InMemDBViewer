#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <vector>

namespace memdb::viewer::detail {

/// @brief Model (MVC) des DataInputWidgets: Verlauf der Ladevorgaenge
///        (Datei, Quelle, Ergebnis, Zeitpunkt) + Liste der unterstuetzten Formate.
class ImportModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        PathRole = Qt::UserRole + 1,
        SourceRole,
        SummaryRole,
        OkRole,
        TimeRole,
    };

    struct Entry {
        QString path;
        QString source;
        QString summary;
        bool ok = false;
        QDateTime time;
    };

    explicit ImportModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : static_cast<int>(entries_.size());
    }
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(entries_.size()))
            return {};
        const Entry& e = entries_[static_cast<std::size_t>(index.row())];
        switch (role) {
        case Qt::DisplayRole:
            return QStringLiteral("%1  %2").arg(e.ok ? QStringLiteral("✓") : QStringLiteral("✗"), e.summary);
        case Qt::ToolTipRole:
            return e.path;
        case PathRole:
            return e.path;
        case SourceRole:
            return e.source;
        case SummaryRole:
            return e.summary;
        case OkRole:
            return e.ok;
        case TimeRole:
            return e.time;
        default:
            return {};
        }
    }

    void addResult(const QString& path, const QString& source, bool ok, const QString& summary) {
        beginInsertRows(QModelIndex(), 0, 0);
        entries_.insert(entries_.begin(),
                        Entry{path, source, summary, ok, QDateTime::currentDateTime()});
        if (entries_.size() > 50)
            entries_.pop_back();
        endInsertRows();
    }
    void updateLatest(const QString& path, bool ok, const QString& summary) {
        if (entries_.empty() || entries_.front().path != path) {
            addResult(path, QString(), ok, summary);
            return;
        }
        entries_.front().ok = ok;
        entries_.front().summary = summary;
        const QModelIndex idx = index(0, 0);
        emit dataChanged(idx, idx);
    }
    void clearHistory() {
        beginResetModel();
        entries_.clear();
        endResetModel();
    }
    const std::vector<Entry>& entries() const { return entries_; }
    QString pathAt(int row) const {
        return (row >= 0 && row < static_cast<int>(entries_.size())) ? entries_[static_cast<std::size_t>(row)].path
                                                                     : QString();
    }

private:
    std::vector<Entry> entries_;
};

} // namespace memdb::viewer::detail
