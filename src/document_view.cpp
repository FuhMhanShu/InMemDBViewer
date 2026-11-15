#include "memdbviewer/document_view.hpp"

#include <memdb/document_id.hpp>
#include <memdb/exceptions.hpp>
#include <memdb/metrics.hpp>
#include <memdb/query_dsl.hpp>
#include <memdb/value.hpp>

#include <QComboBox>
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

    // Zeile 1: DSL-Query-Filter (FA-10)
    auto* filterRow = new QHBoxLayout();
    queryEdit_ = new QLineEdit(this);
    queryEdit_->setPlaceholderText(QStringLiteral("DSL-Query, z. B. age >= 18"));
    auto* applyBtn = new QPushButton(QStringLiteral("Filter"), this);
    filterRow->addWidget(queryEdit_);
    filterRow->addWidget(applyBtn);
    layout->addLayout(filterRow);

    // Zeile 2: CRUD (FA-14) + Aggregat (FA-17)
    auto* editRow = new QHBoxLayout();
    insertBtn_ = new QPushButton(QStringLiteral("Neu (leer)"), this);
    deleteBtn_ = new QPushButton(QStringLiteral("Löschen"), this);
    aggOpCombo_ = new QComboBox(this);
    aggOpCombo_->addItems({QStringLiteral("count"), QStringLiteral("min"), QStringLiteral("max"),
                           QStringLiteral("sum"), QStringLiteral("avg")});
    aggFieldEdit_ = new QLineEdit(this);
    aggFieldEdit_->setPlaceholderText(QStringLiteral("Feld, z. B. age"));
    auto* aggBtn = new QPushButton(QStringLiteral("Agg"), this);
    editRow->addWidget(insertBtn_);
    editRow->addWidget(deleteBtn_);
    editRow->addWidget(aggOpCombo_);
    editRow->addWidget(aggFieldEdit_);
    editRow->addWidget(aggBtn);
    layout->addLayout(editRow);

    table_ = new QTableView(this);
    table_->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(table_);

    status_ = new QLabel(QStringLiteral("keine Collection gewählt"), this);
    layout->addWidget(status_);

    connect(applyBtn, &QPushButton::clicked, this, &DocumentView::applyQuery);
    connect(queryEdit_, &QLineEdit::returnPressed, this, &DocumentView::applyQuery);
    connect(insertBtn_, &QPushButton::clicked, this, [this] { insertEmptyDocument(); });
    connect(deleteBtn_, &QPushButton::clicked, this, [this] { deleteSelectedDocument(); });
    connect(aggBtn, &QPushButton::clicked, this, &DocumentView::onAggregate);
    connect(table_, &QTableView::clicked, this, &DocumentView::onCellClicked);
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

bool DocumentView::insertEmptyDocument() {
    if (!coll_)
        return false;
    coll_->insert(memdb::Document{}); // Observer -> Auto-Refresh des Models
    updateStatus();
    return true;
}

bool DocumentView::deleteSelectedDocument() {
    if (!coll_ || !model_)
        return false;
    const QModelIndex current = table_->currentIndex();
    if (!current.isValid())
        return false;
    const auto ids = currentIds();
    if (current.row() < 0 || current.row() >= static_cast<int>(ids.size()))
        return false;
    const bool ok = coll_->removeById(ids[static_cast<std::size_t>(current.row())]);
    updateStatus();
    return ok;
}

void DocumentView::selectRow(int row) {
    if (model_ && row >= 0 && row < model_->rowCount())
        table_->setCurrentIndex(model_->index(row, 0));
}

QString DocumentView::applyAggregate(memdb::AggOp op, const QString& fieldPath) {
    if (!coll_)
        return {};
    const auto result = coll_->aggregate(op, fieldPath.toStdString(), currentQuery());
    const QString text =
        result ? QString::fromStdString(memdb::toString(*result)) : QStringLiteral("(leere Menge)");
    status_->setText(QStringLiteral("Agg: %1").arg(text));
    return text;
}

QString DocumentView::statusText() const {
    return status_->text();
}

void DocumentView::onAggregate() {
    static const memdb::AggOp ops[] = {memdb::AggOp::Count, memdb::AggOp::Min, memdb::AggOp::Max,
                                        memdb::AggOp::Sum, memdb::AggOp::Avg};
    const int i = aggOpCombo_->currentIndex();
    if (i < 0 || i >= static_cast<int>(sizeof(ops) / sizeof(ops[0])))
        return;
    applyAggregate(ops[i], aggFieldEdit_->text());
}

void DocumentView::onCellClicked(const QModelIndex& index) {
    if (!index.isValid() || !coll_)
        return;
    const auto ids = currentIds();
    if (index.row() >= 0 && index.row() < static_cast<int>(ids.size()))
        emit documentSelected(ids[static_cast<std::size_t>(index.row())].value());
}

memdb::Expression DocumentView::currentQuery() const {
    const QString q = queryEdit_->text().trimmed();
    if (q.isEmpty() || !coll_)
        return memdb::Expression{};
    try {
        return memdb::parseQuery(q.toStdString());
    } catch (const memdb::Error&) {
        return memdb::Expression{}; // ungueltige Query wurde bereits in applyQuery gemeldet
    }
}

std::vector<memdb::DocumentId> DocumentView::currentIds() const {
    if (!coll_)
        return {};
    return coll_->findIds(currentQuery());
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
