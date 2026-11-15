# InMemDBViewer
Visualisierung zu [InMemDB](https://github.com/FuhMhanShu/InMemDB) – C++/Qt, MVC-Architektur, als Bibliothek ausgeliefert.

- [Anforderungen](anforderungen.md) (v0.1)

## Bibliothek (v0.4)

Öffentliche API unter `include/memdbviewer/`, Namespace `memdb::viewer` (MVC, FA-01/FA-02):

| Klasse | Schicht | Zweck |
|---|---|---|
| `viewer::ViewerController` | Controller | hält die `memdb::Database`, Observer-Bridge (`Collection::onChange` → queued Qt-Signale, FA-08), stellt das Model bereit |
| `viewer::DatabaseModel` | Model | `QAbstractItemModel`: Collections als Top-Level (Dokumente/Schema/Indizes, FA-03), Schema-Felder als Kind-Zeilen |
| `viewer::SchemaScene` + `viewer::CollectionItem` | View | QGraphicsFramework (FA-04): Collection-Knoten mit Schema-Metadaten, FA-18-Referenzen als Kanten (FA-05) |
| `viewer::SchemaView` | View | `QGraphicsView` mit Pan/Zoom (FA-09) |
| `viewer::DocumentView` | View | Dokument-Tabelle auf `memdb::qt::MemDbTableModel` (v0.23) mit DSL-Query-Filter und Index-Diagnose (FA-07/FA-10) |
| `viewer::MetricsPanel` | View | Metriken (`CollectionMetrics`/`DatabaseMetrics`, v0.22) inkl. Hybrid-/Overflow-Info (FA-11/FA-13) |
| `viewer::DocumentTreeView` | View | Detailbaum eines Dokuments: Felder mit Typ/Wert, verschachtelte Dokumente/Arrays rekursiv (FA-07) |

### Weitere Features (v0.4)
- FA-14 CRUD-Editing in `DocumentView`: Zellen editierbar (schreibt via `updateById`), „Neu (leer)“/„Löschen“, Auto-Refresh per Observer
- FA-17 Aggregationen in `DocumentView`: count/min/max/sum/avg über optionalen DSL-Filter (`Collection::aggregate`)

### Features (v0.3/v0.4)
- Schema-Szene (QGraphicsFramework): Collection-Knoten mit rekursivem, per Doppelklick auf-/zuklappbarem Schema (FA-06), FA-18-Referenzen als Kanten mit Dangling-Markierung (FA-05)
- Bidirektionale Selektion Baum ↔ Szene über `ViewerController::collectionSelected` (FA-09)
- Live-Update per Observer (`onChange` → queued Signale, FA-08)
- Snapshot laden inkl. verschlüsselter Snapshots: `ViewerController::openSnapshot/openSnapshotEncrypted` (FA-12)
- Export: Szene als PNG und SVG (`SchemaScene::exportPng/exportSvg`, QtSvg), Graph als Mermaid/JSON/Markdown (`SchemaScene::mermaid/json/markdown`, FA-15)
### Auto-Sync: neue/gelöschte Collections erkennen (v0.4)
InMemDB (Stand v0.24) feuert `Collection::onChange` nur für Dokumentänderungen; `createCollection`/`dropCollection`/`clear` erzeugen kein Database-Level-Event. Der `ViewerController` gleicht daher zusätzlich per leichtem QTimer (Default 250 ms, nur `collectionNames()`-Vergleich) den Collection-Stand ab: neue Collections werden automatisch abonniert, gelöschte abgemeldet, Model und Szene aktualisiert. Steuerung: `setAutoSyncEnabled(bool)`, `setAutoSyncInterval(int)`, `isAutoSyncEnabled()`.

### CI
GitHub Actions (`.github/workflows/ci.yml`): CMake-Build + QtTest (headless) + Demo-Smoke-Test für Qt 6 (ubuntu-latest) und Qt 5.15 (ubuntu-22.04) sowie ein qmake-Build (Bibliothek, Tests, Demo). Das Schwester-Repositorium InMemDB wird per `actions/checkout` geladen; beide Repositories sind öffentlich, daher ist kein Secret erforderlich und Standard-Runner verbrauchen keine Actions-Minuten.

Voraussetzungen: C++17-Compiler, Qt 5.15+ oder Qt 6 (Widgets, Svg), InMemDB (Default: Schwestern-Checkout `../InMemDB`, sonst Pfad setzen).

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
