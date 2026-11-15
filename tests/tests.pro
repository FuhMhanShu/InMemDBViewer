# QtTest-Tests fuer InMemDBViewer (NFA-05), headless lauffaehig:
#   mkdir build && cd build && qmake ../tests/tests.pro && make -j
#   QT_QPA_PLATFORM=offscreen ./viewer_tests
# InMemDB-Pfad ggf. setzen:  qmake "INMEMDB_DIR=/pfad/zu/InMemDB" ../tests/tests.pro
QT += widgets testlib
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = viewer_tests

INMEMDBVIEWER_DIR = $$clean_path($$_PRO_FILE_PWD_/..)
isEmpty(INMEMDB_DIR): INMEMDB_DIR = $$clean_path($$_PRO_FILE_PWD_/../../InMemDB)
DEFINES += INMEMDBVIEWER_BUILD_SOURCES

include($$INMEMDBVIEWER_DIR/InMemDBViewer.pri)

SOURCES += $$_PRO_FILE_PWD_/main.cpp $$_PRO_FILE_PWD_/test_viewer.cpp
HEADERS += $$_PRO_FILE_PWD_/test_viewer.hpp
