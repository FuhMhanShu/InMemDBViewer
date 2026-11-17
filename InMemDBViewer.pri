# InMemDBViewer.pri – InMemDBViewer in ein qmake-Projekt einbinden (FA-01).
#
# Verwendung in deiner .pro-Datei:
#   INMEMDBVIEWER_DIR = /pfad/zu/InMemDBViewer   # optional; Default = Verzeichnis dieser .pri
#   INMEMDB_DIR       = /pfad/zu/InMemDB         # optional; Default ../InMemDB relativ zum Viewer
#   include($$INMEMDBVIEWER_DIR/InMemDBViewer.pri)
#   qmake "CONFIG+=viewer_sqlite" …             # optionaler SQLite-Import (libsqlite3 noetig)
#
# Zwei Varianten:
#   A) Default: vorgebaute statische Library linken.
#      Library einmalig bauen:  qmake InMemDBViewer.pro && make   (im InMemDBViewer-Verzeichnis)
#      -> libInMemDBViewer.a landet im InMemDBViewer-Root (enthaelt bereits die InMemDB-Quellen).
#      Build-Verzeichnis der Lib ggf. setzen:  INMEMDBVIEWER_BUILD_DIR = /pfad/zum/build
#   B) Quellen direkt mitkompilieren (kein extra Build-Schritt):
#      DEFINES += INMEMDBVIEWER_BUILD_SOURCES vor dem include() setzen.

isEmpty(INMEMDBVIEWER_DIR): INMEMDBVIEWER_DIR = $$PWD
isEmpty(INMEMDBVIEWER_BUILD_DIR): INMEMDBVIEWER_BUILD_DIR = $$INMEMDBVIEWER_DIR
isEmpty(INMEMDB_DIR): INMEMDB_DIR = $$clean_path($$INMEMDBVIEWER_DIR/../InMemDB)

QT += widgets svg
CONFIG += c++17

# Optionaler SQLite-Import (vor include(InMemDB.pri), damit Variante B import_sqlite.cpp aufnimmt)
contains(CONFIG, viewer_sqlite) {
    DEFINES += INMEMDB_HAS_SQLITE
    LIBS += -lsqlite3
}

INCLUDEPATH += $$INMEMDBVIEWER_DIR/include
INCLUDEPATH += $$INMEMDB_DIR/include

contains(DEFINES, INMEMDBVIEWER_BUILD_SOURCES) {
    # Variante B: Viewer- + InMemDB-Quellen direkt ins Projekt aufnehmen
    DEFINES += INMEMDB_BUILD_SOURCES
    include($$INMEMDB_DIR/InMemDB.pri)
    SOURCES += $$files($$INMEMDBVIEWER_DIR/src/*.cpp, false)
    HEADERS += $$files($$INMEMDBVIEWER_DIR/include/memdbviewer/*.hpp, false)
    HEADERS += $$files($$INMEMDBVIEWER_DIR/src/*.hpp, false)
} else {
    # Variante A: vorgebaute statische Lib (enthaelt InMemDB- und moc-Objekte bereits)
    LIBS += -L$$INMEMDBVIEWER_BUILD_DIR -lInMemDBViewer
}
