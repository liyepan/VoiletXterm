#pragma once

#include <cstdint>

namespace vx {

// Display width of a code point following the East Asian Width rules used by
// terminals: 0 for combining marks, 2 for wide (CJK / emoji) and 1 otherwise.
int charWidth(char32_t cp);

bool isZeroWidth(char32_t cp);
bool isWideChar(char32_t cp);

} // namespace vx
