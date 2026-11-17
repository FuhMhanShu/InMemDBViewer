#pragma once

#include <QObject>

class TestViewerCore : public QObject {
    Q_OBJECT
private slots:
    void importCsv();
    void importJson();
    void importJsonl();
    void importSqlDump();
    void importSqlite();
    void unknownExtensionFails();
    void encryptedSnapshotRoundtrip();
    void snapshotRoundtrip();
    void resetDatabaseEmitsSignals();
    void observerTriggersSignal();
    void autoSyncDetectsNewCollection();
    void demoDataLoads();
    void asyncImportWorks();
};

class TestImportModel : public QObject {
    Q_OBJECT
private slots:
    void addUpdateClear();
};

class TestDataInputWidget : public QObject {
    Q_OBJECT
private slots:
    void supportedFormatsListed();
    void dropCsvImports();
    void handleDroppedFilesAsync();
    void encryptedDropOffscreenIsHandled();
};
