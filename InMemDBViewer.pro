# InMemDBViewer als statische Qt-Bibliothek mit qmake bauen:
#   qmake InMemDBViewer.pro && make -j        -> libInMemDBViewer.a im Repo-Root
# Pfad zum InMemDB-Repo ggf. setzen (Default: Schwestern-Checkout ../InMemDB):
#   qmake "INMEMDB_DIR=/pfad/zu/InMemDB" InMemDBViewer.pro
#
# Einbindung in fremde qmake-Projekte siehe InMemDBViewer.pri.
TEMPLATE = lib
TARGET = InMemDBViewer
CONFIG += staticlib c++17
QT += widgets svg

DESTDIR = $$_PRO_FILE_PWD_
OBJECTS_DIR = $$_PRO_FILE_PWD_/build-qmake/obj
MOC_DIR = $$_PRO_FILE_PWD_/build-qmake/moc
UI_DIR = $$_PRO_FILE_PWD_/build-qmake/ui
RCC_DIR = $$_PRO_FILE_PWD_/build-qmake/rcc

INCLUDEPATH += $$_PRO_FILE_PWD_/include

# InMemDB-Quellen direkt mitkompilieren (Variante B aus InMemDB.pri);
# INMEMDB_WITH_QT setzt InMemDB.pri automatisch (QT enthaelt core), damit der
# Qt-Modell-Adapter memdb::qt::MemDbTableModel (v0.23) verfuegbar ist.
isEmpty(INMEMDB_DIR): INMEMDB_DIR = $$clean_path($$_PRO_FILE_PWD_/../InMemDB)
DEFINES += INMEMDB_BUILD_SOURCES
include($$INMEMDB_DIR/InMemDB.pri)

SOURCES += $$files($$_PRO_FILE_PWD_/src/*.cpp, false)
HEADERS += $$files($$_PRO_FILE_PWD_/include/memdbviewer/*.hpp, false)
