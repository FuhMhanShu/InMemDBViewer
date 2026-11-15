#include "memdbviewer/metrics_panel.hpp"

#include <memdb/metrics.hpp>

#include <QLabel>
#include <QVBoxLayout>

namespace memdb::viewer {

MetricsPanel::MetricsPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    title_ = new QLabel(QStringLiteral("Metriken"), this);
    QFont bold = title_->font();
    bold.setBold(true);
    title_->setFont(bold);
    body_ = new QLabel(QStringLiteral("keine Auswahl"), this);
    QFont mono(QStringLiteral("Monospace"));
    mono.setStyleHint(QFont::TypeWriter);
    body_->setFont(mono);
    body_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    body_->setWordWrap(true);
    layout->addWidget(title_);
    layout->addWidget(body_);
}

void MetricsPanel::showCollection(memdb::Collection collection) {
    coll_ = collection;
    db_ = nullptr;
    databaseMode_ = false;
    refresh();
}

void MetricsPanel::showDatabase(memdb::Database& db) {
    coll_.reset();
    db_ = &db;
    databaseMode_ = true;
    refresh();
}

QString MetricsPanel::bodyText() const {
    return title_->text() + QLatin1Char('\n') + body_->text();
}

void MetricsPanel::refresh() {
    render();
}

void MetricsPanel::render() {
    if (databaseMode_ && db_) {
        const memdb::DatabaseMetrics m = db_->metrics();
        title_->setText(QStringLiteral("Database \"%1\"").arg(QString::fromStdString(db_->name())));
        body_->setText(QStringLiteral("Collections: %1\nDokumente gesamt: %2\n"
                                      "inserts: %3 | updates: %4 | removes: %5\n"
                                      "queries: %6 (davon Index: %7) | scannedDocs: %8\n"
                                      "tx commits/rollbacks: %9/%10")
                           .arg(m.collectionCount)
                           .arg(m.totalDocs)
                           .arg(m.sum.inserts)
                           .arg(m.sum.updates)
                           .arg(m.sum.removes)
                           .arg(m.sum.queries)
                           .arg(m.sum.indexQueries)
                           .arg(m.sum.scannedDocs)
                           .arg(m.sum.txCommits)
                           .arg(m.sum.txRollbacks));
        return;
    }
    if (!coll_) {
        title_->setText(QStringLiteral("Metriken"));
        body_->setText(QStringLiteral("keine Auswahl"));
        return;
    }
    const memdb::CollectionMetrics m = coll_->metrics();
    const memdb::OverflowInfo ov = coll_->overflowInfo();
    title_->setText(QStringLiteral("Collection \"%1\"").arg(QString::fromStdString(coll_->name())));
    QString text = QStringLiteral("docs: %1 | indexCount: %2\n"
                                  "inserts: %3 | updates: %4 | removes: %5\n"
                                  "queries: %6 (Index: %7) | scannedDocs: %8\n"
                                  "letzte Query: Index=%9, Kandidaten=%10, %11 ms (gesamt %12 ms)\n"
                                  "changeEvents: %13 | tx commits/rollbacks: %14/%15\n"
                                  "spill reads/writes: %16/%17")
                       .arg(m.docs)
                       .arg(m.indexCount)
                       .arg(m.inserts)
                       .arg(m.updates)
                       .arg(m.removes)
                       .arg(m.queries)
                       .arg(m.indexQueries)
                       .arg(m.scannedDocs)
                       .arg(m.lastQueryUsedIndex ? QStringLiteral("ja") : QStringLiteral("nein"))
                       .arg(m.lastCandidateCount)
                       .arg(m.lastQueryMs)
                       .arg(m.totalQueryMs)
                       .arg(m.changeEvents)
                       .arg(m.txCommits)
                       .arg(m.txRollbacks)
                       .arg(m.spillReads)
                       .arg(m.spillWrites);
    if (ov.enabled)
        text += QStringLiteral("\nOVERFLOW aktiv: inMemory=%1, spilled=%2, spillBytes=%3 (%4)")
                    .arg(ov.inMemory)
                    .arg(ov.spilled)
                    .arg(ov.spillBytes)
                    .arg(QString::fromStdString(ov.spillFile));
    body_->setText(text);
}

} // namespace memdb::viewer
