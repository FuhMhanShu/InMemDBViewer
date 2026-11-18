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
    void asyncImportQueue();
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

class TestDatabaseModel : public QObject {
    Q_OBJECT
private slots:
    void overviewAndRefresh();
};

class TestDocumentTableModel : public QObject {
    Q_OBJECT
private slots:
    void rowsColumnsAndValues();
    void queryFilterAndIndexDiagnostics();
    void invalidQueryReportsError();
};

class TestDocumentTreeModel : public QObject {
    Q_OBJECT
private slots:
    void nestedDocumentTree();
};

class TestGraphScene : public QObject {
    Q_OBJECT
private slots:
    void nodesAndEdgesMatchDbGraph();
    void clickSignal();
    void exportPngAndMermaid();
};

class TestDataViewWidget : public QObject {
    Q_OBJECT
private slots:
    void loadsAndSelectsCollection();
    void liveRefreshOnInsert();
    void applyQueryViaWidget();
};

class TestDiscoveryModel : public QObject {
    Q_OBJECT
private slots:
    void relationsTable();
};

class TestAnalysisModels : public QObject {
    Q_OBJECT
private slots:
    void modelsPopulate();
};

class TestAnalysisController : public QObject {
    Q_OBJECT
private slots:
    void discoversFkAndHealth();
    void scopeSingleCollection();
};

class TestAnalysisWidget : public QObject {
    Q_OBJECT
private slots:
    void runsAnalysisHeadless();
    void cancelDoesNotCrash();
};

class TestViewerCoreMutations : public QObject {
    Q_OBJECT
private slots:
    void collectionOps();
    void schemaOps();
    void indexOps();
    void referenceOps();
    void documentCrud();
    void csvTextImport();
    void snapshotSave();
    void dataMutatedOnImport();
};

class TestOutputPanel : public QObject {
    Q_OBJECT
private slots:
    void logsAndFilters();
};

class TestDesignController : public QObject {
    Q_OBJECT
private slots:
    void collectionSchemaIndexReferenceFlow();
};

class TestDatabaseDesignWidget : public QObject {
    Q_OBJECT
private slots:
    void reflectsCoreState();
};

class TestClipboardCsvImport : public QObject {
    Q_OBJECT
private slots:
    void importsFromClipboardWithDialog();
    void emptyClipboardReportsStatus();
};

class TestDocumentCrudWidget : public QObject {
    Q_OBJECT
private slots:
    void cellEditVerifiesAndMutates();
    void cellEditSchemaErrorIsLogged();
    void insertAndDeleteDocument();
    void invalidJsonIsLogged();
};

class TestAutoAnalysis : public QObject {
    Q_OBJECT
private slots:
    void mutationTriggersAutoAnalysis();
    void disabledDoesNotRun();
    void rapidMutationsAreDebounced();
    void rerunAfterBusyMutation();
};

class TestViewerTheme : public QObject {
    Q_OBJECT
private slots:
    void themeAppliesFusionPaletteAndStylesheet();
};
