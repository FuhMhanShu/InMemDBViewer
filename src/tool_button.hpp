#pragma once

#include <QStyle>
#include <QToolButton>
#include <QWidget>

namespace memdb::viewer::detail {

/// @brief Kompakter Button mit Icon (UI-03/UI-07): QToolButton mit Text neben dem Icon
///        (autoRaise – Rahmen erst bei Hover), Standard-Icon aus dem QStyle.
inline QToolButton* makeToolButton(QWidget* parent, QStyle::StandardPixmap icon, const QString& text,
                                   const QString& toolTip = QString()) {
    auto* b = new QToolButton(parent);
    b->setAutoRaise(true);
    b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    b->setIcon(parent->style()->standardIcon(icon));
    b->setText(text);
    if (!toolTip.isEmpty())
        b->setToolTip(toolTip);
    return b;
}

} // namespace memdb::viewer::detail
