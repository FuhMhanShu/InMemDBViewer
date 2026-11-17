#pragma once

#include <memdb/discovery.hpp>

#include <QObject>
#include <QString>

#include <optional>

namespace memdb::viewer::detail {

/// @brief Model (MVC) der Confusion-Matrix (InMemDB v0.42): entdeckte FKs vs.
///        deklarierte Referenzen (`AnalysisReport::confusion` bzw. `evaluateConfusion`).
///        `nullopt`, wenn keine `__references`-Collection existiert oder der Umfang
///        nicht die ganze Database ist.
class ConfusionModel : public QObject {
    Q_OBJECT
public:
    explicit ConfusionModel(QObject* parent = nullptr) : QObject(parent) {}

    void setConfusion(const std::optional<memdb::ConfusionMatrix>& cm) {
        cm_ = cm;
        emit changed();
    }
    const std::optional<memdb::ConfusionMatrix>& confusion() const { return cm_; }
    bool hasConfusion() const { return cm_.has_value(); }

    /// @brief kompakte Zusammenfassung, z. B. "TP 1 · FP 0 · FN 1 · TN 30 · P 1,00 · R 0,50 · F1 0,67".
    QString summary() const {
        if (!cm_)
            return QStringLiteral("keine Confusion-Matrix (nur bei Umfang „Ganze Database“ mit __references)");
        const auto& c = *cm_;
        return QStringLiteral("TP %1 · FP %2 · FN %3 · TN %4 · Precision %5 · Recall %6 · F1 %7")
            .arg(c.tp)
            .arg(c.fp)
            .arg(c.fn)
            .arg(c.tn)
            .arg(c.precision, 0, 'f', 2)
            .arg(c.recall, 0, 'f', 2)
            .arg(c.f1, 0, 'f', 2);
    }

signals:
    void changed();

private:
    std::optional<memdb::ConfusionMatrix> cm_;
};

} // namespace memdb::viewer::detail
