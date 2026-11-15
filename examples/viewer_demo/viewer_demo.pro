# Demo-App (FA-16): InMemDBViewer als Bibliothek ueber InMemDBViewer.pri einbinden.
# Build (Shadow-Build empfohlen):
#   mkdir build && cd build && qmake ../examples/viewer_demo/viewer_demo.pro && make -j
# Run (GUI):          ./viewer_demo
# Run (headless/CI):  QT_QPA_PLATFORM=offscreen ./viewer_demo   (beendet sich nach 300 ms selbst)
#
# InMemDB-Pfad ggf. setzen:  qmake "INMEMDB_DIR=/pfad/zu/InMemDB" ../examples/viewer_demo/viewer_demo.pro
QT += widgets
CONFIG += c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = viewer_demo

INMEMDBVIEWER_DIR = $$clean_path($$_PRO_FILE_PWD_/../..)
isEmpty(INMEMDB_DIR): INMEMDB_DIR = $$clean_path($$_PRO_FILE_PWD_/../../../InMemDB)

# Variante B: Viewer- + InMemDB-Quellen direkt mitkompilieren (kein Vorab-Build noetig).
DEFINES += INMEMDBVIEWER_BUILD_SOURCES

include($$INMEMDBVIEWER_DIR/InMemDBViewer.pri)

SOURCES += $$_PRO_FILE_PWD_/main.cpp
