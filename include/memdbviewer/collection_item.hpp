#pragma once

#include <QGraphicsItem>
#include <QString>

#include <vector>

namespace memdb::viewer {

/// @brief View (MVC): Collection-Knoten der Schema-Szene (FA-04). Titel mit
///        Dokumentanzahl/Schema-Status, darunter eine Zeile pro Schema-Feld
///        (Name, Typ, Flags wie notNull/index/unique).
class CollectionItem : public QGraphicsItem {
public:
    struct FieldRow {
        QString name;
        QString type;  ///< "" = unbekannt (schema-los / index-only)
        QString flags; ///< z. B. "notNull", "index", "unique"
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

    static constexpr qreal kWidth = 280.0;
    static constexpr qreal kHeaderHeight = 42.0;
    static constexpr qreal kRowHeight = 18.0;
    static constexpr qreal kPadding = 8.0;

private:
    QString title_;
    QString subtitle_;
    std::vector<FieldRow> fields_;
    QRectF rect_;
};

} // namespace memdb::viewer
