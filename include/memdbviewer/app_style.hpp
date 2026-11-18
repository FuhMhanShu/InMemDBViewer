#pragma once

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QPalette>
#include <QString>

namespace memdb::viewer {

/// @brief Zentrales Theme der Viewer-App (NFA-07, UI-01): setzt den Fusion-Stil,
///        eine helle Palette und ein app-weites Stylesheet (Buttons, Tabs, Tabellen,
///        Docks, GroupBoxen, Splitter …). Einmalig am App-Start aufrufen:
///        `memdb::viewer::applyViewerTheme(&app);`
///        Farben centralized hier änderbar (Akzent: #2f6fdd).
inline void applyViewerTheme(QApplication* app) {
    if (!app)
        return;
    app->setStyle(QStringLiteral("Fusion"));

    QFont font = app->font();
    if (font.pointSize() <= 0)
        font.setPointSize(10);
    app->setFont(font);

    QPalette pal;
    pal.setColor(QPalette::Window, QColor(QStringLiteral("#f4f6f9")));
    pal.setColor(QPalette::WindowText, QColor(QStringLiteral("#1f2933")));
    pal.setColor(QPalette::Base, QColor(QStringLiteral("#ffffff")));
    pal.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#f0f4f8")));
    pal.setColor(QPalette::Text, QColor(QStringLiteral("#1f2933")));
    pal.setColor(QPalette::Button, QColor(QStringLiteral("#eef1f5")));
    pal.setColor(QPalette::ButtonText, QColor(QStringLiteral("#1f2933")));
    pal.setColor(QPalette::Highlight, QColor(QStringLiteral("#2f6fdd")));
    pal.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#ffffff")));
    pal.setColor(QPalette::Link, QColor(QStringLiteral("#2f6fdd")));
    pal.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#ffffff")));
    pal.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#1f2933")));
    pal.setColor(QPalette::Disabled, QPalette::Text, QColor(QStringLiteral("#9aa4b2")));
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(QStringLiteral("#9aa4b2")));
    pal.setColor(QPalette::Disabled, QPalette::Highlight, QColor(QStringLiteral("#b9c6da")));
    app->setPalette(pal);

    app->setStyleSheet(QStringLiteral(R"QSS(
QMainWindow, QDialog { background: #f4f6f9; }
QWidget { color: #1f2933; }
QLabel[dim="true"] { color: #6b7787; }

QGroupBox {
    background: #ffffff; border: 1px solid #d5dbe3; border-radius: 6px;
    margin-top: 12px; padding: 10px 8px 8px 8px; font-weight: 600; color: #33404f;
}
QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 10px; padding: 0 4px; }

QPushButton {
    background: #ffffff; border: 1px solid #c6cedb; border-radius: 5px; padding: 5px 14px;
}
QPushButton:hover { background: #eaf1fd; border-color: #2f6fdd; }
QPushButton:pressed { background: #d7e5fb; }
QPushButton:checked { background: #d7e5fb; border-color: #2f6fdd; color: #1f4fb0; }
QPushButton:disabled { color: #9aa4b2; background: #f4f6f9; border-color: #dde3ea; }
QPushButton:focus { border-color: #2f6fdd; }

QTabWidget::pane { border: 1px solid #d5dbe3; border-radius: 6px; background: #ffffff; top: -1px; }
QTabBar::tab {
    background: #eef1f5; border: 1px solid #d5dbe3; border-bottom: none;
    padding: 7px 18px; margin-right: 2px; border-top-left-radius: 5px; border-top-right-radius: 5px;
}
QTabBar::tab:selected { background: #ffffff; color: #2f6fdd; font-weight: 700; }
QTabBar::tab:hover:!selected { background: #e3e9f0; }

QTableView, QTreeView, QListView {
    background: #ffffff; border: 1px solid #d5dbe3; border-radius: 5px;
    gridline-color: #e5eaf0; alternate-background-color: #f6f8fb;
    selection-background-color: #d7e5fb; selection-color: #1f2933;
}
QHeaderView::section {
    background: #eef1f5; border: none; border-right: 1px solid #dde3ea; border-bottom: 1px solid #d5dbe3;
    padding: 5px 8px; font-weight: 600; color: #33404f;
}

QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QPlainTextEdit, QTextBrowser, QListWidget {
    background: #ffffff; border: 1px solid #c6cedb; border-radius: 5px; padding: 4px 6px;
    selection-background-color: #d7e5fb;
}
QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus,
QPlainTextEdit:focus, QTextBrowser:focus { border-color: #2f6fdd; }
QComboBox QAbstractItemView { border: 1px solid #c6cedb; background: #ffffff; selection-background-color: #d7e5fb; }
QCheckBox::indicator, QRadioButton::indicator { width: 15px; height: 15px; border: 1px solid #c6cedb; border-radius: 3px; background: #ffffff; }
QCheckBox::indicator:checked { background: #2f6fdd; border-color: #2f6fdd; }

QToolBar { background: #ffffff; border: none; border-bottom: 1px solid #d5dbe3; spacing: 6px; padding: 4px 6px; }
QToolButton {
    background: transparent; border: 1px solid transparent; border-radius: 5px; padding: 4px 9px;
}
QToolButton:hover { background: #eaf1fd; border-color: #2f6fdd; }
QToolButton:pressed { background: #d7e5fb; }
QToolButton:checked { background: #d7e5fb; border-color: #2f6fdd; color: #1f4fb0; }
QToolButton:disabled { color: #9aa4b2; }

QMenuBar { background: #ffffff; border-bottom: 1px solid #d5dbe3; }
QMenuBar::item { padding: 5px 10px; border-radius: 4px; }
QMenuBar::item:selected { background: #eaf1fd; }
QMenu { background: #ffffff; border: 1px solid #d5dbe3; border-radius: 5px; padding: 4px; }
QMenu::item { padding: 5px 22px 5px 22px; border-radius: 4px; }
QMenu::item:selected { background: #d7e5fb; }
QMenu::separator { height: 1px; background: #e5eaf0; margin: 4px 8px; }

QDockWidget { color: #33404f; }
QDockWidget::title {
    background: #eef1f5; border: 1px solid #d5dbe3; border-radius: 5px;
    padding: 6px 10px; font-weight: 600;
}

QStatusBar { background: #ffffff; border-top: 1px solid #d5dbe3; }
QSplitter::handle { background: #e5eaf0; }
QSplitter::handle:horizontal { width: 4px; }
QSplitter::handle:vertical { height: 4px; }
QSplitter::handle:hover { background: #c9d6ea; }

QProgressBar { border: 1px solid #c6cedb; border-radius: 5px; background: #ffffff; text-align: center; min-height: 14px; }
QProgressBar::chunk { background: #2f6fdd; border-radius: 4px; }

QGraphicsView { background: #ffffff; border: 1px solid #d5dbe3; border-radius: 5px; }
QToolTip { background: #ffffff; color: #1f2933; border: 1px solid #c6cedb; padding: 4px 6px; }
)QSS"));
}

} // namespace memdb::viewer
