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
    return rc;
}
