#pragma once

#include "confusion_model.hpp"

#include <QPainter>
#include <QWidget>

namespace memdb::viewer::detail {

/// @brief View (MVC) der Confusion-Matrix (v0.42): 2x2-Raster
///        (Spalten: FK entdeckt ja/nein, Zeilen: Referenz deklariert ja/nein)
///        mit TP/FP/FN/TN-Zellen und Precision/Recall/F1 darunter.
class ConfusionView : public QWidget {
    Q_OBJECT
public:
    explicit ConfusionView(ConfusionModel* model, QWidget* parent = nullptr) : QWidget(parent), model_(model) {
        setMinimumHeight(170);
        connect(model_, &ConfusionModel::changed, this, qOverload<>(&QWidget::update));
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.fillRect(rect(), palette().color(QWidget::backgroundRole()));
        if (!model_ || !model_->hasConfusion()) {
            p.setPen(palette().color(QPalette::Mid));
            p.drawText(rect(), Qt::AlignCenter,
                       QStringLiteral("Keine Confusion-Matrix – Umfang „Ganze Database“ wählen\n"
                                      "(braucht deklarierte Referenzen in __references, FA-18)"));
            return;
        }
        const auto& cm = *model_->confusion();
        const int labelW = 110, cellW = 90, cellH = 56, top = 34, left = labelW + 8;
        const QRect grid(left, top, cellW * 2, cellH * 2);

        // Achsenbeschriftung
        p.setPen(palette().color(QPalette::Text));
        p.drawText(QRect(left, 6, cellW * 2, 20), Qt::AlignHCenter, QStringLiteral("FK entdeckt (≥ Schwelle)"));
        p.save();
        p.translate(6, top + cellH);
        p.rotate(-90);
        p.drawText(QRect(-cellH, -labelW / 2, cellH * 2, 20), Qt::AlignHCenter,
                   QStringLiteral("Referenz deklariert (__references)"));
        p.restore();
        p.drawText(QRect(left, top, cellW, 18), Qt::AlignHCenter, QStringLiteral("ja"));
        p.drawText(QRect(left + cellW, top, cellW, 18), Qt::AlignHCenter, QStringLiteral("nein"));
        p.drawText(QRect(0, top, labelW, cellH), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("ja"));
        p.drawText(QRect(0, top + cellH, labelW, cellH), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("nein"));

        // Zellen: [zeile deklariert ja/nein][spalte entdeckt ja/nein] -> TP, FN / FP, TN
        struct Cell { QRect r; QString title; std::size_t value; QColor color; };
        const Cell cells[4] = {
            {QRect(left, top, cellW, cellH), QStringLiteral("TP"), cm.tp, QColor(76, 175, 80)},
            {QRect(left + cellW, top, cellW, cellH), QStringLiteral("FP"), cm.fp, QColor(255, 152, 0)},
            {QRect(left, top + cellH, cellW, cellH), QStringLiteral("FN"), cm.fn, QColor(244, 67, 54)},
            {QRect(left + cellW, top + cellH, cellW, cellH), QStringLiteral("TN"), cm.tn, QColor(158, 158, 158)},
        };
        for (const auto& c : cells) {
            p.setPen(QPen(c.color.darker(120), 1.5));
            p.setBrush(c.color.lighter(160));
            p.drawRoundedRect(c.r.adjusted(2, 2, -2, -2), 6, 6);
            p.setPen(c.color.darker(160));
            QFont f = p.font();
            f.setBold(true);
            f.setPointSize(f.pointSize() + 2);
            p.setFont(f);
            p.drawText(c.r, Qt::AlignHCenter | Qt::AlignTop | Qt::TextSingleLine,
                       QStringLiteral("%1 = %2").arg(c.title).arg(c.value));
        }

        // Metriken
        p.setPen(palette().color(QPalette::Text));
        QFont f = p.font();
        f.setBold(false);
        p.setFont(f);
        p.drawText(QRect(left, top + cellH * 2 + 8, cellW * 2 + 160, 20),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("Precision %1 · Recall %2 · F1 %3")
                       .arg(cm.precision, 0, 'f', 2)
                       .arg(cm.recall, 0, 'f', 2)
                       .arg(cm.f1, 0, 'f', 2));
    }

private:
    ConfusionModel* model_;
};

} // namespace memdb::viewer::detail
