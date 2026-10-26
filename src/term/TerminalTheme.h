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

// Classic black on white text for the dark scheme, a clean paper scheme for the
// light one. Both are tuned for contrast first, decoration second.
TerminalTheme darkTheme();
TerminalTheme lightTheme();

TerminalTheme terminalThemeFor(bool dark);

// WCAG relative contrast, exposed so tests can guard the palettes.
double contrastRatio(const QColor &a, const QColor &b);

} // namespace vx
