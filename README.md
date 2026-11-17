# InMemDBViewer

Visualisierungs- und **Analysetool** für [InMemDB](https://github.com/FuhMhanShu/InMemDB) – C++17/Qt, strikte MVC-Architektur, als Bibliothek + App.

- [Anforderungen](anforderungen.md) (v1.0)

## Konzept (v1.0)

Die GUI besteht aus drei **gekapselten MVC-Widgets**, die alle an einem gemeinsamen Model (`ViewerCore`: hält `memdb::Database` + `ReferenceIndex`) hängen:

```
MainWindow: oben uebergreifend "Daten laden" (Menue/Toolbar/Drag & Drop)
  ├─ DataInputWidget   – Dateneingabe: Datei-Dialog, Drag & Drop, Import-Optionen, Verlauf
  ├─ DataViewWidget    – Bereich Datenbetrachtung: Struktur (QGraphicsFramework/DbGraph),
  │                      tabellarische Ansicht (virtualisiert, DSL-Filter), Dokument-Detail
  └─ AnalysisWidget    – Bereich Analyse: Discovery (FA-27) + Analyse (FA-29..FA-32, v0.33/v0.40),
                         Worker-Thread, Health-Score, Report-Export
```

**Daten laden** (übergeordnet, InMemDB FA-21 `ImportRegistry`/`Importer`): CSV, JSON/JSONL/NDJSON, SQL-Dump, BSON, SQLite (optional via `INMEMDB_WITH_SQLITE`), InMemDB-Snapshot (`.inmemdb.json`, inkl. verschlüsselt `.enc` mit Passwort, FA-14/FA-19). Geladene Daten stehen Datenbetrachtung und Analyse zur Verfügung; Änderungen werden live per Observer (FA-15) an alle Widgets verteilt.

## Screenshots

Erzeugt mit der App (`examples/viewer_app`, headless via `QT_QPA_PLATFORM=offscreen` gerendert), Daten: Demo-Daten + CSV- und SQLite-Import:

| Übersicht nach dem Laden | Datenbetrachtung: Struktur (QGraphicsFramework) |
|---|---|
| ![App-Übersicht](docs/images/app-overview.png) | ![Struktur-Graph](docs/images/datenbetrachtung-struktur.png) |

| Datenbetrachtung: Tabelle mit DSL-Filter | Analyse: Übersicht + Health-Score |
|---|---|
| ![Tabelle mit Query-Filter](docs/images/datenbetrachtung-tabelle.png) | ![Analyse-Übersicht](docs/images/analyse-uebersicht-health.png) |

| Analyse: Feld-Statistik (FA-30) mit Histogramm | Analyse: Discovery (FA-27) Relationen |
|---|---|
| ![Feld-Statistik + Histogramm](docs/images/analyse-feldstatistik-histogramm.png) | ![Discovery](docs/images/analyse-discovery.png) |

![Gesamtfenster nach Analyse](docs/images/app-gesamt-nach-analyse.png)

## Öffentliche API (Namespace `memdb::viewer`, Header unter `include/memdbviewer/`)

| Klasse | Schicht | Zweck |
|---|---|---|
| `ViewerCore` | Model | hält `memdb::Database` + `ReferenceIndex`; Import/Snapshot laden (sync + async), Observer-Bridge → queued Signale (`collectionChanged`, `collectionsChanged`, `databaseLoaded`) |
| `DataInputWidget` | View | Dateneingabe-Panel (Dialog, Drag & Drop, Formate, Optionen, Verlauf, Status) |
| `DataViewWidget` | View | Datenbetrachtung: Datenbank-Übersicht, Struktur-Szene (QGraphicsFramework), Dokument-Tabelle, Dokument-Detail |
| `AnalysisWidget` | View | Analyse: Discovery + Analyse im Worker-Thread, Health-Ampel, Report-Export (Markdown/JSON/Mermaid) |

Interne MVC-Teile (Model/Controller je Widget) liegen gekapselt in `src/` (`ImportModel`, `DataInputController`, …).

## Voraussetzungen

C++17-Compiler, Qt 5.15+ oder Qt 6 (Widgets, Svg, Test), InMemDB (Default: Schwestern-Checkout `../InMemDB`, sonst Pfad setzen). Optionaler SQLite-Import: `libsqlite3-dev` + Build-Option.

## Build & Tests (CMake)

```sh
cmake -S . -B build -DINMEMDB_DIR=../InMemDB                 # + -DINMEMDBVIEWER_WITH_SQLITE=ON fuer SQLite-Import
cmake --build build -j
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
QT_QPA_PLATFORM=offscreen ./build/viewer_app                  # App-Smoke-Test (headless, beendet sich selbst)
```

## Build & Tests (qmake)

```sh
qmake InMemDBViewer.pro && make -j                 # -> libInMemDBViewer.a (ggf. "CONFIG+=viewer_sqlite")
mkdir build-tests && cd build-tests && qmake ../tests/tests.pro && make -j
QT_QPA_PLATFORM=offscreen ./viewer_tests           # QtTest headless (NFA-05)
mkdir build-app && cd build-app && qmake ../examples/viewer_app/viewer_app.pro && make -j
QT_QPA_PLATFORM=offscreen ./viewer_app              # App-Smoke-Test
```

## Einbindung in ein eigenes qmake-Projekt

```qmake
QT += widgets svg
INMEMDBVIEWER_DIR = /pfad/zu/InMemDBViewer
INMEMDB_DIR = /pfad/zu/InMemDB
include($$INMEMDBVIEWER_DIR/InMemDBViewer.pri)  # Variante A: libInMemDBViewer.a linken
# oder: DEFINES += INMEMDBVIEWER_BUILD_SOURCES   # Variante B: Quellen mitkompilieren
# optional: CONFIG += viewer_sqlite               # SQLite-Import (libsqlite3)
```

## Mini-Beispiel

```cpp
#include <memdbviewer/viewer.hpp>

memdb::viewer::ViewerCore core;                 // Model: haelt die Database
core.loadDemoData();                            // oder: core.importFile("daten.csv");

memdb::viewer::DataInputWidget input;           // Dateneingabe (Dialog + Drag & Drop)
memdb::viewer::DataViewWidget view;             // Datenbetrachtung (Struktur/Tabelle/Dokument)
memdb::viewer::AnalysisWidget analysis;         // Analyse (Discovery + Analyse, Worker-Thread)
input.setCore(&core);
view.setCore(&core);
analysis.setCore(&core);
```

## CI

GitHub Actions (`.github/workflows/ci.yml`): CMake-Build + QtTest (headless) + App-Smoke-Test für Qt 6 (mit SQLite-Import) und Qt 5.15 sowie ein qmake-Build inkl. SQLite. InMemDB wird als öffentlicher Schwestern-Checkout mitgebaut (kein Secret nötig).

## Lizenz

MIT (wie InMemDB).
