#include "memdbviewer/document_tree_view.hpp"

#include <memdb/schema.hpp>
#include <memdb/value.hpp>

#include <QHeaderView>
#include <QStandardItemModel>

namespace memdb::viewer {

DocumentTreeView::DocumentTreeView(QWidget* parent) : QTreeView(parent) {
    model_ = new QStandardItemModel(this);
    model_->setHorizontalHeaderLabels(
        {QStringLiteral("Feld"), QStringLiteral("Typ"), QStringLiteral("Wert")});
    setModel(model_);
    setAlternatingRowColors(true);
    header()->setStretchLastSection(true);
}

void DocumentTreeView::clearDocument() {
    model_->removeRows(0, model_->rowCount());
}

void DocumentTreeView::showDocument(const memdb::Document& doc) {
    clearDocument();
    auto* root = model_->invisibleRootItem();
    auto* docItem = new QStandardItem(
        QStringLiteral("Dokument #%1 (%2 Felder)").arg(doc.id().value()).arg(doc.fieldCount()));
    auto* typeItem = new QStandardItem(QStringLiteral("document"));
    auto* valItem = new QStandardItem(QStringLiteral("{...}"));
    for (auto* item : {docItem, typeItem, valItem})
        item->setEditable(false);
    root->appendRow({docItem, typeItem, valItem});
    doc.forEachField([&](const std::string& key, const memdb::Value& v) {
        appendValue(docItem, QString::fromStdString(key), v);
    });
    expandAll();
}

void DocumentTreeView::appendValue(QStandardItem* parent, const QString& key,
                                   const memdb::Value& value) {
    const QString type = QString::fromStdString(memdb::typeName(value.type()));
    auto* keyItem = new QStandardItem(key);
    auto* typeItem = new QStandardItem(type);
    auto* valItem = new QStandardItem;
    keyItem->setEditable(false);
    typeItem->setEditable(false);
    valItem->setEditable(false);
    if (value.isDocument()) {
        auto sub = value.asDocument();
        valItem->setText(QStringLiteral("{ %1 Feld(er) }").arg(sub->fieldCount()));
        sub->forEachField([&](const std::string& k, const memdb::Value& v) {
            appendValue(keyItem, QString::fromStdString(k), v);
        });
    } else if (value.isArray()) {
        const auto& arr = value.asArray();
        valItem->setText(QStringLiteral("[ %1 Element(e) ]").arg(arr.size()));
        for (std::size_t i = 0; i < arr.size(); ++i)
            appendValue(keyItem, QStringLiteral("[%1]").arg(i), arr[i]);
    } else {
        valItem->setText(QString::fromStdString(memdb::toString(value)));
    }
    parent->appendRow({keyItem, typeItem, valItem});
}

} // namespace memdb::viewer
