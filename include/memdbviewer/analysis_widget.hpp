#pragma once

#include <QWidget>
#include <QString>

namespace memdb::viewer {
class ViewerCore;
namespace detail {
class AnalysisController;
}
} // namespace memdb::viewer

namespace memdb::viewer {

/// @brief Gekapseltes MVC-Widget „Analyse" (AN-01…AN-07): Discovery (FA-27:
///        Relationen/Orphans/Pfade/Merges/Candidate-Keys/Profile) und Analyse
///        (FA-29 Charakter, FA-30 numerisch, FA-31/FA-32 Einfluss, v0.40 Ausreißer,
///        v0.33 Health-Score) – ausgefuehrt in einem Worker-Thread mit Abbrechen,
///        Optionen-Dialog und Report-Export (Markdown/JSON/Mermaid).
///        **Auto-Analyse (DE-06)**: nach jeder Datenmanipulation (`ViewerCore::dataMutated`,
///        z. B. Design/CRUD/Import) wird Discovery + Analyse automatisch nach kurzem
///        Debounce neu ausgeführt; abschaltbar über „Auto-Analyse nach Änderung“.
///
/// Verwendung: `auto* analysis = new AnalysisWidget; analysis->setCore(core);`
class AnalysisWidget : public QWidget {
    Q_OBJECT
public:
    explicit AnalysisWidget(QWidget* parent = nullptr);
    ~AnalysisWidget() override;

    void setCore(ViewerCore* core);
    ViewerCore* core() const;

    bool isRunning() const;
    double healthScore() const;

    /// @brief Auto-Analyse nach jeder Datenmanipulation (DE-06) an/aus; Default: an.
    void setAutoAnalysisEnabled(bool on);
    bool autoAnalysisEnabled() const;

    // Introspection fuer Tests/Diagnose
    int discoveryRowCount() const;
    int overviewRowCount() const;
    int fieldStatsRowCount() const;
    int influenceRowCount() const;
    int outlierRowCount() const;
    /// @brief Confusion-Matrix (InMemDB v0.42) vorhanden? Nur bei Umfang „Ganze Database“ mit __references.
    bool hasConfusion() const;
    QString confusionSummary() const;

public slots:
    /// @brief Analyse starten (ganze Database oder gesetzter Scope).
    void runAnalysis();
    void cancelAnalysis();
    /// @brief Umfang: leere QString = ganze Database, sonst nur diese Collection.
    void setScopeCollection(const QString& name);
    QString scopeCollection() const;
    /// @brief Slot für `ViewerCore::dataMutated` (DE-06): startet die Analyse nach
    ///        Debounce (~400 ms); läuft sie gerade, wird ein erneuter Lauf vorgemerkt.
    void onDataMutated();

signals:
    void statusMessage(const QString& message);
    void analysisStarted();
    void analysisFinished(bool ok, const QString& summary);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace memdb::viewer
