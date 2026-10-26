#include "TerminalTheme.h"

#include <algorithm>
#include <cmath>

namespace vx {

namespace {

double channel(double value)
{
    value /= 255.0;
    return value <= 0.03928 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
}

double luminance(const QColor &color)
{
    return 0.2126 * channel(color.red()) + 0.7152 * channel(color.green())
        + 0.0722 * channel(color.blue());
}

} // namespace

double contrastRatio(const QColor &a, const QColor &b)
{
    const double la = luminance(a);
    const double lb = luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

TerminalTheme darkTheme()
{
    TerminalTheme t;
    t.background = QColor(0x0A, 0x0A, 0x0A);
    // Slightly brighter than the usual #E8E8E8: on a true black background a
    // high-value foreground is what gives the text its "lit" look.
    t.foreground = QColor(0xF5, 0xF5, 0xF5);
    t.cursor = QColor(0xF5, 0xF5, 0xF5);
    t.cursorText = QColor(0x0A, 0x0A, 0x0A);
    t.selection = QColor(0x26, 0x4F, 0x78);

    // The "black" slot is lifted off the background so it stays readable.
    t.ansi[0] = QColor(0x3C, 0x3C, 0x3C);
    t.ansi[1] = QColor(0xE0, 0x5A, 0x5A);
    t.ansi[2] = QColor(0x6F, 0xCF, 0x6F);
    t.ansi[3] = QColor(0xE5, 0xC0, 0x7B);
    t.ansi[4] = QColor(0x61, 0xA8, 0xFF);
    t.ansi[5] = QColor(0xD9, 0x8C, 0xD9);
    t.ansi[6] = QColor(0x5F, 0xC7, 0xD0);
    t.ansi[7] = QColor(0xE6, 0xE6, 0xE6);

    t.ansi[8] = QColor(0x7A, 0x7A, 0x7A);
    t.ansi[9] = QColor(0xFF, 0x6B, 0x6B);
    t.ansi[10] = QColor(0x8C, 0xE8, 0x8C);
    t.ansi[11] = QColor(0xFF, 0xD4, 0x79);
    t.ansi[12] = QColor(0x7C, 0xBB, 0xFF);
    t.ansi[13] = QColor(0xEA, 0xA3, 0xEA);
    t.ansi[14] = QColor(0x7F, 0xE3, 0xEA);
    t.ansi[15] = QColor(0xFF, 0xFF, 0xFF);
    return t;
}

TerminalTheme lightTheme()
{
    TerminalTheme t;
    t.background = QColor(0xFF, 0xFF, 0xFF);
    t.foreground = QColor(0x1A, 0x1A, 0x1A);
    t.cursor = QColor(0x1A, 0x1A, 0x1A);
    t.cursorText = QColor(0xFF, 0xFF, 0xFF);
    t.selection = QColor(0xB4, 0xD7, 0xFF);

    // On a white background the "white" slots become dark greys.
    t.ansi[0] = QColor(0x20, 0x20, 0x20);
    t.ansi[1] = QColor(0xC6, 0x28, 0x28);
    t.ansi[2] = QColor(0x2E, 0x7D, 0x32);
    t.ansi[3] = QColor(0xA9, 0x62, 0x00);
    t.ansi[4] = QColor(0x15, 0x65, 0xC0);
    t.ansi[5] = QColor(0x8E, 0x24, 0xAA);
    t.ansi[6] = QColor(0x00, 0x83, 0x8F);
    t.ansi[7] = QColor(0x5F, 0x5F, 0x5F);

    t.ansi[8] = QColor(0x61, 0x61, 0x61);
    t.ansi[9] = QColor(0xD3, 0x2F, 0x2F);
    t.ansi[10] = QColor(0x38, 0x8E, 0x3C);
    t.ansi[11] = QColor(0xB8, 0x73, 0x00);
    t.ansi[12] = QColor(0x19, 0x76, 0xD2);
    t.ansi[13] = QColor(0xAB, 0x47, 0xBC);
    t.ansi[14] = QColor(0x00, 0x97, 0xA7);
    t.ansi[15] = QColor(0x11, 0x11, 0x11);
    return t;
}



} // namespace vx
