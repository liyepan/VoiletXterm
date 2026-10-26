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

// The terminal keeps the classic black-on-light-grey scheme regardless of the
// surrounding UI: a light UI with a black terminal is what people expect, and a
// white terminal is hard to read for long sessions.
TerminalTheme darkTheme();

// Kept for the "light terminal" palette tests and future use.
TerminalTheme lightTheme();

// WCAG relative contrast, exposed so tests can guard the palettes.
double contrastRatio(const QColor &a, const QColor &b);

} // namespace vx
