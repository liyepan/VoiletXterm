#include "Style.h"

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>

namespace vx {

namespace {

struct Tokens {
    const char *window;
    const char *panel;
    const char *panelAlt;
    const char *border;
    const char *text;
    const char *muted;
    const char *dim;
    const char *accent;
    const char *accentStrong;
    const char *hover;
    const char *selected;
    const char *scrollHandle;
    const char *scrollHandleHover;
    const char *tooltipBg;
    const char *tooltipText;
    const char *tooltipBorder;
};

const Tokens kDark = {
    "#14121C", "#1B1826", "#252134", "#2C2740", "#E4E0F0", "#A29BC0", "#6B6485",
    "#A78BFA", "#6D4FD1", "#3B3555", "#4A3F7A", "#3B3555", "#4A3F7A", "#2C2740",
    "#E4E0F0", "#4A3F7A",
};

const Tokens kLight = {
    "#F4F3FA", "#FFFFFF", "#ECEAF5", "#DCD7E9", "#262133", "#6B6485", "#8F8AA3",
    "#7C5FE0", "#6D4FD1", "#E4E0F3", "#D8D1F5", "#C7C1DA", "#ADA4C9", "#FFFFFF",
    "#262133", "#C9C3DC",
};

const char *kStyleSheet = R"QSS(
    QMainWindow, QDialog { background: @window@; }
    QWidget { color: @text@; }
    QTabWidget::pane { border: 0; }
    QTabBar::tab {
        background: @panel@; color: @muted@; padding: 6px 16px; margin-right: 2px;
        border-top-left-radius: 4px; border-top-right-radius: 4px;
    }
    QTabBar::tab:selected { background: @window@; color: @text@; border-top: 2px solid @accent@; }
    QTabBar::tab:hover:!selected { background: @panelAlt@; }
    QToolBar { background: @panel@; border: 0; spacing: 4px; padding: 4px; }
    QToolBar QToolButton { color: @text@; padding: 5px 10px; border-radius: 4px; }
    QToolBar QToolButton:hover { background: @panelAlt@; }
    QMenuBar { background: @panel@; color: @text@; }
    QMenuBar::item { padding: 4px 10px; background: transparent; }
    QMenuBar::item:selected { background: @panelAlt@; border-radius: 4px; }
    QMenu { background: @panel@; color: @text@; border: 1px solid @border@; }
    QMenu::item { padding: 6px 26px; }
    QMenu::item:selected { background: @selected@; }
    QMenu::separator { height: 1px; background: @border@; margin: 4px 8px; }
    QStatusBar { background: @panel@; color: @muted@; }
    QStatusBar::item { border: 0; }
    QScrollBar:vertical { background: @window@; width: 10px; margin: 0; }
    QScrollBar::handle:vertical { background: @scrollHandle@; border-radius: 5px; min-height: 24px; }
    QScrollBar::handle:vertical:hover { background: @scrollHandleHover@; }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
    QLineEdit, QSpinBox, QComboBox {
        background: @panel@; color: @text@; border: 1px solid @border@;
        border-radius: 4px; padding: 5px 8px; selection-background-color: @accentStrong@;
        selection-color: #FFFFFF;
    }
    QLineEdit:focus, QSpinBox:focus, QComboBox:focus { border: 1px solid @accent@; }
    QComboBox QAbstractItemView {
        background: @panel@; color: @text@; border: 1px solid @border@;
        selection-background-color: @selected@; selection-color: @text@;
    }
    QPushButton {
        background: @panelAlt@; color: @text@; border: 0; padding: 6px 16px;
        border-radius: 4px; min-width: 64px;
    }
    QPushButton:hover { background: @hover@; }
    QPushButton:pressed { background: @selected@; }
    QPushButton:default { background: @accentStrong@; color: #FFFFFF; }
    QPushButton:default:hover { background: @accent@; }
    QListWidget {
        background: @panel@; color: @text@; border: 1px solid @border@; border-radius: 6px;
        selection-background-color: @selected@; selection-color: @text@;
    }
    QListWidget::item { padding: 6px 8px; }
    QListWidget::item:selected { background: @selected@; border-radius: 4px; }
    QLabel { color: @text@; }
    QLabel[role="muted"] { color: @muted@; }
    QLabel[role="dim"] { color: @dim@; }
    QLabel[role="title"] { color: @text@; }
    QCheckBox { color: @text@; }
    QGroupBox { border: 1px solid @border@; border-radius: 6px; margin-top: 8px; }
    QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; color: @muted@; }
    QToolTip { background: @tooltipBg@; color: @tooltipText@; border: 1px solid @tooltipBorder@; }
)QSS";

QString substituted(const Tokens &t)
{
    QString css = QString::fromLatin1(kStyleSheet);
    css.replace(QStringLiteral("@window@"), QLatin1String(t.window));
    css.replace(QStringLiteral("@panel@"), QLatin1String(t.panel));
    css.replace(QStringLiteral("@panelAlt@"), QLatin1String(t.panelAlt));
    css.replace(QStringLiteral("@border@"), QLatin1String(t.border));
    css.replace(QStringLiteral("@text@"), QLatin1String(t.text));
    css.replace(QStringLiteral("@muted@"), QLatin1String(t.muted));
    css.replace(QStringLiteral("@dim@"), QLatin1String(t.dim));
    css.replace(QStringLiteral("@accent@"), QLatin1String(t.accent));
    css.replace(QStringLiteral("@accentStrong@"), QLatin1String(t.accentStrong));
    css.replace(QStringLiteral("@hover@"), QLatin1String(t.hover));
    css.replace(QStringLiteral("@selected@"), QLatin1String(t.selected));
    css.replace(QStringLiteral("@scrollHandle@"), QLatin1String(t.scrollHandle));
    css.replace(QStringLiteral("@scrollHandleHover@"), QLatin1String(t.scrollHandleHover));
    css.replace(QStringLiteral("@tooltipBg@"), QLatin1String(t.tooltipBg));
    css.replace(QStringLiteral("@tooltipText@"), QLatin1String(t.tooltipText));
    css.replace(QStringLiteral("@tooltipBorder@"), QLatin1String(t.tooltipBorder));
    return css;
}

} // namespace

void applyStyle(bool dark)
{
    QApplication *app = qobject_cast<QApplication *>(QCoreApplication::instance());
    if (!app)
        return;

    static bool fusionInstalled = false;
    if (!fusionInstalled) {
        app->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        fusionInstalled = true;
    }

    QPalette palette;
    if (dark) {
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
        palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x6B, 0x64, 0x85));
    } else {
        palette.setColor(QPalette::Window, QColor(0xF4, 0xF3, 0xFA));
        palette.setColor(QPalette::WindowText, QColor(0x26, 0x21, 0x33));
        palette.setColor(QPalette::Base, QColor(0xFF, 0xFF, 0xFF));
        palette.setColor(QPalette::AlternateBase, QColor(0xEC, 0xEA, 0xF5));
        palette.setColor(QPalette::ToolTipBase, QColor(0xFF, 0xFF, 0xFF));
        palette.setColor(QPalette::ToolTipText, QColor(0x26, 0x21, 0x33));
        palette.setColor(QPalette::Text, QColor(0x26, 0x21, 0x33));
        palette.setColor(QPalette::Button, QColor(0xEC, 0xEA, 0xF5));
        palette.setColor(QPalette::ButtonText, QColor(0x26, 0x21, 0x33));
        palette.setColor(QPalette::BrightText, QColor(0xC0, 0x39, 0x2B));
        palette.setColor(QPalette::Highlight, QColor(0x6D, 0x4F, 0xD1));
        palette.setColor(QPalette::HighlightedText, QColor(0xFF, 0xFF, 0xFF));
        palette.setColor(QPalette::Link, QColor(0x5B, 0x3F, 0xC4));
        palette.setColor(QPalette::Disabled, QPalette::Text, QColor(0xA9, 0xA3, 0xBA));
        palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0xA9, 0xA3, 0xBA));
        palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0xA9, 0xA3, 0xBA));
    }
    app->setPalette(palette);
    app->setStyleSheet(substituted(dark ? kDark : kLight));
}

} // namespace vx
