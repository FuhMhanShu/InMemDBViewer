#include "memdbviewer/database_design_widget.hpp"
#include "memdbviewer/viewer_core.hpp"

#include "design_controller.hpp"

#include <memdb/memdb.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QStringListModel>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace memdb::viewer {

namespace {
const char* kTypeNames[] = {"null", "bool", "int", "double", "string", "binary", "document", "array"};
}

struct DatabaseDesignWidget::Impl {
    ViewerCore* core = nullptr;
    detail::DesignController* controller = nullptr;

    QListWidget* collList = nullptr;
    QLabel* schemaStatus = nullptr;
    QTableWidget* schemaTable = nullptr;
    QListWidget* indexList = nullptr;
    QComboBox* indexField = nullptr;
    QCheckBox* indexUnique = nullptr;
    QTableWidget* refTable = nullptr;
    QComboBox *refFromColl = nullptr, *refToColl = nullptr;
    QSpinBox *refFromId = nullptr, *refToId = nullptr;
    QCheckBox *refHasFromId = nullptr, *refHasToId = nullptr;
    QLineEdit* refRelation = nullptr;
    QString current;
};

DatabaseDesignWidget::DatabaseDesignWidget(QWidget* parent) : QWidget(parent), impl_(std::make_unique<Impl>()) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(6, 6, 6, 6);
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // ---- linke Seite: Collections ----
    auto* collBox = new QGroupBox(QStringLiteral("Collections"), splitter);
    auto* collLayout = new QVBoxLayout(collBox);
    impl_->collList = new QListWidget(collBox);
    collLayout->addWidget(impl_->collList, 1);
    auto* collBtns = new QHBoxLayout();
    auto* addCollBtn = new QPushButton(QStringLiteral("Anlegen…"), collBox);
    auto* delCollBtn = new QPushButton(QStringLiteral("Löschen"), collBox);
    auto* clearCollBtn = new QPushButton(QStringLiteral("Leeren"), collBox);
    collBtns->addWidget(addCollBtn);
    collBtns->addWidget(delCollBtn);
    collBtns->addWidget(clearCollBtn);
    collLayout->addLayout(collBtns);
    splitter->addWidget(collBox);

    // ---- rechte Seite: Tabs Schema / Indizes / Referenzen ----
    auto* tabs = new QTabWidget(splitter);

    // Tab Schema
    auto* schemaWidget = new QWidget(tabs);
    auto* schemaLayout = new QVBoxLayout(schemaWidget);
    impl_->schemaStatus = new QLabel(schemaWidget);
    schemaLayout->addWidget(impl_->schemaStatus);
    impl_->schemaTable = new QTableWidget(0, 5, schemaWidget);
    impl_->schemaTable->setHorizontalHeaderLabels(
        {QStringLiteral("Feldpfad"), QStringLiteral("Typ"), QStringLiteral("nullable"),
         QStringLiteral("Unter-Schema (pfad:typ, …)"), QStringLiteral("Array-Elementtyp")});
    impl_->schemaTable->horizontalHeader()->setStretchLastSection(true);
    schemaLayout->addWidget(impl_->schemaTable, 1);
    auto* schemaBtns = new QHBoxLayout();
    auto* addFieldBtn = new QPushButton(QStringLiteral("Feld +"), schemaWidget);
    auto* delFieldBtn = new QPushButton(QStringLiteral("Feld −"), schemaWidget);
    auto* applySchemaBtn = new QPushButton(QStringLiteral("Schema anwenden (validiert)"), schemaWidget);
    auto* removeSchemaBtn = new QPushButton(QStringLiteral("Schema entfernen"), schemaWidget);
    applySchemaBtn->setToolTip(QStringLiteral("Setzt das strikte Schema (FA-20); InMemDB prueft alle vorhandenen Dokumente – Fehler landen in der Ausgabe"));
    schemaBtns->addWidget(addFieldBtn);
    schemaBtns->addWidget(delFieldBtn);
    schemaBtns->addStretch();
    schemaBtns->addWidget(applySchemaBtn);
    schemaBtns->addWidget(removeSchemaBtn);
    schemaLayout->addLayout(schemaBtns);
    tabs->addTab(schemaWidget, QStringLiteral("Schema"));

    // Tab Indizes
    auto* indexWidget = new QWidget(tabs);
    auto* indexLayout = new QVBoxLayout(indexWidget);
    indexLayout->addWidget(new QLabel(QStringLiteral("Vorhandene Indizes (FA-10):"), indexWidget));
    impl_->indexList = new QListWidget(indexWidget);
    indexLayout->addWidget(impl_->indexList, 1);
    auto* indexForm = new QHBoxLayout();
    impl_->indexField = new QComboBox(indexWidget);
    impl_->indexField->setEditable(true);
    impl_->indexField->setInsertPolicy(QComboBox::NoInsert);
    indexForm->addWidget(new QLabel(QStringLiteral("Feldpfad:"), indexWidget));
    indexForm->addWidget(impl_->indexField, 1);
    impl_->indexUnique = new QCheckBox(QStringLiteral("unique"), indexWidget);
    auto* addIndexBtn = new QPushButton(QStringLiteral("Index anlegen"), indexWidget);
    auto* delIndexBtn = new QPushButton(QStringLiteral("Index löschen"), indexWidget);
    indexForm->addWidget(impl_->indexUnique);
    indexForm->addWidget(addIndexBtn);
    indexForm->addWidget(delIndexBtn);
    indexLayout->addLayout(indexForm);
    tabs->addTab(indexWidget, QStringLiteral("Indizes"));

    // Tab Referenzen
    auto* refWidget = new QWidget(tabs);
    auto* refLayout = new QVBoxLayout(refWidget);
    refLayout->addWidget(new QLabel(QStringLiteral("Referenzen (FA-18):"), refWidget));
    impl_->refTable = new QTableWidget(0, 6, refWidget);
    impl_->refTable->setHorizontalHeaderLabels(
        {QStringLiteral("Von"), QStringLiteral("Von-ID"), QStringLiteral("Nach"), QStringLiteral("Nach-ID"),
         QStringLiteral("Relation"), QStringLiteral("Status")});
    impl_->refTable->horizontalHeader()->setStretchLastSection(true);
    impl_->refTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    refLayout->addWidget(impl_->refTable, 1);
    auto* refForm = new QFormLayout();
    impl_->refFromColl = new QComboBox(refWidget);
    impl_->refToColl = new QComboBox(refWidget);
    impl_->refHasFromId = new QCheckBox(QStringLiteral("Dokument-ID"), refWidget);
    impl_->refFromId = new QSpinBox(refWidget);
    impl_->refFromId->setRange(0, 999999999);
    impl_->refHasToId = new QCheckBox(QStringLiteral("Dokument-ID"), refWidget);
    impl_->refToId = new QSpinBox(refWidget);
    impl_->refToId->setRange(0, 999999999);
    impl_->refRelation = new QLineEdit(refWidget);
    impl_->refRelation->setPlaceholderText(QStringLiteral("z. B. kunde, bestellt"));
    auto* fromRow = new QHBoxLayout();
    fromRow->addWidget(impl_->refFromColl, 1);
    fromRow->addWidget(impl_->refHasFromId);
    fromRow->addWidget(impl_->refFromId);
    auto* fromW = new QWidget(refWidget);
    fromW->setLayout(fromRow);
    auto* toRow = new QHBoxLayout();
    toRow->addWidget(impl_->refToColl, 1);
    toRow->addWidget(impl_->refHasToId);
    toRow->addWidget(impl_->refToId);
    auto* toW = new QWidget(refWidget);
    toW->setLayout(toRow);
    refForm->addRow(QStringLiteral("Von (Collection):"), fromW);
    refForm->addRow(QStringLiteral("Nach (Collection):"), toW);
    refForm->addRow(QStringLiteral("Relation:"), impl_->refRelation);
    refLayout->addLayout(refForm);
    auto* refBtns = new QHBoxLayout();
    auto* addRefBtn = new QPushButton(QStringLiteral("Verknüpfen"), refWidget);
    auto* delRefBtn = new QPushButton(QStringLiteral("Ausgewählte entfernen"), refWidget);
    auto* pruneBtn = new QPushButton(QStringLiteral("Dangling bereinigen"), refWidget);
    refBtns->addWidget(addRefBtn);
    refBtns->addWidget(delRefBtn);
    refBtns->addWidget(pruneBtn);
    refLayout->addLayout(refBtns);
    tabs->addTab(refWidget, QStringLiteral("Referenzen"));

    splitter->addWidget(tabs);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    outer->addWidget(splitter);

    // ---- Controller (MVC) ----
    impl_->controller = new detail::DesignController(nullptr, this);
    connect(impl_->controller, &detail::DesignController::changed, this, &DatabaseDesignWidget::refresh);

    // ---- Interaktionen ----
    connect(impl_->collList, &QListWidget::currentTextChanged, this, [this](const QString& name) {
        impl_->current = name;
        loadCollectionDetail(name);
    });
    connect(addCollBtn, &QPushButton::clicked, this, [this] {
        bool ok = false;
        const QString name = QInputDialog::getText(this, QStringLiteral("Collection anlegen"),
                                                   QStringLiteral("Name:"), QLineEdit::Normal, QString(), &ok);
        if (ok && !name.isEmpty()) {
            impl_->controller->createCollection(name);
            refresh();
        }
    });
    connect(delCollBtn, &QPushButton::clicked, this, [this] {
        if (!impl_->current.isEmpty())
            impl_->controller->dropCollection(impl_->current);
        refresh();
    });
    connect(clearCollBtn, &QPushButton::clicked, this, [this] {
        if (!impl_->current.isEmpty())
            impl_->controller->clearCollection(impl_->current);
        refresh();
    });
    connect(addFieldBtn, &QPushButton::clicked, this, [this] { addSchemaRow(detail::FieldDef{}); });
    connect(delFieldBtn, &QPushButton::clicked, this, [this] {
        const int row = impl_->schemaTable->currentRow();
        if (row >= 0)
            impl_->schemaTable->removeRow(row);
    });
    connect(applySchemaBtn, &QPushButton::clicked, this, [this] {
        if (impl_->current.isEmpty())
            return;
        std::vector<detail::FieldDef> fields;
        for (int r = 0; r < impl_->schemaTable->rowCount(); ++r) {
            detail::FieldDef d;
            d.path = impl_->schemaTable->item(r, 0) ? impl_->schemaTable->item(r, 0)->text().trimmed() : QString();
            auto* combo = qobject_cast<QComboBox*>(impl_->schemaTable->cellWidget(r, 1));
            d.typeName = combo ? combo->currentText() : QStringLiteral("string");
            auto* nul = impl_->schemaTable->item(r, 2);
            d.nullable = nul ? (nul->checkState() == Qt::Checked) : true;
            d.subSchema = impl_->schemaTable->item(r, 3) ? impl_->schemaTable->item(r, 3)->text() : QString();
            d.arrayElem = impl_->schemaTable->item(r, 4) ? impl_->schemaTable->item(r, 4)->text() : QString();
            fields.push_back(std::move(d));
        }
        impl_->controller->applySchema(impl_->current, fields); // verifiziert -> Ausgabe
        refresh();
        emit statusMessage(QStringLiteral("Schema für „%1“ angewendet (ggf. Fehler in der Ausgabe).").arg(impl_->current));
    });
    connect(removeSchemaBtn, &QPushButton::clicked, this, [this] {
        if (!impl_->current.isEmpty())
            impl_->controller->removeSchema(impl_->current);
        refresh();
    });
    connect(addIndexBtn, &QPushButton::clicked, this, [this] {
        if (!impl_->current.isEmpty())
            impl_->controller->addIndex(impl_->current, impl_->indexField->currentText().trimmed(),
                                        impl_->indexUnique->isChecked());
        refresh();
    });
    connect(delIndexBtn, &QPushButton::clicked, this, [this] {
        auto* item = impl_->indexList->currentItem();
        if (item && !impl_->current.isEmpty()) {
            const QString field = item->data(Qt::UserRole).toString();
            impl_->controller->removeIndex(impl_->current, field);
        }
        refresh();
    });
    connect(addRefBtn, &QPushButton::clicked, this, [this] {
        impl_->controller->addReference(impl_->refFromColl->currentText(),
                                        static_cast<quint64>(impl_->refFromId->value()), impl_->refHasFromId->isChecked(),
                                        impl_->refToColl->currentText(),
                                        static_cast<quint64>(impl_->refToId->value()), impl_->refHasToId->isChecked(),
                                        impl_->refRelation->text().trimmed());
        refresh();
    });
    connect(delRefBtn, &QPushButton::clicked, this, [this] {
        const int row = impl_->refTable->currentRow();
        if (row < 0)
            return;
        const QString from = impl_->refTable->item(row, 0)->text();
        const QString to = impl_->refTable->item(row, 2)->text();
        const QString rel = impl_->refTable->item(row, 4)->text();
        const bool hasFrom = impl_->refTable->item(row, 1)->text() != QLatin1String("–");
        const bool hasTo = impl_->refTable->item(row, 3)->text() != QLatin1String("–");
        const quint64 fid = hasFrom ? impl_->refTable->item(row, 1)->text().toULongLong() : 0;
        const quint64 tid = hasTo ? impl_->refTable->item(row, 3)->text().toULongLong() : 0;
        impl_->controller->removeReference(from, fid, hasFrom, to, tid, hasTo, rel);
        refresh();
    });
    connect(pruneBtn, &QPushButton::clicked, this, [this] {
        impl_->controller->pruneDangling();
        refresh();
    });
}

DatabaseDesignWidget::~DatabaseDesignWidget() = default;

void DatabaseDesignWidget::setCore(ViewerCore* core) {
    impl_->core = core;
    if (core)
        impl_->controller->setCore(core);
    refresh();
}

ViewerCore* DatabaseDesignWidget::core() const { return impl_->core; }
QString DatabaseDesignWidget::currentCollection() const { return impl_->current; }
int DatabaseDesignWidget::collectionItemCount() const { return impl_->collList->count(); }
int DatabaseDesignWidget::schemaRowCount() const { return impl_->schemaTable->rowCount(); }
int DatabaseDesignWidget::indexItemCount() const { return impl_->indexList->count(); }
int DatabaseDesignWidget::referenceRowCount() const { return impl_->refTable->rowCount(); }

void DatabaseDesignWidget::selectCollection(const QString& name) {
    const int row = impl_->collList->findItems(name, Qt::MatchExactly).value(0, nullptr)
                        ? impl_->collList->row(impl_->collList->findItems(name, Qt::MatchExactly).first())
                        : -1;
    if (row >= 0)
        impl_->collList->setCurrentRow(row);
    else {
        impl_->current = name;
        loadCollectionDetail(name);
    }
}

void DatabaseDesignWidget::refresh() {
    if (!impl_->core)
        return;
    const QStringList colls = impl_->controller->collections();
    impl_->collList->clear();
    impl_->collList->addItems(colls);
    impl_->refFromColl->clear();
    impl_->refToColl->clear();
    impl_->refFromColl->addItems(colls);
    impl_->refToColl->addItems(colls);
    if (!impl_->current.isEmpty() && colls.contains(impl_->current))
        impl_->collList->setCurrentRow(colls.indexOf(impl_->current));
    else if (!colls.isEmpty()) {
        impl_->current = colls.first();
        impl_->collList->setCurrentRow(0);
    } else {
        impl_->current.clear();
    }
    loadCollectionDetail(impl_->current);
}

void DatabaseDesignWidget::addSchemaRow(const detail::FieldDef& d) {
    const int row = impl_->schemaTable->rowCount();
    impl_->schemaTable->insertRow(row);
    auto* pathItem = new QTableWidgetItem(d.path);
    impl_->schemaTable->setItem(row, 0, pathItem);
    auto* typeCombo = new QComboBox(impl_->schemaTable);
    for (const char* t : kTypeNames)
        typeCombo->addItem(QString::fromLatin1(t));
    const int ti = typeCombo->findText(d.typeName.isEmpty() ? QStringLiteral("string") : d.typeName);
    typeCombo->setCurrentIndex(ti >= 0 ? ti : typeCombo->findText(QStringLiteral("string")));
    impl_->schemaTable->setCellWidget(row, 1, typeCombo);
    auto* nulItem = new QTableWidgetItem;
    nulItem->setFlags(nulItem->flags() | Qt::ItemIsUserCheckable);
    nulItem->setCheckState(d.nullable ? Qt::Checked : Qt::Unchecked);
    impl_->schemaTable->setItem(row, 2, nulItem);
    impl_->schemaTable->setItem(row, 3, new QTableWidgetItem(d.subSchema));
    impl_->schemaTable->setItem(row, 4, new QTableWidgetItem(d.arrayElem));
}

void DatabaseDesignWidget::loadCollectionDetail(const QString& name) {
    if (!impl_->core || name.isEmpty())
        return;
    // Schema-Tabelle fuellen
    const auto schema = impl_->controller->currentSchema(name);
    const bool hasSchema = impl_->core->database().hasCollection(name.toStdString()) &&
                           impl_->core->database().collection(name.toStdString()).hasSchema();
    impl_->schemaStatus->setText(hasSchema ? QStringLiteral("Schema: ja (%1 Felder) – Bearbeitung und „Anwenden“ validiert gegen alle Dokumente")
                                            : QStringLiteral("Schema: nein (schema-los)"));
    impl_->schemaTable->setRowCount(0);
    for (const auto& f : schema)
        addSchemaRow(f);
    // Index-Completer
    impl_->indexField->clear();
    const QStringList fields = impl_->controller->fieldSuggestions(name);
    impl_->indexField->addItems(fields);
    if (impl_->indexField->completer() == nullptr)
        impl_->indexField->setCompleter(new QCompleter(fields, impl_->indexField));
    else
        impl_->indexField->completer()->setModel(new QStringListModel(fields, impl_->indexField->completer()));
    // Index-Liste
    impl_->indexList->clear();
    if (impl_->core->database().hasCollection(name.toStdString())) {
        for (const auto& idx : impl_->core->database().collection(name.toStdString()).indexes()) {
            auto* item = new QListWidgetItem(
                QStringLiteral("%1 (%2)").arg(QString::fromStdString(idx.field()), idx.isUnique() ? QStringLiteral("unique") : QStringLiteral("index")));
            item->setData(Qt::UserRole, QString::fromStdString(idx.field()));
            impl_->indexList->addItem(item);
        }
    }
    // Referenzen-Tabelle: alle Referenzen, die diese Collection betreffen (Scan ueber __references)
    impl_->refTable->setRowCount(0);
    if (impl_->core->references() && impl_->core->database().hasCollection("__references")) {
        const std::string nameStd = name.toStdString();
        std::vector<memdb::Reference> all;
        impl_->core->database().collection("__references").forEach(memdb::Expression{}, [&](const memdb::Document& d) {
            const memdb::Value* fc = d.find("from_coll");
            const memdb::Value* tc = d.find("to_coll");
            if (!fc || !tc || !fc->isString() || !tc->isString())
                return;
            if (fc->asString() != nameStd && tc->asString() != nameStd)
                return;
            memdb::Reference r;
            r.fromCollection = fc->asString();
            r.toCollection = tc->asString();
            r.relation = d.find("relation") && d.find("relation")->isString() ? d.find("relation")->asString() : std::string();
            if (const memdb::Value* v = d.find("from_id"); v && v->isInt())
                r.fromId = memdb::DocumentId(static_cast<std::uint64_t>(v->asInt()));
            if (const memdb::Value* v = d.find("to_id"); v && v->isInt())
                r.toId = memdb::DocumentId(static_cast<std::uint64_t>(v->asInt()));
            all.push_back(std::move(r));
        });
        const auto dang = impl_->core->references()->dangling();
        int row = 0;
        for (const auto& r : all) {
            const bool isDangling = std::any_of(dang.begin(), dang.end(), [&](const memdb::Reference& d) {
                return d == r;
            });
            impl_->refTable->insertRow(row);
            impl_->refTable->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(r.fromCollection)));
            impl_->refTable->setItem(row, 1, new QTableWidgetItem(
                r.fromId ? QString::number(r.fromId->value()) : QStringLiteral("–")));
            impl_->refTable->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(r.toCollection)));
            impl_->refTable->setItem(row, 3, new QTableWidgetItem(
                r.toId ? QString::number(r.toId->value()) : QStringLiteral("–")));
            impl_->refTable->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(r.relation)));
            auto* status = new QTableWidgetItem(isDangling ? QStringLiteral("dangling") : QStringLiteral("ok"));
            if (isDangling)
                status->setForeground(QColor(200, 40, 40));
            impl_->refTable->setItem(row, 5, status);
            ++row;
        }
    }
}

} // namespace memdb::viewer
