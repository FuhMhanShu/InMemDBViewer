#pragma once

#include <QObject>

class TestDatabaseModel : public QObject {
    Q_OBJECT
private slots:
    void overview();
    void refreshAfterInsert();
};

class TestSchemaScene : public QObject {
    Q_OBJECT
private slots:
    void nodesAndEdges();
    void schemaFieldsRendered();
};

class TestViewerController : public QObject {
    Q_OBJECT
private slots:
    void observerTriggersSignal();
    void modelFollowsObserver();
};
