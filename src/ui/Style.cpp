#include "Style.h"

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>

namespace vx {

UiColors uiColors(bool dark)
{
    UiColors c;
    if (dark) {
        c.window = QColor(0x1B, 0x1B, 0x1D);
        c.panel = QColor(0x23, 0x23, 0x26);
        c.panelAlt = QColor(0x2C, 0x2C, 0x30);
        c.border = QColor(0x3E, 0x3E, 0x44);
        c.text = QColor(0xE6, 0xE6, 0xE6);
        c.muted = QColor(0xA8, 0xA8, 0xAE);
        c.dim = QColor(0x7C, 0x7C, 0x82);
        c.accent = QColor(0x6C, 0x8C, 0xFF);
        c.accentStrong = QColor(0x4C, 0x6E, 0xF5);
        c.hover = QColor(0x33, 0x33, 0x3A);
        c.selected = QColor(0x34, 0x40, 0x5C);
        c.scrollHandle = QColor(0x4A, 0x4A, 0x50);
        c.scrollHandleHover = QColor(0x5C, 0x5C, 0x64);
        c.tooltipBg = QColor(0x2C, 0x2C, 0x30);
        c.tooltipText = QColor(0xED, 0xED, 0xED);
        c.tooltipBorder = QColor(0x4A, 0x4A, 0x50);
    } else {
        c.window = QColor(0xF4, 0xF4, 0xF6);
        c.panel = QColor(0xFF, 0xFF, 0xFF);
        c.panelAlt = QColor(0xED, 0xED, 0xF1);
        c.border = QColor(0xC8, 0xC8, 0xD0);
        c.text = QColor(0x1C, 0x1C, 0x1F);
        c.muted = QColor(0x5A, 0x5A, 0x62);
        c.dim = QColor(0x85, 0x85, 0x8E);
        c.accent = QColor(0x2F, 0x6F, 0xEB);
        c.accentStrong = QColor(0x1F, 0x5B, 0xD8);
        c.hover = QColor(0xE4, 0xE4, 0xEA);
        c.selected = QColor(0xD5, 0xE3, 0xFB);
        c.scrollHandle = QColor(0xC4, 0xC4, 0xCE);
        c.scrollHandleHover = QColor(0xA6, 0xA6, 0xB2);
        c.tooltipBg = QColor(0xFF, 0xFF, 0xFF);
        c.tooltipText = QColor(0x1C, 0x1C, 0x1F);
        c.tooltipBorder = QColor(0xC8, 0xC8, 0xD0);
    }
    return c;
}

namespace {

const char *kStyleSheet = R"QSS(
    QMainWindow, QDialog { background: @window@; }
    QWidget { color: @text@; }

    QTabWidget::pane { border: 1px solid @border@; top: -1px; }
    QTabBar { background: @panel@; }
    QTabBar::tab {
        background: transparent; color: @muted@;
        padding: 7px 18px; margin-right: 2px;
        border: 1px solid transparent; border-bottom: none;
        border-top-left-radius: 4px; border-top-right-radius: 4px;
        font-weight: 600;
    }
    QTabBar::tab:hover:!selected { background: @panelAlt@; color: @text@; }
    QTabBar::tab:selected {
        background: @window@; color: @text@;
        border: 1px solid @border@; border-bottom: none;
        border-top: 2px solid @accent@;
    }

    QToolBar { background: @panel@; border: 0; border-bottom: 1px solid @border@; spacing: 4px; padding: 4px; }
    QToolBar QToolButton {
        color: @text@; padding: 5px 10px; border-radius: 4px;
        border: 1px solid transparent;
    }
    QToolBar QToolButton:hover { background: @panelAlt@; border-color: @border@; }
    QToolBar QToolButton:pressed { background: @hover@; }

    QMenuBar { background: @panel@; color: @text@; border-bottom: 1px solid @border@; }
    QMenuBar::item { padding: 4px 10px; background: transparent; }
    QMenuBar::item:selected { background: @panelAlt@; border-radius: 4px; }
    QMenu { background: @panel@; color: @text@; border: 1px solid @border@; padding: 4px; }
    QMenu::item { padding: 6px 26px; border-radius: 4px; }
    QMenu::item:selected { background: @selected@; }
    QMenu::separator { height: 1px; background: @border@; margin: 4px 8px; }

    QStatusBar { background: @panel@; color: @muted@; border-top: 1px solid @border@; }
    QStatusBar::item { border: 0; }

    QScrollBar:vertical { background: transparent; width: 11px; margin: 0; }
    QScrollBar::handle:vertical { background: @scrollHandle@; border-radius: 5px; min-height: 24px; }
    QScrollBar::handle:vertical:hover { background: @scrollHandleHover@; }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }

    QLineEdit, QSpinBox, QComboBox {
        background: @panel@; color: @text@; border: 1px solid @border@;
        border-radius: 5px; padding: 5px 8px; min-height: 18px;
        selection-background-color: @accentStrong@; selection-color: #FFFFFF;
    }
    QLineEdit:hover, QSpinBox:hover, QComboBox:hover { border-color: @dim@; }
    QLineEdit:focus, QSpinBox:focus, QComboBox:focus { border: 1px solid @accent@; }
    QComboBox::drop-down { border: 0; width: 18px; }
    QComboBox QAbstractItemView {
        background: @panel@; color: @text@; border: 1px solid @border@;
        selection-background-color: @selected@; selection-color: @text@;
    }

    QPushButton {
        background: @panelAlt@; color: @text@;
        font-weight: 500;
        border: 1px solid @border@; border-radius: 5px;
        padding: 6px 18px; min-width: 64px; min-height: 20px;
    }
    QPushButton:hover { background: @hover@; border-color: @dim@; }
    QPushButton:pressed { background: @selected@; }
    QPushButton:disabled { color: @dim@; border-color: @panelAlt@; }
    QPushButton:default { background: @accentStrong@; border-color: @accentStrong@; color: #FFFFFF; }
    QPushButton:default:hover { background: @accent@; border-color: @accent@; }

    QListWidget {
        background: @panel@; color: @text@; border: 1px solid @border@; border-radius: 6px;
        selection-background-color: @selected@; selection-color: @text@;
    }
    QListWidget::item { padding: 6px 8px; border-radius: 4px; }
    QListWidget::item:selected { background: @selected@; }

    QLabel { color: @text@; }
    QLabel[role="muted"] { color: @muted@; }
    QLabel[role="dim"] { color: @dim@; }
    QLabel[role="title"] { color: @text@; }
    QCheckBox { color: @text@; }
    QGroupBox { border: 1px solid @border@; border-radius: 6px; margin-top: 8px; }
    QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; color: @muted@; }
    QToolTip { background: @tooltipBg@; color: @tooltipText@; border: 1px solid @tooltipBorder@; padding: 4px; }
)QSS";

QString substituted(const UiColors &c)
{
    static const char *keys[] = {
        "@window@", "@panel@", "@panelAlt@", "@border@", "@text@", "@muted@", "@dim@",
        "@accent@", "@accentStrong@", "@hover@", "@selected@", "@scrollHandle@",
        "@scrollHandleHover@", "@tooltipBg@", "@tooltipText@", "@tooltipBorder@",
    };
    const QColor values[] = {
        c.window, c.panel, c.panelAlt, c.border, c.text, c.muted, c.dim,
        c.accent, c.accentStrong, c.hover, c.selected, c.scrollHandle,
        c.scrollHandleHover, c.tooltipBg, c.tooltipText, c.tooltipBorder,
    };

    QString css = QString::fromLatin1(kStyleSheet);
    for (int i = 0; i < int(sizeof(keys) / sizeof(keys[0])); ++i)
        css.replace(QLatin1String(keys[i]), values[i].name());
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

    const UiColors c = uiColors(dark);

    QPalette palette;
    palette.setColor(QPalette::Window, c.window);
    palette.setColor(QPalette::WindowText, c.text);
    palette.setColor(QPalette::Base, c.panel);
    palette.setColor(QPalette::AlternateBase, c.panelAlt);
    palette.setColor(QPalette::ToolTipBase, c.tooltipBg);
    palette.setColor(QPalette::ToolTipText, c.tooltipText);
    palette.setColor(QPalette::Text, c.text);
    palette.setColor(QPalette::Button, c.panelAlt);
    palette.setColor(QPalette::ButtonText, c.text);
    palette.setColor(QPalette::BrightText, QColor(0xFF, 0x6B, 0x6B));
    palette.setColor(QPalette::Highlight, c.accentStrong);
    palette.setColor(QPalette::HighlightedText, QColor(0xFF, 0xFF, 0xFF));
    palette.setColor(QPalette::Link, c.accent);
    palette.setColor(QPalette::PlaceholderText, c.dim);
    palette.setColor(QPalette::Disabled, QPalette::Text, c.dim);
    palette.setColor(QPalette::Disabled, QPalette::WindowText, c.dim);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, c.dim);
    app->setPalette(palette);
    app->setStyleSheet(substituted(c));
}

} // namespace vx
