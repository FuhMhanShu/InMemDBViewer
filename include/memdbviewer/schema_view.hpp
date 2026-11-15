#pragma once

#include <QGraphicsView>
#include <QString>

class QMouseEvent;

namespace memdb::viewer {

class SchemaScene;

/// @brief View (MVC): QGraphicsView fuer die SchemaScene – Pan/Zoom per Maus,
///        Klick auf einen Collection-Knoten signalisiert die Selektion (FA-09),
///        Doppelklick klappt Schema-Zeilen auf/zu (FA-06).
class SchemaView : public QGraphicsView {
    Q_OBJECT
public:
    explicit SchemaView(QWidget* parent = nullptr);

    /// @brief Szene setzen und Inhalt komplett einpassen.
    void setSchemaScene(SchemaScene* scene);
    /// @brief Gesamten Szeneninhalt in den View einpassen.
    void fitContent();

signals:
    /// @brief Collection-Knoten wurde angeklickt (FA-09).
    void collectionClicked(const QString& collectionName);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
};

} // namespace memdb::viewer
