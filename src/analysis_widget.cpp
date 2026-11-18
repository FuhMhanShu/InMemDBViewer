#include "memdbviewer/analysis_widget.hpp"
#include "memdbviewer/viewer_core.hpp"

#include "analysis_controller.hpp"
#include "analysis_model.hpp"
#include "confusion_model.hpp"
#include "confusion_view.hpp"
#include "discovery_model.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QProgressBar>
#include <QStyle>
#include <QToolButton>
#include "tool_button.hpp"
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTableView>
#include <QTimer>
#include <QTreeView>
#include <QVBoxLayout>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace memdb::viewer {

namespace {
/// @brief Kleines Balkendiagramm (Histogramm) fuer ein `NumericFieldAnalysis` (QPainter).
class HistogramWidget : public QWidget {
public:
    explicit HistogramWidget(QWidget* parent = nullptr) : QWidget(parent) { setMinimumHeight(90); }
    void setHistogram(const std::vector<std::size_t>& bins, double binMin, double binWidth) {
        bins_ = bins;
        binMin_ = binMin;
        binWidth_ = binWidth;
        update();
    }
    void clear() {
        bins_.clear();
        update();
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), palette().color(QWidget::backgroundRole()));
        if (bins_.empty()) {
            p.setPen(palette().color(QPalette::Mid));
            p.drawText(rect(), Qt::AlignCenter, QStringLiteral("kein Histogramm – Feld selektieren"));
            return;
        }
        const std::size_t maxV = *std::max_element(bins_.begin(), bins_.end());
        if (maxV == 0)
            return;
        const int n = static_cast<int>(bins_.size());
        const qreal bw = width() / static_cast<qreal>(n);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(70, 130, 200));
        for (int i = 0; i < n; ++i) {
            const qreal h = height() * bins_[static_cast<std::size_t>(i)] / static_cast<qreal>(maxV);
            p.drawRect(QRectF(i * bw + 1, height() - h, bw - 2, h));
        }
        p.setPen(palette().color(QPalette::Text));
        p.drawText(QRectF(0, 0, width(), 14), Qt::AlignLeft,
                   QStringLiteral("[%1 … %2]").arg(binMin_, 0, 'g', 4).arg(binMin_ + binWidth_ * n, 0, 'g', 4));
    }
private:
    std::vector<std::size_t> bins_;
    double binMin_ = 0.0, binWidth_ = 0.0;
};

/// @brief Optionen-Dialog: DiscoveryOptions + NumericAnalysisOptions + Einfluss-Schwellen.
class AnalysisOptionsDialog : public QDialog {
public:
    AnalysisOptionsDialog(const memdb::DiscoveryOptions& d, const memdb::NumericAnalysisOptions& n,
                          const memdb::InfluenceOptions& i, QWidget* parent = nullptr)
        : QDialog(parent) {
        setWindowTitle(QStringLiteral("Analyse-Optionen"));
        auto* form = new QFormLayout(this);
        minSupport = addDouble(form, QStringLiteral("minSupport (FK)"), d.minSupport, 0.0, 1.0, 0.05);
        minConfidence = addDouble(form, QStringLiteral("minConfidence (FK/FD)"), d.minConfidence, 0.0, 1.0, 0.05);
        minCorrelation = addDouble(form, QStringLiteral("minCorrelation |r|"), d.minCorrelation, 0.0, 1.0, 0.05);
        minSimilarity = addDouble(form, QStringLiteral("minSimilarity (Near-Dupes)"), d.minSimilarity, 0.0, 1.0, 0.05);
        confMin = addDouble(form, QStringLiteral("Confusion-Matrix: FK-Confidence ab (v0.42)"), d.confusionMinConfidence, 0.0, 1.0, 0.05);
        sampleSize = addInt(form, QStringLiteral("Sample-Größe (0 = alle)"), d.sampleSize);
        seed = addInt(form, QStringLiteral("Seed"), d.seed);
        histBins = addInt(form, QStringLiteral("Histogramm-Balken"), n.histogramBins);
        minValues = addInt(form, QStringLiteral("min Werte/Feld (numerisch)"), n.minValuesPerField);
        strongCorr = addDouble(form, QStringLiteral("starker Zusammenhang |r| ab (FA-31)"), i.strongCorrelation, 0.0, 1.0, 0.05);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        form->addRow(buttons);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }
    memdb::DiscoveryOptions discovery() const {
        memdb::DiscoveryOptions d;
        d.minSupport = minSupport->value();
        d.minConfidence = minConfidence->value();
        d.minCorrelation = minCorrelation->value();
        d.minSimilarity = minSimilarity->value();
        d.confusionMinConfidence = confMin->value();
        d.sampleSize = static_cast<std::size_t>(sampleSize->value());
        d.seed = static_cast<std::uint64_t>(seed->value());
        return d;
    }
    memdb::NumericAnalysisOptions numeric() const {
        memdb::NumericAnalysisOptions n;
        n.sampleSize = static_cast<std::size_t>(sampleSize->value());
        n.seed = static_cast<std::uint64_t>(seed->value());
        n.histogramBins = static_cast<std::size_t>(histBins->value());
        n.minValuesPerField = static_cast<std::size_t>(minValues->value());
        return n;
    }
    memdb::InfluenceOptions influence() const {
        memdb::InfluenceOptions i;
        i.numeric = numeric();
        i.strongCorrelation = strongCorr->value();
        return i;
    }
private:
    static QDoubleSpinBox* addDouble(QFormLayout* form, const QString& label, double v, double lo, double hi, double step) {
        auto* s = new QDoubleSpinBox();
        s->setRange(lo, hi);
        s->setSingleStep(step);
        s->setValue(v);
        form->addRow(label, s);
        return s;
    }
    static QSpinBox* addInt(QFormLayout* form, const QString& label, long long v) {
        auto* s = new QSpinBox();
        s->setRange(0, 1000000);
        s->setValue(static_cast<int>(v));
        form->addRow(label, s);
        return s;
    }
    QDoubleSpinBox *minSupport, *minConfidence, *minCorrelation, *minSimilarity, *strongCorr, *confMin;
    QSpinBox *sampleSize, *seed, *histBins, *minValues;
};
} // namespace

struct AnalysisWidget::Impl {
    ViewerCore* core = nullptr;
    detail::AnalysisController* controller = nullptr;

    detail::DiscoveryModel* discoveryModel = nullptr;
    detail::TextListModel *orphansModel = nullptr, *mergesModel = nullptr, *keysModel = nullptr,
                         *pathsModel = nullptr, *profilesModel = nullptr;
    detail::CollectionOverviewModel* overviewModel = nullptr;
    detail::FieldStatsModel* fieldStatsModel = nullptr;
    detail::InfluenceModel* influenceModel = nullptr;
    detail::OutlierModel* outlierModel = nullptr;
    detail::ConfusionModel* confusionModel = nullptr;

    QComboBox* scopeBox = nullptr;
    QToolButton* runBtn = nullptr;
    QToolButton* cancelBtn = nullptr;
    QToolButton* optionsBtn = nullptr;
    QCheckBox* autoBox = nullptr;
    QTimer* debounce = nullptr;   // DE-06: buendelt dataMutated-Ereignisse
    bool rerunPending = false;    // DE-06: Mutation waehrend laufender Analyse
    QProgressBar* progress = nullptr;
    QLabel* healthLabel = nullptr;
    QLabel* statusLabel = nullptr;
    QTableView* discoveryView = nullptr;
    QTableView* overviewView = nullptr;
    QTableView* fieldStatsView = nullptr;
    QTableView* influenceView = nullptr;
    QTableView* outlierView = nullptr;
    QTreeView *orphansView = nullptr, *mergesView = nullptr, *keysView = nullptr, *pathsView = nullptr,
              *profilesView = nullptr;
    HistogramWidget* histogram = nullptr;
    QString scope; // leer = ganze Database
};

AnalysisWidget::AnalysisWidget(QWidget* parent) : QWidget(parent), impl_(std::make_unique<Impl>()) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(6, 6, 6, 6);

    // Werkzeugleiste
    auto* bar = new QHBoxLayout();
    impl_->runBtn = detail::makeToolButton(this, QStyle::SP_MediaPlay, QStringLiteral("Analysieren (F5)"));
    impl_->runBtn->setToolTip(QStringLiteral("Discovery + Analyse der geladenen Daten im Hintergrund ausfuehren"));
    impl_->cancelBtn = detail::makeToolButton(this, QStyle::SP_MediaStop, QStringLiteral("Abbrechen"));
    impl_->cancelBtn->setEnabled(false);
    impl_->optionsBtn = detail::makeToolButton(this, QStyle::SP_FileDialogInfoView, QStringLiteral("Optionen…"));
    impl_->autoBox = new QCheckBox(QStringLiteral("Auto-Analyse nach Änderung"), this);
    impl_->autoBox->setChecked(true);
    impl_->autoBox->setToolTip(QStringLiteral(
        "Nach jeder Datenmanipulation (Design/CRUD/Import, DE-06) wird Discovery + Analyse "
        "automatisch nach kurzer Verzögerung neu ausgeführt"));
    impl_->scopeBox = new QComboBox(this);
    impl_->scopeBox->addItem(QStringLiteral("Ganze Database"), QString());
    bar->addWidget(impl_->runBtn);
    bar->addWidget(impl_->cancelBtn);
    bar->addWidget(impl_->optionsBtn);
    bar->addWidget(impl_->autoBox);
    bar->addWidget(new QLabel(QStringLiteral("Umfang:"), this));
    bar->addWidget(impl_->scopeBox, 1);
    impl_->progress = new QProgressBar(this);
    impl_->progress->setRange(0, 0); // indeterminate
    impl_->progress->setVisible(false);
    impl_->progress->setMaximumWidth(120);
    bar->addWidget(impl_->progress);
    impl_->healthLabel = new QLabel(this);
    QFont hf = impl_->healthLabel->font();
    hf.setBold(true);
    impl_->healthLabel->setFont(hf);
    bar->addWidget(impl_->healthLabel);
    outer->addLayout(bar);

    impl_->statusLabel = new QLabel(QStringLiteral("Bereit. Daten laden und „Analysieren“ klicken."), this);
    impl_->statusLabel->setStyleSheet(QStringLiteral("color: palette(mid);"));
    outer->addWidget(impl_->statusLabel);

    // Models
    impl_->discoveryModel = new detail::DiscoveryModel(this);
    impl_->orphansModel = new detail::TextListModel(this);
    impl_->mergesModel = new detail::TextListModel(this);
    impl_->keysModel = new detail::TextListModel(this);
    impl_->pathsModel = new detail::TextListModel(this);
    impl_->profilesModel = new detail::TextListModel(this);
    impl_->overviewModel = new detail::CollectionOverviewModel(this);
    impl_->fieldStatsModel = new detail::FieldStatsModel(this);
    impl_->influenceModel = new detail::InfluenceModel(this);
    impl_->outlierModel = new detail::OutlierModel(this);

    auto* tabs = new QTabWidget(this);

    // Tab Discovery
    auto* discWidget = new QWidget(tabs);
    auto* discLayout = new QVBoxLayout(discWidget);
    auto* discSplit = new QSplitter(Qt::Horizontal, discWidget);
    impl_->discoveryView = new QTableView(discSplit);
    impl_->discoveryView->setModel(impl_->discoveryModel);
    impl_->discoveryView->setSortingEnabled(true);
    impl_->discoveryView->horizontalHeader()->setStretchLastSection(true);
    auto* discSide = new QTabWidget(discSplit);
    impl_->orphansView = new QTreeView(discSide);
    impl_->orphansView->setModel(impl_->orphansModel);
    impl_->orphansView->setHeaderHidden(true);
    discSide->addTab(impl_->orphansView, QStringLiteral("Orphans"));
    impl_->pathsView = new QTreeView(discSide);
    impl_->pathsView->setModel(impl_->pathsModel);
    impl_->pathsView->setHeaderHidden(true);
    discSide->addTab(impl_->pathsView, QStringLiteral("Pfade"));
    impl_->mergesView = new QTreeView(discSide);
    impl_->mergesView->setModel(impl_->mergesModel);
    impl_->mergesView->setHeaderHidden(true);
    discSide->addTab(impl_->mergesView, QStringLiteral("Merge-Vorschläge"));
    impl_->keysView = new QTreeView(discSide);
    impl_->keysView->setModel(impl_->keysModel);
    impl_->keysView->setHeaderHidden(true);
    discSide->addTab(impl_->keysView, QStringLiteral("Candidate Keys"));
    impl_->profilesView = new QTreeView(discSide);
    impl_->profilesView->setModel(impl_->profilesModel);
    impl_->profilesView->setHeaderHidden(true);
    discSide->addTab(impl_->profilesView, QStringLiteral("Profile"));
    discSplit->addWidget(impl_->discoveryView);
    discSplit->addWidget(discSide);
    discSplit->setStretchFactor(0, 2);
    discSplit->setStretchFactor(1, 1);
    discLayout->addWidget(discSplit);
    // Confusion-Matrix (InMemDB v0.42): entdeckte FKs vs. deklarierte __references
    impl_->confusionModel = new detail::ConfusionModel(this);
    auto* confBox = new QGroupBox(QStringLiteral("Confusion-Matrix (v0.42): entdeckte FKs vs. deklarierte Referenzen"), discWidget);
    auto* confLayout = new QHBoxLayout(confBox);
    auto* confView = new detail::ConfusionView(impl_->confusionModel, confBox);
    confLayout->addWidget(confView);
    auto* confLabel = new QLabel(confBox);
    confLabel->setWordWrap(true);
    confLayout->addWidget(confLabel, 1);
    connect(impl_->confusionModel, &detail::ConfusionModel::changed, this, [confLabel, this] {
        confLabel->setText(impl_->confusionModel->summary());
    });
    discLayout->addWidget(confBox);
    tabs->addTab(discWidget, QStringLiteral("Discovery"));

    // Tab Analyse
    auto* anaWidget = new QWidget(tabs);
    auto* anaLayout = new QVBoxLayout(anaWidget);
    auto* anaTabs = new QTabWidget(anaWidget);
    impl_->overviewView = new QTableView(anaTabs);
    impl_->overviewView->setModel(impl_->overviewModel);
    impl_->overviewView->horizontalHeader()->setStretchLastSection(true);
    anaTabs->addTab(impl_->overviewView, QStringLiteral("Übersicht (FA-29/31)"));
    auto* statsWidget = new QWidget(anaTabs);
    auto* statsLayout = new QVBoxLayout(statsWidget);
    impl_->fieldStatsView = new QTableView(statsWidget);
    impl_->fieldStatsView->setModel(impl_->fieldStatsModel);
    impl_->fieldStatsView->setSelectionBehavior(QAbstractItemView::SelectRows);
    impl_->fieldStatsView->horizontalHeader()->setStretchLastSection(true);
    impl_->histogram = new HistogramWidget(statsWidget);
    statsLayout->addWidget(impl_->fieldStatsView, 1);
    statsLayout->addWidget(new QLabel(QStringLiteral("Histogramm des selektierten Feldes:"), statsWidget));
    statsLayout->addWidget(impl_->histogram);
    anaTabs->addTab(statsWidget, QStringLiteral("Feld-Statistik (FA-30)"));
    impl_->influenceView = new QTableView(anaTabs);
    impl_->influenceView->setModel(impl_->influenceModel);
    impl_->influenceView->horizontalHeader()->setStretchLastSection(true);
    anaTabs->addTab(impl_->influenceView, QStringLiteral("Einfluss (FA-31/32)"));
    impl_->outlierView = new QTableView(anaTabs);
    impl_->outlierView->setModel(impl_->outlierModel);
    impl_->outlierView->horizontalHeader()->setStretchLastSection(true);
    anaTabs->addTab(impl_->outlierView, QStringLiteral("Ausreißer (v0.40)"));
    anaLayout->addWidget(anaTabs);
    tabs->addTab(anaWidget, QStringLiteral("Analyse"));

    outer->addWidget(tabs, 1);

    // Report-Export
    auto* exportBar = new QHBoxLayout();
    auto* mdBtn = detail::makeToolButton(this, QStyle::SP_DialogSaveButton, QStringLiteral("Report: Markdown…"));
    auto* jsBtn = detail::makeToolButton(this, QStyle::SP_DialogSaveButton, QStringLiteral("Report: JSON…"));
    auto* mmBtn = detail::makeToolButton(this, QStyle::SP_DialogSaveButton, QStringLiteral("Report: Mermaid…"));
    exportBar->addWidget(mdBtn);
    exportBar->addWidget(jsBtn);
    exportBar->addWidget(mmBtn);
    exportBar->addStretch();
    outer->addLayout(exportBar);

    // Controller (MVC)
    impl_->controller = new detail::AnalysisController(
        nullptr, impl_->discoveryModel, impl_->orphansModel, impl_->mergesModel, impl_->keysModel,
        impl_->pathsModel, impl_->profilesModel, impl_->overviewModel, impl_->fieldStatsModel,
        impl_->influenceModel, impl_->outlierModel, impl_->confusionModel, this);
    connect(impl_->controller, &detail::AnalysisController::started, this, [this] {
        impl_->runBtn->setEnabled(false);
        impl_->cancelBtn->setEnabled(true);
        impl_->progress->setVisible(true);
        impl_->statusLabel->setText(QStringLiteral("Analyse läuft …"));
        emit analysisStarted();
    });
    connect(impl_->controller, &detail::AnalysisController::progress, this, [this](const QString& s) {
        impl_->statusLabel->setText(s);
    });
    connect(impl_->controller, &detail::AnalysisController::finished, this, [this](bool ok, const QString& summary) {
        impl_->runBtn->setEnabled(true);
        impl_->cancelBtn->setEnabled(false);
        impl_->progress->setVisible(false);
        impl_->statusLabel->setText(summary);
        const double hs = impl_->controller->healthScore();
        impl_->healthLabel->setText(QStringLiteral("Health: %1/100").arg(hs, 0, 'f', 1));
        impl_->healthLabel->setStyleSheet(
            hs >= 80 ? QStringLiteral("color: green;") : hs >= 50 ? QStringLiteral("color: darkorange;")
                                                                 : QStringLiteral("color: red;"));
        emit analysisFinished(ok, summary);
        emit statusMessage(summary);
        // DE-06: waehrend der Analyse angefallene Mutation -> jetzt erneut laufen lassen
        if (impl_->rerunPending) {
            impl_->rerunPending = false;
            if (impl_->autoBox->isChecked())
                runAnalysis();
        }
    });
    // DE-06: Debounce-Timer fuer Auto-Analyse nach dataMutated
    impl_->debounce = new QTimer(this);
    impl_->debounce->setSingleShot(true);
    impl_->debounce->setInterval(400);
    connect(impl_->debounce, &QTimer::timeout, this, [this] {
        if (!impl_->autoBox->isChecked() || !impl_->core)
            return;
        if (impl_->controller->isRunning())
            impl_->rerunPending = true;
        else
            runAnalysis();
    });
    connect(impl_->controller, &detail::AnalysisController::statusMessage, this,
            &AnalysisWidget::statusMessage);

    // Interaktionen
    connect(impl_->runBtn, &QToolButton::clicked, this, &AnalysisWidget::runAnalysis);
    connect(impl_->cancelBtn, &QToolButton::clicked, this, &AnalysisWidget::cancelAnalysis);
    connect(impl_->optionsBtn, &QToolButton::clicked, this, [this] {
        AnalysisOptionsDialog dlg(impl_->controller->discoveryOptions, impl_->controller->numericOptions,
                                  impl_->controller->influenceOptions, this);
        if (dlg.exec() == QDialog::Accepted) {
            impl_->controller->discoveryOptions = dlg.discovery();
            impl_->controller->numericOptions = dlg.numeric();
            impl_->controller->influenceOptions = dlg.influence();
            impl_->controller->crossInfluenceOptions.influence = dlg.influence();
        }
    });
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    connect(impl_->scopeBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        impl_->scope = impl_->scopeBox->currentData().toString();
    });
#else
    connect(impl_->scopeBox, &QComboBox::currentIndexChanged, this, [this](int) {
        impl_->scope = impl_->scopeBox->currentData().toString();
    });
#endif
    connect(impl_->fieldStatsView->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex& current, const QModelIndex&) {
                if (current.isValid())
                    if (const auto* f = impl_->fieldStatsModel->fieldAt(current.row()))
                        impl_->histogram->setHistogram(f->histogram, f->binMin, f->binWidth);
                    else
                        impl_->histogram->clear();
                else
                    impl_->histogram->clear();
            });
    const auto saveReport = [this](const QString& title, const QString& filter, const QString& content) {
        const QString path = QFileDialog::getSaveFileName(this, title, QString(), filter);
        if (path.isEmpty())
            return;
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write(content.toUtf8());
            emit statusMessage(QStringLiteral("Report exportiert: %1").arg(path));
        }
    };
    connect(mdBtn, &QToolButton::clicked, this,
            [this, saveReport] { saveReport(QStringLiteral("Report als Markdown"), QStringLiteral("Markdown (*.md)"), impl_->controller->reportMarkdown()); });
    connect(jsBtn, &QToolButton::clicked, this,
            [this, saveReport] { saveReport(QStringLiteral("Report als JSON"), QStringLiteral("JSON (*.json)"), impl_->controller->reportJson()); });
    connect(mmBtn, &QToolButton::clicked, this,
            [this, saveReport] { saveReport(QStringLiteral("Beziehungsgraph als Mermaid"), QStringLiteral("Mermaid (*.mmd *.md)"), impl_->controller->reportMermaid()); });
}

AnalysisWidget::~AnalysisWidget() = default;

void AnalysisWidget::setCore(ViewerCore* core) {
    if (impl_->core)
        disconnect(impl_->core, &ViewerCore::dataMutated, this, &AnalysisWidget::onDataMutated);
    impl_->core = core;
    if (core) {
        impl_->controller->setCore(core);
        // DE-06: jede Datenmanipulation stoesst (gedebounced) eine Auto-Analyse an
        connect(core, &ViewerCore::dataMutated, this, &AnalysisWidget::onDataMutated);
    }
    // Scope-Combo um die Collections erweitern
    impl_->scopeBox->clear();
    impl_->scopeBox->addItem(QStringLiteral("Ganze Database"), QString());
    if (core)
        for (const QString& n : core->collectionNames())
            impl_->scopeBox->addItem(n, n);
}

ViewerCore* AnalysisWidget::core() const { return impl_->core; }

bool AnalysisWidget::isRunning() const { return impl_->controller->isRunning(); }
double AnalysisWidget::healthScore() const { return impl_->controller->healthScore(); }
int AnalysisWidget::discoveryRowCount() const { return impl_->discoveryModel->rowCount(); }
int AnalysisWidget::overviewRowCount() const { return impl_->overviewModel->rowCount(); }
int AnalysisWidget::fieldStatsRowCount() const { return impl_->fieldStatsModel->rowCount(); }
int AnalysisWidget::influenceRowCount() const { return impl_->influenceModel->rowCount(); }
int AnalysisWidget::outlierRowCount() const { return impl_->outlierModel->rowCount(); }
bool AnalysisWidget::hasConfusion() const { return impl_->confusionModel->hasConfusion(); }
QString AnalysisWidget::confusionSummary() const { return impl_->confusionModel->summary(); }

void AnalysisWidget::runAnalysis() {
    if (impl_->core)
        impl_->controller->setCore(impl_->core);
    impl_->controller->run(impl_->scope);
}

void AnalysisWidget::cancelAnalysis() { impl_->controller->cancel(); }

void AnalysisWidget::setAutoAnalysisEnabled(bool on) {
    impl_->autoBox->setChecked(on);
    if (!on) {
        impl_->debounce->stop();
        impl_->rerunPending = false;
    }
}

bool AnalysisWidget::autoAnalysisEnabled() const { return impl_->autoBox->isChecked(); }

void AnalysisWidget::onDataMutated() {
    if (!impl_->autoBox->isChecked())
        return;
    impl_->debounce->start(); // jede Mutation startet den Timer neu (Debounce ~400 ms)
}

void AnalysisWidget::setScopeCollection(const QString& name) {
    impl_->scope = name;
    const int idx = impl_->scopeBox->findData(name);
    if (idx >= 0)
        impl_->scopeBox->setCurrentIndex(idx);
    else if (!name.isEmpty()) {
        impl_->scopeBox->addItem(name, name);
        impl_->scopeBox->setCurrentIndex(impl_->scopeBox->count() - 1);
    }
}

QString AnalysisWidget::scopeCollection() const { return impl_->scope; }

} // namespace memdb::viewer
