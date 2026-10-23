#pragma once

#include <QColor>

namespace vx {

struct TerminalTheme {
    QColor background;
    QColor foreground;
    QColor cursor;
    QColor cursorText;
    QColor selection;
    QColor ansi[16];
};

TerminalTheme darkVioletTheme();

} // namespace vx
