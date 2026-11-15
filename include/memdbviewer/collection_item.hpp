#pragma once

#include <QGraphicsItem>
#include <QSet>
#include <QString>

#include <vector>

namespace memdb::viewer {

/// @brief View (MVC): Collection-Knoten der Schema-Szene (FA-04). Titel mit
///        Dokumentanzahl/Schema-Status, darunter eine Zeile pro Schema-Feld –
///        rekursiv eingerueckt bei Unter-Schemas (FA-06), per Doppelklick
///        auf-/zuklappbar (FA-09).
class CollectionItem : public QGraphicsItem {
public:
    struct FieldRow {
        QString name;  ///< Anzeige (letztes Pfadsegment)
        QString path;  ///< voller Feldpfad (Schluessel fuers Collapse)
        QString type;  ///< "" = unbekannt (schema-los / index-only)
        QString flags; ///< z. B. "notNull", "index", "unique"
        int depth = 0; ///< Einrueckung (0 = Top-Level-Feld)
    };

    CollectionItem(QString title, QString subtitle, std::vector<FieldRow> fields,
                   QGraphicsItem* parent = nullptr);

    enum { Type = UserType + 100 };
    int type() const override { return Type; }

    QRectF boundingRect() const override { return rect_; }
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override;

    const QString& title() const { return title_; }
    const std::vector<FieldRow>& fields() const { return fields_; }
    int visibleRowCount() const { return static_cast<int>(visible_.size()); }
    bool isCollapsed(const QString& path) const { return collapsed_.contains(path); }

    /// @brief Sichtbare Zeile an Item-Position auf-/zuklappen (FA-06/FA-09).
    void toggleCollapseAt(const QPointF& itemPos);
    /// @brief Index der sichtbaren Zeile an Item-Position, sonst -1.
    int rowAt(const QPointF& itemPos) const;

    static constexpr qreal kWidth = 280.0;
    static constexpr qreal kHeaderHeight = 42.0;
    static constexpr qreal kRowHeight = 18.0;
    static constexpr qreal kPadding = 8.0;
    static constexpr qreal kIndent = 14.0;

private:
    bool hasChildren(const QString& path) const;
    void rebuildVisible();

    QString title_;
    QString subtitle_;
    std::vector<FieldRow> fields_;
    std::vector<int> visible_; ///< Indizes in fields_, die aktuell sichtbar sind
    QSet<QString> collapsed_;
    QRectF rect_;
};

} // namespace memdb::viewer
