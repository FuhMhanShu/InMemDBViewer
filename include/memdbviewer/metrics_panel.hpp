#pragma once

#include <memdb/collection.hpp>
#include <memdb/database.hpp>

#include <QWidget>

#include <optional>

class QLabel;

namespace memdb::viewer {

/// @brief View (MVC): Metrik-/Diagnose-Panel (FA-11) – CollectionMetrics (v0.22)
///        oder aggregierte DatabaseMetrics, plus Hybrid-/Overflow-Info (FA-13).
class MetricsPanel : public QWidget {
    Q_OBJECT
public:
    explicit MetricsPanel(QWidget* parent = nullptr);

    /// @brief Metriken einer Collection anzeigen (nicht-owning Handle).
    void showCollection(memdb::Collection collection);
    /// @brief Aggregierte Metriken der gesamten Database anzeigen.
    void showDatabase(memdb::Database& db);
    /// @brief Aktueller Anzeigetext (u. a. fuer Tests).
    QString bodyText() const;

public slots:
    /// @brief Anzeige aus den aktuellen Metriken neu aufbauen.
    void refresh();

private:
    void render();

    std::optional<memdb::Collection> coll_;
    memdb::Database* db_ = nullptr;
    bool databaseMode_ = false;
    QLabel* title_ = nullptr;
    QLabel* body_ = nullptr;
};

} // namespace memdb::viewer
