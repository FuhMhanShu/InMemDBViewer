#include <QApplication>
#include <QTest>

#include "test_viewer.hpp"

int main(int argc, char** argv) {
    QApplication app(argc, argv); // Widgets -> QApplication; laeuft headless via QT_QPA_PLATFORM=offscreen
    int status = 0;
    {
        TestDatabaseModel t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestSchemaScene t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestViewerController t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestCollectionItem t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestMetricsPanel t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestSnapshotExport t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestDocumentTreeView t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestDocumentView t;
        status |= QTest::qExec(&t, argc, argv);
    }
    return status;
}
