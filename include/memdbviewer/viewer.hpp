#pragma once

/// @file
/// @brief Umbrella-Header der InMemDBViewer-Bibliothek (Namespace `memdb::viewer`).
///        Zentrale Komponenten: `ViewerCore` (Model) + die gekapselten MVC-Widgets
///        `DataInputWidget` (Dateneingabe), `DataViewWidget` (Datenbetrachtung)
///        und `AnalysisWidget` (Datenanalyse).

#include "viewer_core.hpp"
#include "data_input_widget.hpp"
#include "data_view_widget.hpp"
// AnalysisWidget (Analyse) folgt im eigenen Umsetzungs-Branch und wird hier ergaenzt.
