#pragma once

#include <QColor>

namespace vx {

// UI colour tokens. Kept as data so the stylesheet and the contrast tests share
// exactly the same values.
struct UiColors {
    QColor window;
    QColor panel;
    QColor panelAlt;
    QColor border;
    QColor text;
    QColor muted;
    QColor dim;
    QColor accent;
    QColor accentStrong;
    QColor hover;
    QColor selected;
    QColor scrollHandle;
    QColor scrollHandleHover;
    QColor tooltipBg;
    QColor tooltipText;
    QColor tooltipBorder;
};

UiColors uiColors(bool dark);

// Installs the application palette and stylesheet for the requested scheme.
void applyStyle(bool dark);

} // namespace vx
