#include "Palette.h"

namespace vx {

namespace {

const quint32 kBase[16] = {
    0x000000, 0xCD0000, 0x00CD00, 0xCDCD00, 0x0000EE, 0xCD00CD, 0x00CDCD, 0xE5E5E5,
    0x7F7F7F, 0xFF0000, 0x00FF00, 0xFFFF00, 0x5C5CFF, 0xFF00FF, 0x00FFFF, 0xFFFFFF,
};

const int kCubeLevels[6] = {0, 95, 135, 175, 215, 255};

} // namespace

quint32 defaultPaletteColor(int index)
{
    if (index < 0)
        index = 0;
    if (index < 16)
        return kBase[index];
    if (index < 232) {
        const int i = index - 16;
        const int r = kCubeLevels[i / 36];
        const int g = kCubeLevels[(i / 6) % 6];
        const int b = kCubeLevels[i % 6];
        return quint32((r << 16) | (g << 8) | b);
    }
    const int v = 8 + (index - 232) * 10;
    return quint32((v << 16) | (v << 8) | v);
}

} // namespace vx
