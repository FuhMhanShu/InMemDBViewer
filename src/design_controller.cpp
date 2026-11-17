#include "design_controller.hpp"

#include "memdbviewer/viewer_core.hpp"

#include <memdb/memdb.hpp>

namespace memdb::viewer::detail {

DesignController::DesignController(ViewerCore* core, QObject* parent) : QObject(parent), core_(core) {
    if (core_) {
        connect(core_, &ViewerCore::dataMutated, this, &DesignController::changed);
        connect(core_, &ViewerCore::collectionsChanged, this, &DesignController::changed);
        connect(core_, &ViewerCore::databaseLoaded, this, &DesignController::changed);
    }
}

void DesignController::setCore(ViewerCore* core) {
    if (core_)
        disconnect(core_, nullptr, this, nullptr);
    core_ = core;
    if (core_) {
        connect(core_, &ViewerCore::dataMutated, this, &DesignController::changed);
        connect(core_, &ViewerCore::collectionsChanged, this, &DesignController::changed);
        connect(core_, &ViewerCore::databaseLoaded, this, &DesignController::changed);
    }
}

QStringList DesignController::collections() const {
    return core_ ? core_->collectionNames() : QStringList();
}

std::vector<FieldDef> DesignController::currentSchema(const QString& collection) const {
    std::vector<FieldDef> out;
    if (!core_ || !core_->database().hasCollection(collection.toStdString()))
        return out;
    const auto schema = core_->database().collection(collection.toStdString()).schema();
    if (!schema)
        return out;
    for (const auto& [path, fs] : schema->fields()) {
        FieldDef d;
        d.path = QString::fromStdString(path);
        d.typeName = QString::fromStdString(memdb::typeName(fs.type()));
        d.nullable = fs.nullable();
        if (fs.elementType())
            d.arrayElem = QString::fromStdString(memdb::typeName(*fs.elementType()));
        if (const memdb::Schema* sub = fs.sub()) {
            QStringList parts;
            for (const auto& [sp, sf] : sub->fields())
                parts << QString::fromStdString(sp + ":" + memdb::typeName(sf.type()));
            d.subSchema = parts.join(QStringLiteral(","));
        }
        out.push_back(std::move(d));
    }
    return out;
}

QStringList DesignController::fieldSuggestions(const QString& collection) const {
    QStringList out;
    if (!core_ || !core_->database().hasCollection(collection.toStdString()))
        return out;
    auto coll = core_->database().collection(collection.toStdString());
    if (const auto schema = coll.schema())
        for (const auto& [path, fs] : schema->fields())
            out << QString::fromStdString(path);
    if (out.isEmpty())
        for (const auto& v : coll.distinctValues("name")) // Fallback: Feld "name" falls vorhanden
            out << QString::fromStdString(memdb::toString(v));
    if (out.isEmpty()) {
        // Felder aus dem ersten Dokument ableiten
        coll.forEach(memdb::Expression{}, [&](const memdb::Document& d) {
            if (!out.isEmpty())
                return;
            d.forEachField([&](const std::string& k, const memdb::Value&) { out << QString::fromStdString(k); });
        });
    }
    return out;
}

void DesignController::createCollection(const QString& name) {
    if (core_ && !name.isEmpty())
        core_->createCollection(name);
}

void DesignController::dropCollection(const QString& name) {
    if (core_)
        core_->dropCollection(name);
}

void DesignController::clearCollection(const QString& name) {
    if (core_)
        core_->clearCollectionContent(name);
}

void DesignController::applySchema(const QString& collection, const std::vector<FieldDef>& fields) {
    if (!core_)
        return;
    memdb::Schema schema;
    for (const auto& f : fields) {
        if (f.path.isEmpty())
            continue;
        const memdb::Value::Type t = memdb::typeFromName(f.typeName.toStdString());
        auto& fs = schema.add(f.path.toStdString(), t);
        if (!f.nullable)
            fs.notNull();
        if (t == memdb::Value::Type::Array && !f.arrayElem.isEmpty())
            fs.elements(memdb::typeFromName(f.arrayElem.toStdString()));
        if (t == memdb::Value::Type::Document && !f.subSchema.isEmpty()) {
            memdb::Schema sub;
            const QStringList parts = f.subSchema.split(QLatin1Char(','), Qt::SkipEmptyParts);
            for (const QString& p : parts) {
                const int colon = p.indexOf(QLatin1Char(':'));
                if (colon > 0)
                    sub.add(p.left(colon).trimmed().toStdString(),
                            memdb::typeFromName(p.mid(colon + 1).trimmed().toStdString()));
            }
            if (sub.size() > 0)
                fs.subSchema(std::move(sub));
        }
    }
    core_->setCollectionSchema(collection, schema); // verifiziert gegen Bestand (SchemaError -> Log)
}

void DesignController::removeSchema(const QString& collection) {
    if (core_)
        core_->clearCollectionSchema(collection);
}

void DesignController::addIndex(const QString& collection, const QString& fieldPath, bool unique) {
    if (core_ && !fieldPath.isEmpty())
        core_->createIndex(collection, fieldPath, unique); // UniqueConstraintError -> Log
}

void DesignController::removeIndex(const QString& collection, const QString& fieldPath) {
    if (core_)
        core_->dropIndex(collection, fieldPath);
}

void DesignController::addReference(const QString& fromColl, quint64 fromId, bool hasFromId, const QString& toColl,
                                    quint64 toId, bool hasToId, const QString& relation) {
    if (core_)
        core_->linkReference(fromColl, fromId, hasFromId, toColl, toId, hasToId, relation);
}

void DesignController::removeReference(const QString& fromColl, quint64 fromId, bool hasFromId, const QString& toColl,
                                       quint64 toId, bool hasToId, const QString& relation) {
    if (core_)
        core_->unlinkReference(fromColl, fromId, hasFromId, toColl, toId, hasToId, relation);
}

void DesignController::pruneDangling() {
    if (core_)
        core_->pruneDanglingReferences();
}

} // namespace memdb::viewer::detail
