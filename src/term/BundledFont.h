#pragma once

#include <QString>

namespace vx {

// Registers the bundled JetBrains Mono NL faces (compiled into the binary via
// resources/fonts.qrc) and returns the resolved family name. Returns an empty
// string if the resources are missing. Safe to call more than once; must be
// called after QGuiApplication exists.
QString bundledFontFamily();

} // namespace vx
