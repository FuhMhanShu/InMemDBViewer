# InMemDBViewer-App (GA-01): Shell mit Dateneingabe (DataInputWidget), Bereich
# Datenbetrachtung und Bereich Analyse – alle am gemeinsamen ViewerCore.
# Build (Shadow-Build empfohlen):
#   mkdir build && cd build && qmake ../examples/viewer_app/viewer_app.pro && make -j
# Run (GUI):          ./viewer_app
# Run (headless/CI):  QT_QPA_PLATFORM=offscreen ./viewer_app   (beendet sich nach 300 ms selbst)
#
# InMemDB-Pfad ggf. setzen:  qmake "INMEMDB_DIR=/pfad/zu/InMemDB" ../examples/viewer_app/viewer_app.pro
# Optionaler SQLite-Import:  qmake "CONFIG+=viewer_sqlite" …
QT += widgets svg
CONFIG += c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = viewer_app

INMEMDBVIEWER_DIR = $$clean_path($$_PRO_FILE_PWD_/../..)
isEmpty(INMEMDB_DIR): INMEMDB_DIR = $$clean_path($$_PRO_FILE_PWD_/../../../InMemDB)

# Variante B: Viewer- + InMemDB-Quellen direkt mitkompilieren (kein Vorab-Build noetig).
DEFINES += INMEMDBVIEWER_BUILD_SOURCES

include($$INMEMDBVIEWER_DIR/InMemDBViewer.pri)

SOURCES += $$_PRO_FILE_PWD_/main.cpp
