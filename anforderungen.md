# Anforderungen

## InMemDBViewer – Visualisierungs- und Analysetool für InMemDB (C++17/Qt, MVC, Bibliothek + App)

| Feld | Wert |
|---|---|
| Projekttitel | InMemDBViewer – Daten laden, betrachten und analysieren für [InMemDB](https://github.com/FuhMhanShu/InMemDB) |
| Version | 1.1 |
| Status | v1.0 umgesetzt (4 Branches nach Abschnitt 8 gemerged: Anforderungen, Dateneingabe, Datenbetrachtung, Datenanalyse; die bisherige Demo-Implementierung v0.1–v0.4 wurde verworfen, siehe Abschnitt 3). v1.1 „Design & Bearbeiten“ (Abschnitt 4.5): Schritte A–C umgesetzt (verifizierte Mutationen + Ausgabe-Panel + DatabaseDesignWidget + Clipboard-CSV + Dokument-CRUD), Schritt D (Auto-Discovery/Analyse nach jeder Mutation) folgt. |
| Sprache | Deutsch (UI und Doku) |
| Technologie | C++17, Qt (Widgets + Graphics View Framework), qmake (.pro/.pri) **und** CMake |
| Bereitstellungsform | Wiederverwendbare **C++/Qt-Bibliothek** (statische Library, öffentliche Header unter `include/memdbviewer/`, Implementierung in `src/`, Namespace `memdb::viewer`, Einbindung via `InMemDBViewer.pri` bzw. CMake-Target) **plus** lauffähige Anwendung unter `examples/viewer_app/` |
| Abhängigkeiten | Qt (core, gui, widgets, svg, test) + InMemDB (statische Lib oder Quellen via `InMemDB.pri`). Optionaler SQLite-Import über InMemDB-Build-Option `INMEMDB_WITH_SQLITE=ON`. Keine weiteren externen Pflichtabhängigkeiten. |

---

## 1. Zielbestimmung

### 1.1 Produktzweck
InMemDBViewer ist ein **Qt-Desktop-Tool**, mit dem Daten in eine laufende `memdb::Database` **geladen** (CSV, JSON/JSONL, SQL-Dump, BSON, SQLite, InMemDB-Snapshot inkl. verschlüsselt), dort **betrachtet** (Struktur-Graph, Tabellen, Dokumente) und **analysiert** (Discovery + statistische Analyse) werden. Die Architektur folgt strikt **MVC**; die GUI besteht aus drei **gekapselten Widgets**, die über ein gemeinsames Core-Objekt (`ViewerCore`) auf dieselbe Database zugreifen.

### 1.2 Nutzen
- Schneller Überblick über geladene Daten: Welche Collections gibt es, wie sind sie strukturiert (Schema, Indizes, Referenzen)?
- Direkter Blick in die Daten: tabellarische Ansicht mit Query-Filter, Detailbaum pro Dokument, Live-Aktualisierung bei Änderungen.
- Datenanalyse ohne Code: Relationship-Discovery (Fremdschlüssel, funktionale Abhängigkeiten, Korrelationen, Dubletten), Datenprofile, Ausreißer, numerische Statistik, Einfluss-Analyse, Health-Score – alles auf Basis der InMemDB-Analyse-APIs (FA-27, FA-29…FA-32, v0.33/v0.40).
- Wiederverwendbar: die drei Widgets sind als Bibliothek in eigene Qt-Anwendungen einbettbar.

### 1.3 Abgrenzung (Nicht-Ziele)
- Kein Ersatz für die InMemDB-API; das Tool liest und (optional) schreibt über die öffentliche InMemDB-API.
- Kein Server, kein Netzwerk, keine Remote-Datenbank.
- Die geladenen Daten leben nur im RAM der App; Persistenz erfolgt ausschließlich über InMemDB-Snapshots (`Database::saveToFile` / `loadFromFile`, FA-14/FA-19).
- InMemDB selbst bleibt Qt-frei (harte Qt-Abhängigkeit nur im Viewer).

---

## 2. Produktumfeld / Systemkontext

- **Einsatz**: Desktop-Anwendung (Linux, Windows, macOS) sowie als Bibliothek in eigenen Qt-Apps; Demo-App `examples/viewer_app`.
- **Qt-Versionen**: Qt 5.15+ und Qt 6.x, Build über qmake und CMake.
- **Datenquellen**: externe Dateien (Abschnitt 4.1) sowie InMemDB-Snapshots; Ergebnis ist immer eine `memdb::Database`, die `ViewerCore` hält.
- **Threading**: GUI-Thread only für Widgets; InMemDB-Observer-Events und lange Analysen laufen über queued Signale bzw. einen Worker-Thread (Abschnitt 5).
- **Headless/CI**: alle Widgets müssen mit `QT_QPA_PLATFORM=offscreen` testbar sein; Demo-App beendet sich headless selbst (Smoke-Test).

---

## 3. Gesamtarchitektur (MVC, drei gekapselte Widgets + Core)

Die bisherige Demo-Implementierung (v0.1–v0.4: `SchemaScene`/`SchemaView`/`DocumentView`/… als lose Bibliothek) wird **verworfen**. Neu aufgebaut wird:

```
┌──────────────────────── MainWindow (examples/viewer_app) ─────────────────────────┐
│  Oben (übergreifend): Daten laden – Menü/Toolbar ("Daten öffnen…", "Snapshot öffnen…",│
│  "Neu"), Drag & Drop anywhere auf das Fenster, Statusleiste                        │
│  ┌────────────────────────────┐  ┌────────────────────────────────────────────┐   │
│  │ Bereich 1: Datenbetrachtung│  │ Bereich 2: Datenanalyse (Discovery + Analyse)  │   │
│  │  DataViewWidget (MVC)      │  │  AnalysisWidget (MVC)                          │   │
│  └────────────────────────────┘  └────────────────────────────────────────────┘   │
│  Panel: DataInputWidget (MVC) – Dateneingabe: Dialog, Drag & Drop,                   │
│  unterstützte Formate, Import-Optionen, Verlauf/Status                              │
└───────────────────────────────────────────────────────────────────────────────────┘
                              │ alle drei hängen am selben ViewerCore
              ┌───────────────┴───────────────┐
              ▼                               ▼
        ViewerCore (Model, QObject): hält memdb::Database + ReferenceIndex,
        ImportRegistry/Importer (FA-21), Snapshot-Loading (FA-14/FA-19),
        Observer-Bridge (FA-15 → queued Qt-Signale), Auto-Sync der Collection-Liste.
        Signale: databaseLoaded(), collectionChanged(name), databaseCleared().
```

| Schicht | Verantwortung | Konkrete Ausprägung |
|---|---|---|
| **Model** | Kapselt `memdb::Database`/`Collection` und Analyse-Ergebnisse als Qt-Models; keine UI. | `ViewerCore` (zentraler Datenhalter), `ImportModel` (verfügbare Import-Quellen + Verlauf), `DatabaseModel` (Collections + Metadaten), `DocumentTableModel` (virtualisierte Zeilen), `DocumentTreeModel` (verschachteltes Dokument), `GraphSceneModel` (DbGraph FA-22 → Szenen-Struktur), `DiscoveryModel`, `AnalysisModel` (Profile/Statistik/Einfluss/Health) |
| **View** | Reine Darstellung, kennt Models nur über Qt-Model-Interfaces/Signale. | `DataInputWidget`, `DataViewWidget` (Struktur: `QGraphicsView`+`QGraphicsScene`; Tabelle: `QTableView`; Detail: `QTreeView`), `AnalysisWidget` (Tabellen/Trees/Health-Ampel/Report) |
| **Controller** | Vermittelt View ↔ Model, enthält keine UI. | `DataInputController` (Datei-Dialog, Drag&Drop, Import-Optionen → `Importer`), `DataViewController` (Selektion, Query-Filter, Refresh), `AnalysisController` (Discovery/Analyse auslösen, Worker-Thread, Ergebnisse ins Model) |

Regeln: Views kennen keine `memdb::`-Typen direkt; Controller kennen keine konkreten Widgets; Models emittieren nur Qt-Signale; jedes Widget ist **vollständig gekapselt** (ein öffentlicher Header `…_widget.hpp` + interne Model/Controller-Klassen, benutzbar als `widget->setCore(core)`).

---

## 4. Funktionale Anforderungen

Prioritäten: **M** = Muss, **S** = Soll, **K** = Kann. InMemDB-Referenzen in Klammern.

### 4.1 Übergreifend: Daten laden (DL)

| ID | Anforderung | Prio |
|---|---|---|
| DL-01 | **DataInputWidget (MVC, gekapselt)**: Dateneingabe per Datei-Dialog (multi-select), Drag & Drop (einzeln + mehrere Dateien + Ordner) und über die App-Toolbar; Drag & Drop auf dem Hauptfenster landet im selben Widget. | M |
| DL-02 | **Formate über `ImportRegistry` (FA-21)**: Auto-Erkennung per Dateiendung – `csv`, `json`/`jsonl`/`ndjson`, `sqldump` (`sql`/`dump`), `bson`, InMemDB-Snapshot (`.inmemdb.json` via `InmemdbSnapshotSource`), optional `sqlite` (`db`/`sqlite`/`sqlite3`, nur wenn InMemDB mit `INMEMDB_WITH_SQLITE` gebaut; sonst klarer Hinweis). Liste der unterstützten Quellen/Endungen im Widget anzeigen. | M |
| DL-03 | **Snapshot-Laden (FA-14/FA-19)**: JSON-Snapshots und verschlüsselte Snapshots (Magic-Header `INMEMDB\x01`) erkennen; bei verschlüsselt Passwort-Dialog; Laden ersetzt die aktuelle Database im `ViewerCore` und stößt `databaseLoaded()` an. | M |
| DL-04 | **Import-Optionen (FA-21 `ImportOptions`)**: im Widget einstellbar – Batch-Größe, Indizes anlegen, Schema generieren, PK-Feld, Fremdschlüssel als FA-18-Referenzen (FA-26). | S |
| DL-05 | **Ergebnis/Status**: ImportStats (Zeilen pro Collection, Gesamt, übersprungene, importierte Referenzen) und Fehler (mit Fehlermeldung) im Widget/Statusleiste anzeigen; Verlauf der zuletzt geladenen Dateien (Liste klickbar zum erneuten Laden). | S |
| DL-06 | **Fortschritt** bei großen Dateien: Statusanzeige, UI bleibt bedienbar (Import im Worker-Thread mit queued Lebenszeichen oder Batches). | S |
| DL-07 | **"Neue/leere Database"**: Aktion, die `ViewerCore` auf eine frische, leere Database setzt (alle Bereiche aktualisieren sich). | M |
| DL-08 | **Demo-Daten**: Aktion "Beispiel laden", die eine kleine Demo-Database (2–3 Collections, Schema, Index, Referenz) direkt per Code befüllt – für Smoke-Test und erstes Ausprobieren. | S |

### 4.2 Bereich Datenbetrachtung (DB)

| ID | Anforderung | Prio |
|---|---|---|
| DB-01 | **DataViewWidget (MVC, gekapselt)**: Bereich "Datenbetrachtung" mit Datenbank-Übersicht: alle Collections (`Database::collectionNames()`) mit Dokumentanzahl (`size()`), Schema-Status (`hasSchema()`), Indexanzahl (`indexes().size()`). | M |
| DB-02 | **Strukturansicht mit QGraphicsFramework (FA-22 `DbGraph`)**: Collections als Knoten (`QGraphicsItem`: Titel, Dok-Anzahl, Schema-Felder mit Typ/`index`/`unique`, Index-Markierung), FA-18-Referenzen als gerichtete Kanten inkl. Relationsname und Dangling-Markierung. Pan/Zoom, Selektion per Klick, Doppelklick klappt Schema-Details auf/zu. | M |
| DB-03 | **Tabellarische Datenansicht** pro Collection: `QTableView` + virtualisiertes Model (Zeilen = Dokumente via `findIds`, Zellen on demand via `findById` – keine Voll-Kopien, NFA-03), Spalten = Feldpfade (Vereinigung, inkl. Punktnotation für verschachtelte Felder). | M |
| DB-04 | **Query-Filter (String-DSL)**: Eingabezeile, `Collection::find(dsl)`; Ergebnis in Tabelle/Szene markiert; Diagnose: Index genutzt? (`lastQueryUsedIndex`, `lastQueryCandidateCount` vs. `size()`); DSL-Fehler anzeigen. | S |
| DB-05 | **Dokument-Detail**: Baumansicht (`QTreeView` + rekursives Model) des selektierten Dokuments – Felder mit Wert + `Value::Type`, verschachtelte Dokumente/Arrays rekursiv, Typ-Farben. | M |
| DB-06 | **Live-Aktualisierung (FA-15)**: Änderungen (insert/update/remove/clear, auch nach Tx-Commit) über `Collection::onChange` per queued Qt-Signal → Model/Szene aktualisieren, ohne manuelles Reload; neue/gelöschte Collections per leichtem Auto-Sync (Timer-Vergleich `collectionNames()`). | M |
| DB-07 | **Metadaten pro Collection**: Metriken (`Collection::metrics()` v0.22: inserts/updates/removes/queries/Index-Nutzung/Tx/Spill-IO) und Hybrid/Overflow-Info (`overflowInfo()` v0.18) im Detailbereich. | S |
| DB-08 | **Aggregationen (FA-13)**: pro Collection `aggregate`/`groupBy` (count/min/max/sum/avg/distinct) mit optionalem DSL-Filter als kleine Übersicht über der Tabelle. | K |
| DB-09 | **Export**: Struktur-Szene als PNG/SVG (`QGraphicsScene::render`), Graph als Mermaid/JSON/Markdown (`DbGraph::toMermaid/toJson/toMarkdown`). | K |
| DB-10 | **Selektion koppeln**: Klick in Übersicht/Struktur/Tabelle wählt dieselbe Collection/dasselbe Dokument in allen Ansichten (bidirektional). | S |

### 4.3 Bereich Datenanalyse – Discovery + Analyse (AN)

| ID | Anforderung | Prio |
|---|---|---|
| AN-01 | **AnalysisWidget (MVC, gekapselt)**: Bereich "Analyse" mit zwei Sichten: **Discovery** und **Analyse**, plus Health-Übersicht und Report-Export. Ausführung per Button "Analysieren" (immer ganze Database). | M |
| AN-02 | **Discovery (FA-27)**: entdeckte Relationen als Tabelle (Art: FK/FD/Korrelation/Near-Duplicate, von Collection.Feld → Ziel, Support, Confidence, Kardinalität, Sample-Größe); Orphan-Reports (`findOrphansForRelations`), Multi-Hop-Pfade (`findPaths`), Merge-Vorschläge (`suggestMerges` v0.32), Datenprofile (`profileCollection` v0.31), Candidate Keys (`discoverCandidateKeys`) und **Confusion-Matrix (v0.42: entdeckte FKs vs. deklarierte `__references` als 2x2-Raster mit TP/FP/FN/TN + Precision/Recall/F1, Schwelle `confusionMinConfidence`)**. Option: Befunde mit Confidence ≥ Schwelle als FA-18-Referenzen anwenden (`applyDiscovered`). | M |
| AN-03 | **Analyse**: Datencharakter pro Collection (`assessCharacter`, FA-29: Numeric/Mixed/Complex + Begründung), numerische Feld-Analyse (`analyzeNumeric`, FA-30: Min/Max/Mittel/Std/Quantile/Schiefe, Histogramm, Sequenz-/Raster-/Benford-Erkennung, Pearson-Korrelationen), Einfluss-Analyse (`analyzeInfluence` FA-31 + `analyzeCrossInfluence` FA-32: Rollen driver/dependent/mediator, influenceScore), Ausreißer (`discoverOutliers` v0.40: z-Score/IQR). | M |
| AN-04 | **Gesamt-Report (FA-27 v0.33 `analyze()`)**: Health-Score (0–100, Ampel ≥80 grün/≥50 gelb) prominent; Export des Reports als Markdown/JSON/Mermaid (`toMarkdown`/`toJson`/`toMermaid`) per Datei-Dialog. | S |
| AN-05 | **Optionen-Dialog**: Schwellen/Sampling der `DiscoveryOptions`/`NumericAnalysisOptions` (minSupport, minCorrelation, sampleSize, seed …) einstellbar. | S |
| AN-06 | **Ausführung im Worker-Thread**: Analyse läuft in einem QThread, Ergebnisse per queued Signalen ins Model (UI bleibt responsiv, NFA-04); Busy-Indikator + Abbrechen. | M |
| AN-07 | **Ergebnis-Darstellung**: alle Befunde als Qt-Models (Tabellen/Trees) mit Sortierung; numerische Histogramme als Balkendiagramm (QPainter, Qt-eigene Mittel); lange Texte (Notes/Begründungen) in Detail-Panel. | S |
| AN-08 | **Analyse auf Auswahl**: optional nur die aktuell selektierte Collection analysieren (statt ganzer Database). | K |

### 4.4 App-Shell und Demo (GA)

| ID | Anforderung | Prio |
|---|---|---|
| GA-01 | **MainWindow** (`examples/viewer_app`): oben übergeordnet Daten laden (Menü + Toolbar + Drag & Drop), darunter zwei klar getrennte Bereiche (z. B. QSplitter oder Tabs: "Datenbetrachtung" / "Analyse") plus einklappbares Dateneingabe-Panel; Statusleiste mit letzter Aktion/Fehler. | M |
| GA-02 | **Ergonomie**: konsistente deutsche Beschriftungen, gruppierte Werkzeugleisten mit Tooltips, Tastenkürzel (Öffnen Strg+O, Analysieren F5 …), sinnvolle Größenverhältnisse/Splitter, keine modal blockierenden Dialoge außer Passwort/Datei-Öffnen. | M |
| GA-03 | **Demo-App headless-tauglich**: `QT_QPA_PLATFORM=offscreen ./viewer_app` lädt Demo-Daten, rendert die Struktur-Szene in ein Offscreen-Bild, führt eine Analyse aus und beendet sich mit Exit-Code 0 (CI-Smoke-Test). | M |

### 4.5 Design & Bearbeiten der Database (DE)

| ID | Anforderung | Prio |
|---|---|---|
| DE-01 | **Verifizierte Mutationen im `ViewerCore`**: Collection anlegen/löschen/leeren, Schema setzen/entfernen (FA-20 – Bestand wird mitgeprüft), Index anlegen/löschen (unique inkl. Duplikat-Prüfung, FA-10), Referenzen verknüpfen/trennen + Dangling-Prune (FA-18), Dokument einfügen (JSON)/Feld ändern (Punktnotation, typisierter Wert)/löschen, CSV-Text importieren (FA-21), Snapshot speichern (FA-14 / verschlüsselt FA-19). Jede Operation liefert `bool` + Fehlertext, fängt Exceptions ab, schreibt ins Log und stößt bei Erfolg `dataMutated()` an. | M |
| DE-02 | **Ausgabe-Panel** (`OutputPanel`, Dock unten in der App): Log mit Zeitstempel, Fehler rot hervorgehoben, Filter „nur Fehler“, Zähler, „Leeren“; gekoppelt an `ViewerCore::logged`. | M |
| DE-03 | **DatabaseDesignWidget (MVC, gekapselt)**: neue Database anlegen (App-Menü „Neue Datenbank…“) oder geladene bearbeiten – Collections anlegen/löschen/leeren; **Schema-Editor** (Feldpfad/Typ/nullable/Unter-Schema/Array-Elementtyp, „Schema anwenden (validiert)“); **Indizes** (Feld mit Auto-Vervollständigung, unique-Checkbox, anlegen/löschen); **Referenzen** (Von/Nach + IDs + Relation, Verknüpfen/Entfernen/Dangling bereinigen). Alle Operationen laufen über DE-01 und werden verifiziert/geloggt. | M |
| DE-04 | **CSV aus der Zwischenablage** im `DataInputWidget`: Button „CSV aus Zwischenablage…“ + Optionen-Dialog (Collection-Name, Trennzeichen, Quote, Header, Typ-Inferenz, Vorschau) → `ViewerCore::importCsvText`. | M |
| DE-05 | **Dokument-CRUD in der Datenbetrachtung**: Zellen der Tabelle editierbar (Doppelklick; id-Spalte geschützt) → `updateDocumentField` (verifiziert: Schema/Unique werden geprüft, Fehler ins Log); „Dokument neu…“ (JSON-Dialog) und „Dokument löschen“ (Zeilenauswahl + Sicherheitsabfrage). | M |
| DE-06 | **Auto-Aktualisierung nach jeder Datenmanipulation**: `ViewerCore::dataMutated()` triggert Datenbetrachtung (Graph/Models) und – mit Debounce – **Discovery + Analyse** (AN-01) automatisch; abschaltbar („Auto-Analyse nach Änderung“). | M |
| DE-07 | **App-Menü**: „Neue Datenbank…“ (leere Database mit Name), „Snapshot speichern…“, „Snapshot verschlüsselt speichern…“. | S |

---

## 5. Nicht-funktionale Anforderungen

| ID | Anforderung | Prio |
|---|---|---|
| NFA-01 | **Qt-Kompatibilität**: Qt 5.15+ und Qt 6.x; Module `core gui widgets svg test`; C++17; Qt5/Qt6-Unterschiede per `QT_VERSION_CHECK`. | M |
| NFA-02 | **Build**: qmake (`InMemDBViewer.pro` + `InMemDBViewer.pri`, Shadow-Build-kompatibel) **und** CMake ≥ 3.20 (analog InMemDB; InMemDB als Unterprojekt via `INMEMDB_DIR`). Keine externen Pflichtabhängigkeiten außer Qt + InMemDB. | M |
| NFA-03 | **Performance/Usability**: Tabellen virtualisiert (nur IDs, Details on demand); Struktur-Szene nur Collections als Knoten (nie Dokumente als Items); Analyse im Worker-Thread; große Importe mit Fortschritt. | S |
| NFA-04 | **Thread-Sicherheit**: GUI-Zugriffe nur im GUI-Thread; InMemDB-Observer-Callbacks und Worker-Ergebnisse per `Qt::QueuedConnection`/`QMetaObject::invokeMethod` in den GUI-Thread heben. | M |
| NFA-05 | **Testbarkeit**: QtTest-Unit-Tests pro Widget (Model/Controller: Import-Pfade, Model-Aufbau, Refresh nach Observer-Event, Szenen-Aufbau == DbGraph, Analyse-Models nach Lauf); headless via `QT_QPA_PLATFORM=offscreen`; CI-Smoke-Test der App. | S |
| NFA-06 | **Dokumentation**: Doxygen-kommentierte öffentliche API (`include/memdbviewer/`), README mit Quickstart (Einbindung via .pri und CMake, Mini-Beispiel je Widget). | S |
| NFA-07 | **Anpassbarkeit**: Farben/Typ-Markierungen über zentrale Konstanten/Theme-Klasse änderbar. | K |
| NFA-08 | **Portabilität**: Linux, Windows, macOS; GCC, Clang, MSVC. | S |
| NFA-09 | **Lizenz**: MIT (wie InMemDB). | K |

---

## 6. Schnittstellen (öffentliche API der Bibliothek)

Namespace `memdb::viewer`. Jedes Widget ist eine Einheit aus öffentlichem Widget-Header und internen (in `src/` gekapselten) Model/Controller-Klassen.

| Klasse | Schicht | Verantwortung |
|---|---|---|
| `memdb::viewer::ViewerCore` | Model (zentral) | Hält `memdb::Database` + `ReferenceIndex`; Import (FA-21) & Snapshot (FA-14/FA-19) in die Database; Observer-Bridge (FA-15 → queued Signale); Auto-Sync der Collection-Liste; Signale `databaseLoaded(QString)`, `collectionChanged(QString)`, `databaseCleared()`, `lastErrorChanged(QString)`. |
| `memdb::viewer::DataInputWidget` | View | Dateneingabe-Panel (Dialog, Drag & Drop, CSV aus Zwischenablage DE-04, unterstützte Formate, Optionen, Verlauf, Status). Intern: `ImportModel`, `DataInputController`. |
| `memdb::viewer::DataViewWidget` | View | Bereich Datenbetrachtung: Übersicht (`DatabaseModel`), Struktur (`GraphScene`/`GraphView`, QGraphicsFramework + DbGraph FA-22), Tabelle (`DocumentTableModel`, Zellen editierbar DE-05), Dokument-Detail (`DocumentTreeModel`), Query-Filter, Metriken, Dokument-CRUD. Intern: `DataViewController`. |
| `memdb::viewer::DatabaseDesignWidget` | View | Bereich Design/Bearbeiten (DE-03): Collections, Schema-Editor, Indizes, Referenzen – alles verifiziert über `ViewerCore` (DE-01). Intern: `DesignController`. |
| `memdb::viewer::OutputPanel` | View | Ausgabe-Log (DE-02): Zeitstempel, Fehler rot, Filter „nur Fehler“, Zähler. |
| `memdb::viewer::AnalysisWidget` | View | Bereich Analyse: Tabs Discovery (`DiscoveryModel`: Relationen/Orphans/Pfade/Merges/Keys) und Analyse (`AnalysisModel`: Charakter FA-29, Numeric FA-30, Einfluss FA-31/32, Outliers v0.40, Health FA-27 v0.33), Report-Export. Intern: `AnalysisController` (Worker-Thread). |

Verwendete InMemDB-Schnittstellen: `Database`/`Collection` (CRUD, FA-10 Indizes, FA-13 Aggregation, FA-15 Observer, FA-18 Referenzen, FA-20 Schema, FA-22 DbGraph, v0.22 Metriken, v0.18 Overflow, DSL-Queries), `ImportRegistry`/`Importer`/`ImportSource` (FA-21/FA-26), Snapshots (FA-14/FA-19), Discovery & Analyse (FA-27, FA-29–FA-32, v0.33/v0.40: `discoverRelations`, `profileCollection`, `discoverCandidateKeys`, `findOrphansForRelations`, `findPaths`, `suggestMerges`, `discoverOutliers`, `assessCharacter`, `analyzeNumeric`, `analyzeInfluence`, `analyzeCrossInfluence`, `analyze`).

---

## 7. Qualitätsanforderungen

- Unit-Tests (QtTest) pro Schritt: Import-Pfade (CSV/JSON/SQLite/Snapshot/verschlüsselt, Fehlerfälle), Model-Aufbau und Refresh nach Observer-Event, Szenen-Aufbau (Knoten/Kanten == `DbGraph`), Selektion, Analyse-Models nach Worker-Lauf, Health-Score in 0..100.
- Smoke-Test: App startet headless, lädt Demo-Daten, rendert Szene, läuft Analyse, Exit-Code 0.
- CI: CMake-Build + QtTest + App-Smoke für Qt 6 und Qt 5.15 sowie qmake-Build (analog bisheriger CI); InMemDB wird als Schwestern-Checkout mitgebaut.
- Jeder Entwicklungsschritt (Abschnitt 8) auf eigenem Branch; Merge nach `main` erst wenn alle Tests grün.

---

## 8. Umsetzungs-Schritte (ein Branch pro Schritt)

1. **Anforderungen** (dieses Dokument, v1.0).
2. **Dateneingabe**: alte Demo-Implementierung entfernen; `ViewerCore` + `DataInputWidget` (MVC) + App-Shell mit Dateneingabe; Tests.
3. **Datenbetrachtung**: `DataViewWidget` (MVC) – Übersicht, Struktur (QGraphicsFramework/DbGraph), Tabelle, Dokument-Detail; Tests.
4. **Datenanalyse**: `AnalysisWidget` (MVC) – Discovery + Analyse (Worker-Thread, Health, Report); Tests.
5. **Design & Bearbeiten (v1.1, Abschnitt 4.5)**, je ein Branch:
   - **A**: verifizierte Mutationen im `ViewerCore` (DE-01) + Ausgabe-Panel (DE-02) + App-Menü (DE-07); Tests.
   - **B**: `DatabaseDesignWidget` (DE-03) – Collections/Schema/Indizes/Referenzen; Tests.
   - **C**: CSV aus der Zwischenablage (DE-04) + Dokument-CRUD in der Datenbetrachtung (DE-05); Tests.
   - **D**: Auto-Discovery + Auto-Analyse nach jeder Mutation via `dataMutated()` (DE-06); Tests.

---

## 9. Randbedingungen

- InMemDB bleibt ohne harte Qt-Abhängigkeit; der Viewer ist der einzige Qt-Konsument.
- Keine neuen externen Bibliotheken (kein QCustomPlot o. Ä.); Diagramme nur mit Qt-Mitteln (Graphics View, Widgets, `QPainter`).
- SQLite-Import nur, wenn InMemDB mit `INMEMDB_WITH_SQLITE=ON` gebaut wurde (`INMEMDB_HAS_SQLITE`); sonst im UI als nicht verfügbar kennzeichnen.

---

## 10. Risiken

| Risiko | Gegenmaßnahme |
|---|---|
| Sehr große Datenbanken überlasten Szene/Tabelle oder blockieren die UI. | Virtualisierte Models (nur IDs, Details on demand); Analyse/Import im Worker-Thread; Struktur-Szene nur Collection-Knoten; Level-of-Detail. |
| Lange Analysen frieren die GUI ein. | `AnalysisController` mit QThread + queued Signalen + Abbrechen (NFA-04, AN-06). |
| Observer-Events aus Nicht-GUI-Threads. | Queued Connections zwingend, Test mit künstlichem Thread. |
| SQLite nicht verfügbar (Build ohne `INMEMDB_WITH_SQLITE`). | Endung `.db/.sqlite` wird als "nicht verfügbar" gemeldet, Rest funktioniert. |
| Qt 5 vs. Qt 6 API-Unterschiede. | `QT_VERSION_CHECK`, CI-Matrix über beide Versionen. |
| Schema-lose Collections haben keine Metadaten. | Fallback: Felder aus der Vereinigung der Dokumente ableiten oder als „schema-los" kennzeichnen. |

---

## 11. Ausblick (nicht in v1.0)

- CRUD-Bearbeitung direkt in der Tabelle (schreibend über die InMemDB-API).
- Transaktions-Verlauf als Timeline (Tx-Commits/Rollbacks aus Metriken).
- Editierbare Schema-Ansicht (`Collection::setSchema`).
- Force-Directed-Layout für große Graphen; Szenen-Filter.
- Plugin-Schnittstelle für eigene Analyse-Pipelines.

---

## 12. Glossar

| Begriff | Bedeutung |
|---|---|
| **MVC** | Model–View–Controller: Model = Daten/Qt-Models, View = Darstellung, Controller = Vermittlung. |
| **ViewerCore** | Gemeinsames Model-Objekt: hält `memdb::Database` + `ReferenceIndex`, lädt Daten, verteilt Signale an alle Widgets. |
| **QGraphicsFramework** | Qt Graphics View Framework (`QGraphicsScene`/`QGraphicsView`/`QGraphicsItem`) für zoom-/verschiebbares 2D-Diagramm. |
| **Collection** | Benannte Dokumentmenge in InMemDB (Analogie zu Tabelle, schema-los). |
| **ImportRegistry/Importer (FA-21)** | Zentrale Factory für externe Datenquellen (CSV/JSON/SQL-Dump/BSON/SQLite/Snapshot) + generischer Import in Collections. |
| **DbGraph (FA-22)** | Graph-Abbildung der Database: Collections als Knoten, FA-18-Referenzen als Kanten. |
| **Discovery (FA-27)** | Automatisches Finden von Beziehungen/Abhängigkeiten: FK, funktionale Abhängigkeiten, Korrelationen, Near-Duplicates, Orphans, Pfade, Profile, Candidate Keys, Merge-Vorschläge. |
| **Analyse (FA-29…FA-32, v0.33/v0.40)** | Datencharakter, numerische Statistik, Einfluss-Analyse (intra-/cross-Collection), Ausreißer, Health-Score (`analyze()`). |
| **Observer (FA-15)** | `Collection::onChange`: Callback bei Änderungen, feuert nach Transaktions-Commit. |
| **Snapshot (FA-14/FA-19)** | JSON- oder verschlüsseltes Binär-Format der gesamten Database. |
| **Pimpl** | Pointer-to-implementation: öffentliche Header ohne Implementierungsdetails (ABI-stabil). |
