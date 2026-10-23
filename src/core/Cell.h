#pragma once

#include <QString>
#include <QtGlobal>

namespace vx {

// Color encoding
//   0x00000000              -> terminal default foreground/background
//   0x01RRGGBB              -> direct 24-bit color
//   0x020000II              -> index into the 256 color palette
using Color = quint32;

constexpr Color kColorDefault = 0u;

inline Color rgbColor(int r, int g, int b)
{
    return 0x01000000u | (quint32(r & 0xFF) << 16) | (quint32(g & 0xFF) << 8) | quint32(b & 0xFF);
}

inline Color paletteColor(int index)
{
    return 0x02000000u | quint32(index & 0xFF);
}

inline bool isDefaultColor(Color c) { return c == 0u; }
inline bool isRgbColor(Color c) { return (c & 0xFF000000u) == 0x01000000u; }
inline bool isPaletteColor(Color c) { return (c & 0xFF000000u) == 0x02000000u; }
inline int paletteIndexOf(Color c) { return int(c & 0xFFu); }
inline quint32 rgbValueOf(Color c) { return c & 0x00FFFFFFu; }

enum AttrFlag : quint16 {
    AttrBold = 1u << 0,
    AttrDim = 1u << 1,
    AttrItalic = 1u << 2,
    AttrUnderline = 1u << 3,
    AttrBlink = 1u << 4,
    AttrReverse = 1u << 5,
    AttrHidden = 1u << 6,
    AttrStrike = 1u << 7,
    AttrOverline = 1u << 8,
    AttrDoubleUnderline = 1u << 9,
    AttrWide = 1u << 10,      // first half of a double width character
    AttrWideTail = 1u << 11,  // second half of a double width character
};

// A single screen cell. Kept at 16 bytes so that scrollback stays affordable.
struct Cell {
    char32_t ch = U' ';
    Color fg = kColorDefault;
    Color bg = kColorDefault;
    quint16 attr = 0;
    quint16 extra = 0;  // 1-based index into Screen's grapheme table, 0 = none

    bool isBlank() const { return ch == U' ' && attr == 0 && fg == kColorDefault && bg == kColorDefault && extra == 0; }
    bool isWideHead() const { return (attr & AttrWide) != 0; }
    bool isWideTail() const { return (attr & AttrWideTail) != 0; }
};

// Safe conversion of a stored code point (may be 0 for wide char tails).
inline QString codePointToString(char32_t cp)
{
    if (cp == 0 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
        return QString(QChar(U' '));
    uint value = uint(cp);
    return QString::fromUcs4(&value, 1);
}

static_assert(sizeof(Cell) == 16, "Cell must stay compact");

} // namespace vx
