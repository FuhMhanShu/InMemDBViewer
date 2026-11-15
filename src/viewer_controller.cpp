#include "memdbviewer/viewer_controller.hpp"

#include "memdbviewer/database_model.hpp"

#include <memdb/change_event.hpp>
#include <memdb/collection.hpp>

namespace memdb::viewer {

ViewerController::ViewerController(QObject* parent) : QObject(parent), model_(new DatabaseModel(this)) {
    connect(this, &ViewerController::collectionChanged, model_, &DatabaseModel::refresh);
}

ViewerController::~ViewerController() {
    unregisterObservers();
}

void ViewerController::setDatabase(memdb::Database& db) {
    unregisterObservers();
    db_ = &db;
    model_->setDatabase(db_);
    registerObservers();
    emit databaseChanged();
}

void ViewerController::refresh() {
    registerObservers(); // auch zwischenzeitlich neu angelegte Collections abonnieren
    model_->refresh();
    emit databaseChanged();
}

void ViewerController::registerObservers() {
    if (!db_)
        return;
    for (const std::string& name : db_->collectionNames()) {
        if (observerHandles_.count(name))
            continue;
        Collection coll = db_->collection(name);
        const std::uint64_t handle = coll.onChange([this](const ChangeEvent& e) {
            // Events feuern ggf. aus Nicht-GUI-Threads: queued in den GUI-Thread heben (NFA-04)
            QMetaObject::invokeMethod(
                this,
                [this, collection = e.collection] {
                    emit collectionChanged(QString::fromStdString(collection));
                },
                Qt::QueuedConnection);
        });
        observerHandles_[name] = handle;
    }
}

void ViewerController::unregisterObservers() {
    if (!db_)
        return;
    for (const auto& [name, handle] : observerHandles_) {
        if (db_->hasCollection(name))
            db_->collection(name).offChange(handle);
    }
    observerHandles_.clear();
}

} // namespace memdb::viewer
