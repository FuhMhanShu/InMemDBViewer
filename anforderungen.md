# Anforderungen

## InMemDBViewer – Qt-Visualisierung für InMemDB (C++/Qt, MVC, Bibliothek)

| Feld | Wert |
|---|---|
| Projekttitel | InMemDBViewer – grafische Visualisierung von InMemDB-Datenbanken |
| Version | 0.1 |
| Status | Entwurf |
| Sprache | Deutsch |
| Technologie | C++17, Qt (Widgets + Graphics View Framework), qmake (.pro/.pri) **und** CMake |
| Bereitstellungsform | Wiederverwendbare **C++/Qt-Bibliothek** (statische Library mit Pimpl-Idiom, öffentliche Header unter `include/memdbviewer/`, Implementierung in `src/`), einbindbar via `InMemDBViewer.pri` (qmake) oder CMake-Target; plus schlanke Demo-App unter `examples/`. Namespace `memdb::viewer`. |
| Abhängigkeiten | Qt (core, gui, widgets) + [InMemDB](https://github.com/FuhMhanShu/InMemDB) (statische Lib oder Quellen via `InMemDB.pri`). Keine weiteren externen Pflichtabhängigkeiten. |

---

## 1. Zielbestimmung

### 1.1 Produktzweck
InMemDBViewer ist eine **C++/Qt-Bibliothek**, die eine laufende `memdb::Database` (oder einen Snapshot) **grafisch visualisiert**: Collections und deren Metadaten (Schema, Indizes, Referenzen, Metriken) werden mit dem **Qt Graphics View Framework** (`QGraphicsScene`/`QGraphicsView`/`QGraphicsItem`) als Diagramm dargestellt, Dokumente werden in Tabellen-/Baum-Views angezeigt. Die Architektur folgt strikt **MVC** (Model–View–Controller).

### 1.2 Nutzen
- Schnelle visuelle Inspektion einer In-Memory-Datenbank: Welche Collections existieren, welches Schema haben sie, welche Indizes/Referenzen sind definiert?
- Live-Sicht auf Änderungen (Observer-Events) ohne manuelles Neuladen.
- Debugging-Hilfe für Queries (Index-Nutzung vs. Vollscan), Transaktionen und Hybrid-Modus.
- Wiederverwendbar: als Bibliothek in eigene Qt-Anwendungen einbettbar, nicht nur als standalone App.

### 1.3 Abgrenzung (Nicht-Ziele)
- Kein Ersatz für die InMemDB-API; der Viewer liest (v1) und schreibt nur optional (FA-14).
- Kein Server, kein Netzwerk, keine Remote-Datenbank.
- Keine Persistenz eigener Daten; Snapshot-Laden/-Speichern geht über InMemDB (`Database::loadFromFile`/`saveToFile`).
- InMemDB selbst bleibt Qt-frei (harte Qt-Abhängigkeit nur im Viewer).

---

## 2. Produktumfeld / Systemkontext

- **Einsatz**: als Bibliothek in Qt-Desktop-Anwendungen (Linux, Windows, macOS) sowie als Demo-App (`examples/viewer_demo`) zum direkten Ausprobieren.
- **Qt-Versionen**: Qt 5.15+ und Qt 6.x (Build über qmake und CMake).
- **Datenquelle**: `memdb::Database&` (laufende Instanz) oder JSON-Snapshot-Datei (`Database::loadFromFile`, `DbGraph::fromSnapshotFile`).
- **Threading**: GUI-Thread only; InMemDB-Observer-Events werden per `Qt::QueuedConnection` in den GUI-Thread gehoben (so wie `memdb::qt::MemDbTableModel`, v0.23).
- **Headless/CI**: Views müssen mit `QT_QPA_PLATFORM=offscreen` testbar sein.

---

## 3. Architektur (MVC)

| Schicht | Verantwortung | Konkrete Ausprägung |
|---|---|---|
| **Model** | Kapselt `memdb::Database`/`Collection` und liefert Qt-Models;bildet `DbGraph` (FA-22) auf ein Szenen-Modell ab; hört per `Collection::onChange` (FA-15) auf Änderungen. | `DatabaseModel` (QAbstractItemModel: Top-Level = Collections), `SchemaModel`, `DocumentModel` (nutzt `memdb::qt::MemDbTableModel` v0.23 oder erweitert ihn), `GraphSceneModel`. Reines QtCore/QtWidgets, keine QGraphicsItem-Logik. |
| **View** | Reine Darstellung, kennt das Model nur über Qt-Model-Interfaces/Signale. | `SchemaSceneView` (`QGraphicsView` + `QGraphicsScene`: Collection-Knoten als `QGraphicsItem`, Referenzen als Kanten), `DocumentTableView` (`QTableView`), `DocumentTreeView` (verschachtelte Dokumente), Metrik-Panel. |
| **Controller** | Vermittelt zwischen View und Model: Selektion, Interaktionen, Query-Ausführung, Refresh; enthält keine UI. | `ViewerController` (QObject, verdrahtet Signale/Slots, stößt `refresh()`/`setQuery()` an, reichert Observer-Events als Qt-Signale an). |

Regeln: Views kennen keine `memdb::`-Typen direkt (nur QVariant/Qt-Models); der Controller kennt keine konkreten Widgets; Models emittieren nur Qt-Signale (`dataChanged`, eigene Signale) – keine direkten View-Aufrufe.

---

## 4. Funktionale Anforderungen

Prioritäten: **M** = Muss, **S** = Soll, **K** = Kann.

| ID | Anforderung | Prio |
|---|---|---|
| FA-01 | **Bibliotheksform**: InMemDBViewer wird als wiederverwendbare Bibliothek ausgeliefert (statische Library, Pimpl, öffentliche Header unter `include/memdbviewer/`, Namespace `memdb::viewer`); Einbindung via `InMemDBViewer.pro` + `InMemDBViewer.pri` (analog `InMemDB.pri`, bindet InMemDB über `INMEMDB_DIR`/`InMemDB.pri` ein) **und** via CMake (`find_package`- bzw. `add_subdirectory`-Target). | M |
| FA-02 | **MVC-Architektur** gemäß Abschnitt 3: strikte Trennung Model/View/Controller, Kommunikation ausschließlich über Qt-Models und Signale/Slots. | M |
| FA-03 | **Datenbank-Übersicht**: Anzeige aller Collections (`Database::collectionNames()`) mit Dokumentanzahl (`Collection::size()`), Schema-Status (`hasSchema()`) und Indexanzahl (`indexes().size()`). | M |
| FA-04 | **Metadaten-Anzeige pro Collection mit dem QGraphicsFramework**: Jede Collection wird als Knoten (`QGraphicsItem`, z. B. gerahmtes Rechteck mit Titel) in einer `QGraphicsScene` dargestellt; das Schema (FA-20: Feldpfad, `Value::Type`, `nullable`/`notNull`, Unter-Schema, Array-Elementtyp) wird als Liste innerhalb des Knotens gerendert; Indizes (FA-10) werden am Feld markiert (`index`/`unique`). | M |
| FA-05 | **Referenzen als Kanten**: FA-18-Referenzen (`ReferenceIndex` bzw. `DbGraph::edges`) werden als gerichtete Kanten (`QGraphicsLineItem`/Pfad) zwischen Collection-Knoten gezeichnet, inkl. Relation-Name und Dangling-Markierung. | S |
| FA-06 | **Rekursive Schema-Darstellung**: Verschachtelte Unter-Schemas (`FieldSchema::sub()`) werden aufklappbar/eingerückt dargestellt (expand/collapse im Knoten). | S |
| FA-07 | **Dokument-Anzeige**: Pro Collection eine Tabellenansicht der Dokumente (Zeilen = Dokumente, Spalten = Felder; Basis: `memdb::qt::MemDbTableModel` v0.23 inkl. DSL-Query-Filter `setQuery`) sowie eine Detail-/Baumansicht eines selektierten Dokuments (verschachtelte Dokumente/Arrays als Baum, Werttypen farblich kenntlich). | M |
| FA-08 | **Live-Aktualisierung**: Änderungen an der Database (insert/update/remove/clear, auch nach Transaktions-Commit) werden über `Collection::onChange` (FA-15) per queued Qt-Signal im Model registriert und lösen `dataChanged`/Szenen-Refresh aus – ohne manuelles Reload. | M |
| FA-09 | **Interaktion in der Szene**: Selektion von Collection-Knoten (Detail-Panel wechselt), Pan/Zoom der `QGraphicsView`, Doppelklick auf Knoten klappt Schema auf/zu. | S |
| FA-10 | **Query-Visualisierung**: Eingabe einer String-DSL-Query; Ausführung über `Collection::find(dsl)`; Ergebnismenge wird in Dokumenten- und Szenenansicht gehighlightet; Diagnoseanzeige, ob die letzte Query einen Index nutzte (`lastQueryUsedIndex`, `lastQueryCandidateCount` vs. `size()`). | S |
| FA-11 | **Metrik-Anzeige**: `Collection::metrics()` / `Database::metrics()` (v0.22) als Panel/Overlay im Knoten oder Detailbereich (inserts/updates/removes/queries, Index-Nutzung, Query-Dauer, Tx-Commits/Rollbacks, Spill-IO). | S |
| FA-12 | **Snapshot laden**: Laden einer JSON-Snapshot-Datei (`Database::loadFromFile`) in den Viewer; optional verschlüsselte Snapshots (`loadFromFileEncrypted`) mit Passwort-Dialog; Graph-Ansicht auch direkt aus Datei (`DbGraph::fromSnapshotFile`). | S |
| FA-13 | **Hybrid-/Overflow-Info**: Anzeige von `Collection::overflowInfo()` (enabled, inMemory, spilled, spillBytes) am Collection-Knoten. | K |
| FA-14 | **Bearbeiten (CRUD über GUI)**: Dokumente einfügen/bearbeiten/löschen über die Views; schreibt über `Collection::insert/updateById/removeById` zurück (Observer sorgt für Auto-Refresh). | K |
| FA-15 | **Export der Visualisierung**: Szene als Bild exportieren (`QGraphicsScene::render` → PNG/SVG) sowie textbasierte Exporte der InMemDB (Mermaid/JSON/Markdown via `DbGraph::toMermaid()/toJson()/toMarkdown()`). | K |
| FA-16 | **Demo-App**: Schlankes lauffähiges Beispiel unter `examples/` (analog `examples/qt_table` in InMemDB), das die Bibliothek über `InMemDBViewer.pri` einbindet, eine Demo-Database befüllt und alle Views zeigt; headless-tauglich (`QT_QPA_PLATFORM=offscreen`, beendet sich selbst – CI-Smoke-Test). | M |
| FA-17 | **Aggregations-Anzeige**: `aggregate`/`groupBy` (FA-13 in InMemDB) pro Collection ausführbar und als Balken-/Tabellen-Overlay darstellbar. | K |

---

## 5. Nicht-funktionale Anforderungen

| ID | Anforderung | Prio |
|---|---|---|
| NFA-01 | **Qt-Kompatibilität**: Qt 5.15+ und Qt 6.x; Module `core gui widgets` (Graphics View ist Teil von `widgets`). C++17. | M |
| NFA-02 | **Build**: qmake (`InMemDBViewer.pro` als lib + `InMemDBViewer.pri` zum Einbinden, Shadow-Build-kompatibel via `$$_PRO_FILE_PWD_`) **und** CMake ≥ 3.20 (analog InMemDB). Keine externen Pflichtabhängigkeiten außer Qt und InMemDB. | M |
| NFA-03 | **Performance/Usability**: Szenen mit Hunderten Collections und Tausenden Dokumenten bleiben flüssig (Pan/Zoom in O(1) durch QGraphicsView; Dokumenten-Views virtualisiert via Qt-Model; keine Voll-Kopien aller Dokumente im Model – nur IDs (`findIds`/`forEach`), Details per `findById` on demand). | S |
| NFA-04 | **Thread-Sicherheit**: Alle GUI-Zugriffe im GUI-Thread; InMemDB-Observer-Callbacks dürfen aus anderen Threads feuern und müssen per `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` umgeleitet werden (Muster siehe `MemDbTableModel`). | M |
| NFA-05 | **Testbarkeit**: Unit-Tests mit QtTest für Model und Controller (Selektion, Refresh nach Observer-Event, Query-Filter); headless lauffähig via `QT_QPA_PLATFORM=offscreen`; CI-Smoke-Test der Demo-App. | S |
| NFA-06 | **Dokumentation**: Doxygen-kommentierte öffentliche API (`include/memdbviewer/`), README mit Quickstart (Einbindung via .pri und CMake, Mini-Beispiel), Verweis auf `docs/qt-qmake.md` in InMemDB. | S |
| NFA-07 | **Anpassbarkeit**: Farben/Typ-Markierungen (Schema-Typen, Index, Dangling) über zentrale Konstanten/Theme-Klasse änderbar; dunkles/helles Theme via Qt-Palette. | K |
| NFA-08 | **Portabilität**: Linux, Windows, macOS; Compiler GCC, Clang, MSVC. | S |
| NFA-09 | **Lizenz**: MIT (wie InMemDB). | K |

---

## 6. Schnittstellen (öffentliche API der Bibliothek)

Namespace `memdb::viewer`. Wesentliche Klassen (Namensvorschläge, finale Namen bei Umsetzung festzulegen):

| Klasse | Schicht | Verantwortung |
|---|---|---|
| `memdb::viewer::ViewerController` | Controller | Einstiegspunkt der Bibliothek: hält `memdb::Database`, verdrahtet Model ↔ Views, Observer-Bridge (`onChange` → queued Signale), Query-Ausführung, Selektion. |
| `memdb::viewer::DatabaseModel` | Model | `QAbstractItemModel` über `Database`: Top-Level = Collections (Name, Dok-Anzahl, hasSchema, Indexanzahl); Kind-Knoten = Schema-Felder/Indizes. Signalisiert Änderungen via `dataChanged`. |
| `memdb::viewer::SchemaScene` / `CollectionItem` | View | `QGraphicsScene` mit `QGraphicsItem`-Knoten pro Collection (Schema-Liste im Knoten, Index-Markierungen) und Kanten für FA-18-Referenzen; Quelle: `DbGraph::fromDatabase` + Live-Updates. |
| `memdb::viewer::SchemaView` | View | `QGraphicsView` zur Anzeige der `SchemaScene` (Pan/Zoom, Selektion). |
| `memdb::viewer::DocumentView` | View | `QTableView` (Basis `memdb::qt::MemDbTableModel`) + `QTreeView` für Dokument-Details. |
| `memdb::viewer::MetricsPanel` | View | Widget zur Anzeige von `CollectionMetrics`/`DatabaseMetrics`. |

Verwendete InMemDB-Schnittstellen: `Database` (collectionNames, metrics, loadFromFile), `Collection` (schema, indexes, size, find/findIds, metrics, overflowInfo, onChange), `Schema`/`FieldSchema`, `DbGraph` (FA-22), `ReferenceIndex` (FA-18), `memdb::qt::MemDbTableModel` (v0.23, nur QtCore).

---

## 7. Qualitätsanforderungen

- Unit-Tests (QtTest) für: Model-Aufbau aus Database, Refresh nach simuliertem Observer-Event, Query-Filter, Szenen-Aufbau (Knoten/Kanten-Anzahl == `DbGraph`), Selektion im Controller.
- Smoke-Test: Demo-App startet headless (`QT_QPA_PLATFORM=offscreen`), rendert die Szene in ein Offscreen-Bild und beendet sich mit Exit-Code 0.
- CI: Build + Tests für qmake und CMake, mindestens Qt 5.15 und Qt 6 (Matrix), Plattformen Linux/Windows/macOS.
- Code-Review-Pflicht für Änderungen an der öffentlichen API (`include/memdbviewer/`).

---

## 8. Randbedingungen

- InMemDB bleibt ohne harte Qt-Abhängigkeit; der Viewer ist der einzige Qt-Konsument.
- v1 ist grundsätzlich **lesend**; Schreibzugriffe (FA-14) sind Kann-Feature und dürfen das MVC-Gefüge nicht aufweichen.
- Keine neuen externen Bibliotheken (kein QCustomPlot o. Ä.); Diagramme ausschließlich mit Qt-eigenen Mitteln (Graphics View, Widgets, `QPainter`).

---

## 9. Risiken

| Risiko | Gegenmaßnahme |
|---|---|
| Sehr große Datenbanken überlasten die `QGraphicsScene` (zu viele Items). | Level-of-Detail: Dokumente nie als Items in der Schema-Szene; Dokumenten-Views virtualisiert; ggf. Szenen-Filter (nur sichtbare Collections rendern). Siehe NFA-03. |
| Qt 5 vs. Qt 6 API-Unterschiede (z. B. `QRegExp`, `QGraphicsItem` comportamiento). | Kompatibilitäts-Makros/Version-Checks (`QT_VERSION`), CI-Matrix über beide Qt-Versionen. |
| Observer-Events aus Nicht-GUI-Threads. | Queued Connections zwingend (NFA-04), Test mit künstlichem Thread. |
| Schema-lose Collections haben keine Metadaten zum Rendern. | Fallback: Felder aus der Vereinigung der Dokumente ableiten (wie `MemDbTableModel`-Spalten) oder als „schema-los" kennzeichnen. |

---

## 10. Ausblick (nicht in v1)

- Transaktions-Verlauf als Timeline (Tx-Commits/Rollbacks aus Metriken).
- Import-Assistent (InMemDB-FA-21-Quellen) mit Vorschau.
- Editierbare Schema-Ansicht (Schema ändern → `Collection::setSchema`).
- Plugin-Schnittstelle für eigene View-Komponenten (über Controller-Registrierung).
- Force-Directed-Layout der Graph-Szene (statt festem Raster) für große Graphen.

---

## 11. Glossar

| Begriff | Bedeutung |
|---|---|
| **MVC** | Model–View–Controller: Model = Daten/Qt-Models, View = Darstellung, Controller = Vermittlung. |
| **QGraphicsFramework** | Qt Graphics View Framework: `QGraphicsScene` + `QGraphicsView` + `QGraphicsItem` zum zoomen/verschieben von 2D-Elementen. |
| **Collection** | Benannte Dokumentmenge in InMemDB (Analogie zu Tabelle, schema-los). |
| **Schema (FA-20)** | Optionales striktes Schema pro Collection: deklarierte Felder mit `Value::Type`, `nullable`, Unter-Schema, Array-Elementtyp. |
| **Index (FA-10)** | Sekundärindex auf einem Feldpfad, optional `unique`. |
| **Referenz (FA-18)** | Gerichtete, typisierte Beziehung zwischen Collections/Dokumenten (`ReferenceIndex`). |
| **DbGraph (FA-22)** | Textbasierte Graph-Darstellung (JSON/Mermaid/Markdown) der Database – Datenquelle für die Szenen-Knoten/Kanten. |
| **Observer (FA-15)** | `Collection::onChange`: Callback bei Änderungen, feuert nach Transaktions-Commit. |
| **Pimpl** | Pointer-to-implementation: öffentliche Header ohne Implementierungsdetails (ABI-stabil). |
