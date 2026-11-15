#pragma once

#include <QGraphicsView>

namespace memdb::viewer {

class SchemaScene;

/// @brief View (MVC): QGraphicsView fuer die SchemaScene – Pan/Zoom per Maus (FA-09).
class SchemaView : public QGraphicsView {
    Q_OBJECT
public:
    explicit SchemaView(QWidget* parent = nullptr);

    /// @brief Szene setzen und Inhalt komplett einpassen.
    void setSchemaScene(SchemaScene* scene);
    /// @brief Gesamten Szeneninhalt in den View einpassen.
    void fitContent();
};

} // namespace memdb::viewer
