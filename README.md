# InMemDBViewer
Visualisierung zu [InMemDB](https://github.com/FuhMhanShu/InMemDB) – C++/Qt, MVC-Architektur, als Bibliothek ausgeliefert.

- [Anforderungen](anforderungen.md) (v0.1)

## Bibliothek (v0.2)

Öffentliche API unter `include/memdbviewer/`, Namespace `memdb::viewer` (MVC, FA-01/FA-02):

| Klasse | Schicht | Zweck |
|---|---|---|
| `viewer::ViewerController` | Controller | hält die `memdb::Database`, Observer-Bridge (`Collection::onChange` → queued Qt-Signale, FA-08), stellt das Model bereit |
| `viewer::DatabaseModel` | Model | `QAbstractItemModel`: Collections als Top-Level (Dokumente/Schema/Indizes, FA-03), Schema-Felder als Kind-Zeilen |
| `viewer::SchemaScene` + `viewer::CollectionItem` | View | QGraphicsFramework (FA-04): Collection-Knoten mit Schema-Metadaten, FA-18-Referenzen als Kanten (FA-05) |
| `viewer::SchemaView` | View | `QGraphicsView` mit Pan/Zoom (FA-09) |
| `viewer::DocumentView` | View | Dokument-Tabelle auf `memdb::qt::MemDbTableModel` (v0.23) mit DSL-Query-Filter und Index-Diagnose (FA-07/FA-10) |

Voraussetzungen: C++17-Compiler, Qt 5.15+ oder Qt 6 (Widgets), InMemDB (Default: Schwestern-Checkout `../InMemDB`, sonst Pfad setzen).

### Build & Tests (qmake)

```sh
qmake InMemDBViewer.pro && make -j            # -> libInMemDBViewer.a im Repo-Root
qmake tests/tests.pro && make -j
QT_QPA_PLATFORM=offscreen ./viewer_tests      # QtTest headless (NFA-05)
```

### Build & Tests (CMake)

```sh
cmake -S . -B build -DINMEMDB_DIR=../InMemDB
cmake --build build -j
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

### Einbindung in ein eigenes qmake-Projekt

```qmake
QT += widgets
INMEMDBVIEWER_DIR = /pfad/zu/InMemDBViewer
INMEMDB_DIR = /pfad/zu/InMemDB
include($$INMEMDBVIEWER_DIR/InMemDBViewer.pri)  # Variante A: libInMemDBViewer.a linken
# oder: DEFINES += INMEMDBVIEWER_BUILD_SOURCES   # Variante B: Quellen mitkompilieren
```

### Demo-App (FA-16)

```sh
qmake examples/viewer_demo/viewer_demo.pro && make -j && ./viewer_demo
QT_QPA_PLATFORM=offscreen ./viewer_demo        # CI-Smoke-Test, beendet sich selbst
```

### Mini-Beispiel

```cpp
#include <memdb/memdb.hpp>
#include <memdbviewer/viewer.hpp>

memdb::Database db("demo");
db.createCollection("users").insert({{"name", "Ada"}});

memdb::viewer::ViewerController controller;
controller.setDatabase(db);          // Model + Observer aufsetzen

memdb::viewer::SchemaScene scene;    // QGraphicsFramework-Szene
scene.setDatabase(db);               // z. B. in memdb::viewer::SchemaView anzeigen
```
