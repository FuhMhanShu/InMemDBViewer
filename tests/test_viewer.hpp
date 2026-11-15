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
    void nestedSchemaInScene();
};

class TestCollectionItem : public QObject {
    Q_OBJECT
private slots:
    void collapseExpand();
};

class TestViewerController : public QObject {
    Q_OBJECT
private slots:
    void observerTriggersSignal();
    void modelFollowsObserver();
    void selectCollection();
    void autoSyncDetectsNewAndDroppedCollection();
};

class TestMetricsPanel : public QObject {
    Q_OBJECT
private slots:
    void showsCollectionMetrics();
    void showsOverflowInfo();
};

class TestSnapshotExport : public QObject {
    Q_OBJECT
private slots:
    void openSnapshotRoundtrip();
    void openSnapshotEncrypted();
    void wrongPasswordFails();
    void exportPngAndMermaid();
    void exportSvgJsonMarkdown();
};

class TestDocumentTreeView : public QObject {
    Q_OBJECT
private slots:
    void nestedDocument();
};

class TestDocumentView : public QObject {
    Q_OBJECT
private slots:
    void insertAndDelete();
    void aggregate();
    void documentSelectedSignal();
};
