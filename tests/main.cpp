#include "test_viewer.hpp"

#include <QApplication>
#include <QTest>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    int rc = 0;
    auto run = [&rc, argc, argv](QObject* test) {
        rc |= QTest::qExec(test, argc, argv);
    };
    TestViewerCore core;
    run(&core);
    TestImportModel model;
    run(&model);
    TestDataInputWidget widget;
    run(&widget);
    TestDatabaseModel dbModel;
    run(&dbModel);
    TestDocumentTableModel tableModel;
    run(&tableModel);
    TestDocumentTreeModel treeModel;
    run(&treeModel);
    TestGraphScene graphScene;
    run(&graphScene);
    TestDataViewWidget viewWidget;
    run(&viewWidget);
    TestDiscoveryModel discModel;
    run(&discModel);
    TestAnalysisModels anaModels;
    run(&anaModels);
    TestAnalysisController anaController;
    run(&anaController);
    TestAnalysisWidget anaWidget;
    run(&anaWidget);
    TestViewerCoreMutations mutations;
    run(&mutations);
    TestOutputPanel outputPanel;
    run(&outputPanel);
    TestDesignController designController;
    run(&designController);
    TestDatabaseDesignWidget designWidget;
    run(&designWidget);
    TestClipboardCsvImport clipboardCsv;
    run(&clipboardCsv);
    TestDocumentCrudWidget crudWidget;
    run(&crudWidget);
    return rc;
}
