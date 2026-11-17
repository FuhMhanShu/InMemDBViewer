#pragma once

#include <memdb/cross_influence.hpp>
#include <memdb/discovery.hpp>
#include <memdb/influence.hpp>
#include <memdb/numeric_analysis.hpp>

#include <QObject>
#include <QString>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

namespace memdb::viewer {
class ViewerCore;
namespace detail {
class AnalysisModel;
class CollectionOverviewModel;
class ConfusionModel;
class DiscoveryModel;
class FieldStatsModel;
class InfluenceModel;
class OutlierModel;
class TextListModel;
}
} // namespace memdb::viewer

namespace memdb::viewer::detail {

/// @brief Controller (MVC) des AnalysisWidgets: fuehrt Discovery (FA-27) + Analyse
///        (FA-29…FA-32, v0.33 Health-Score, v0.40 Ausreisser) in einem Worker-Thread
///        aus (auf einer Deep-Copy der Database – thread-sicher, auch wenn der Core
///        Meanwhile eine neue Database adoptiert). Ergebnisse landen per queued
///        Aufruf in den Models (GUI-Thread). Kennt keine Widgets.
class AnalysisController : public QObject {
    Q_OBJECT
public:
    AnalysisController(ViewerCore* core, DiscoveryModel* discovery, TextListModel* orphans,
                       TextListModel* merges, TextListModel* keys, TextListModel* paths,
                       TextListModel* profiles, CollectionOverviewModel* overview,
                       FieldStatsModel* fieldStats, InfluenceModel* influence, OutlierModel* outliers,
                       ConfusionModel* confusion, QObject* parent = nullptr);
    ~AnalysisController() override;

    void setCore(ViewerCore* core);

    memdb::DiscoveryOptions discoveryOptions;
    memdb::NumericAnalysisOptions numericOptions;
    memdb::InfluenceOptions influenceOptions;
    memdb::CrossInfluenceOptions crossInfluenceOptions;

    /// @brief Analyse starten; `scopeCollection` leer = ganze Database, sonst nur diese Collection.
    void run(const QString& scopeCollection = QString());
    void cancel();
    bool isRunning() const { return running_.load(); }

    double healthScore() const { return healthScore_; }
    const std::optional<memdb::ConfusionMatrix>& confusion() const { return confusion_; }
    QString reportMarkdown() const { return reportMarkdown_; }
    QString reportJson() const { return reportJson_; }
    QString reportMermaid() const { return reportMermaid_; }

signals:
    void started();
    void progress(const QString& step);
    /// @brief fertig; ok=false bei Abbruch/Fehler; summary = Health-Score/Zusammenfassung.
    void finished(bool ok, const QString& summary);
    void statusMessage(const QString& message);

private:
    void runWorker(const QString scopeCollection);
    void deliver(std::function<void()> fn); // queued in den GUI-Thread

    ViewerCore* core_ = nullptr;
    DiscoveryModel* discovery_;
    TextListModel* orphans_;
    TextListModel* merges_;
    TextListModel* keys_;
    TextListModel* paths_;
    TextListModel* profiles_;
    CollectionOverviewModel* overview_;
    FieldStatsModel* fieldStats_;
    InfluenceModel* influence_;
    OutlierModel* outliers_;
    ConfusionModel* confusionModel_;

    std::thread worker_;
    std::mutex mutex_;
    std::atomic<bool> running_{false};
    std::atomic<bool> cancel_{false};
    double healthScore_ = 0.0;
    std::optional<memdb::ConfusionMatrix> confusion_;
    QString reportMarkdown_, reportJson_, reportMermaid_;
};

} // namespace memdb::viewer::detail
