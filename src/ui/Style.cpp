#include "Style.h"

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>

namespace vx {

void applyDarkStyle(QApplication &app)
{
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(0x14, 0x12, 0x1C));
    palette.setColor(QPalette::WindowText, QColor(0xE4, 0xE0, 0xF0));
    palette.setColor(QPalette::Base, QColor(0x1B, 0x18, 0x26));
    palette.setColor(QPalette::AlternateBase, QColor(0x24, 0x20, 0x33));
    palette.setColor(QPalette::ToolTipBase, QColor(0x1B, 0x18, 0x26));
    palette.setColor(QPalette::ToolTipText, QColor(0xE4, 0xE0, 0xF0));
    palette.setColor(QPalette::Text, QColor(0xE4, 0xE0, 0xF0));
    palette.setColor(QPalette::Button, QColor(0x24, 0x20, 0x33));
    palette.setColor(QPalette::ButtonText, QColor(0xE4, 0xE0, 0xF0));
    palette.setColor(QPalette::BrightText, QColor(0xFF, 0x7B, 0x86));
    palette.setColor(QPalette::Highlight, QColor(0x6D, 0x4F, 0xD1));
    palette.setColor(QPalette::HighlightedText, QColor(0xFF, 0xFF, 0xFF));
    palette.setColor(QPalette::Link, QColor(0xA7, 0x8B, 0xFA));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(0x6B, 0x64, 0x85));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x6B, 0x64, 0x85));
    app.setPalette(palette);

    app.setStyleSheet(QStringLiteral(R"(
        QMainWindow, QDialog { background: #14121C; }
        QTabWidget::pane { border: 0; }
        QTabBar::tab {
            background: #1B1826; color: #A29BC0; padding: 6px 16px; margin-right: 2px;
            border-top-left-radius: 4px; border-top-right-radius: 4px;
        }
        QTabBar::tab:selected { background: #14121C; color: #E4E0F0; border-top: 2px solid #A78BFA; }
        QTabBar::tab:hover:!selected { background: #252134; }
        QToolBar { background: #1B1826; border: 0; spacing: 4px; padding: 4px; }
        QToolBar QToolButton { color: #E4E0F0; padding: 5px 10px; border-radius: 4px; }
        QToolBar QToolButton:hover { background: #2C2740; }
        QMenuBar { background: #1B1826; color: #E4E0F0; }
        QMenuBar::item { padding: 4px 10px; background: transparent; }
        QMenuBar::item:selected { background: #2C2740; border-radius: 4px; }
        QMenu { background: #1B1826; color: #E4E0F0; border: 1px solid #2C2740; }
        QMenu::item { padding: 6px 26px; }
        QMenu::item:selected { background: #4A3F7A; }
        QStatusBar { background: #1B1826; color: #A29BC0; }
        QStatusBar::item { border: 0; }
        QScrollBar:vertical { background: #14121C; width: 10px; margin: 0; }
        QScrollBar::handle:vertical { background: #3B3555; border-radius: 5px; min-height: 24px; }
        QScrollBar::handle:vertical:hover { background: #4A3F7A; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
        QLineEdit, QSpinBox, QComboBox {
            background: #1B1826; color: #E4E0F0; border: 1px solid #2C2740;
            border-radius: 4px; padding: 5px 8px; selection-background-color: #6D4FD1;
        }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus { border: 1px solid #A78BFA; }
        QComboBox QAbstractItemView { background: #1B1826; color: #E4E0F0; selection-background-color: #4A3F7A; }
        QPushButton {
            background: #2C2740; color: #E4E0F0; border: 0; padding: 6px 16px;
            border-radius: 4px; min-width: 64px;
        }
        QPushButton:hover { background: #3B3555; }
        QPushButton:pressed { background: #4A3F7A; }
        QPushButton:default { background: #6D4FD1; }
        QPushButton:default:hover { background: #7C5FE0; }
        QListWidget { background: #1B1826; color: #E4E0F0; border: 1px solid #2C2740; border-radius: 6px; }
        QListWidget::item { padding: 6px 8px; }
        QListWidget::item:selected { background: #4A3F7A; border-radius: 4px; }
        QLabel { color: #CFC8E4; }
        QCheckBox { color: #CFC8E4; }
        QToolTip { background: #2C2740; color: #E4E0F0; border: 1px solid #4A3F7A; }
    )"));
}

} // namespace vx
