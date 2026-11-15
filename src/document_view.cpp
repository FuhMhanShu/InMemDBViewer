#include "memdbviewer/document_view.hpp"

#include <memdb/exceptions.hpp>
#include <memdb/metrics.hpp>

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

namespace memdb::viewer {

DocumentView::DocumentView(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);

    auto* filterRow = new QHBoxLayout();
    queryEdit_ = new QLineEdit(this);
    queryEdit_->setPlaceholderText(QStringLiteral("DSL-Query, z. B. age >= 18"));
    auto* applyBtn = new QPushButton(QStringLiteral("Filter"), this);
    filterRow->addWidget(queryEdit_);
    filterRow->addWidget(applyBtn);
    layout->addLayout(filterRow);

    table_ = new QTableView(this);
    table_->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(table_);

    status_ = new QLabel(QStringLiteral("keine Collection gewaehlt"), this);
    layout->addWidget(status_);

    connect(applyBtn, &QPushButton::clicked, this, &DocumentView::applyQuery);
    connect(queryEdit_, &QLineEdit::returnPressed, this, &DocumentView::applyQuery);
}

void DocumentView::showCollection(memdb::Collection collection) {
    coll_ = collection;
    if (model_) {
        table_->setModel(nullptr);
        delete model_;
        model_ = nullptr;
    }
    model_ = new memdb::qt::MemDbTableModel(collection, this);
    table_->setModel(model_);
    queryEdit_->clear();
    updateStatus();
}

void DocumentView::applyQuery() {
    if (!model_)
        return;
    try {
        model_->setQuery(queryEdit_->text());
    } catch (const memdb::Error& e) {
        status_->setText(QStringLiteral("Query-Fehler: %1").arg(QString::fromStdString(e.what())));
        return;
    }
    updateStatus();
}

void DocumentView::updateStatus() {
    if (!model_ || !coll_)
        return;
    const memdb::CollectionMetrics m = coll_->metrics();
    status_->setText(QStringLiteral("%1 Dokument(e) | letzte Query: Index=%2, Kandidaten=%3")
                         .arg(model_->rowCount())
                         .arg(m.lastQueryUsedIndex ? QStringLiteral("ja") : QStringLiteral("nein"))
                         .arg(static_cast<qulonglong>(m.lastCandidateCount)));
}

} // namespace memdb::viewer
