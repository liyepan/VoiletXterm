#include "BundledFont.h"

#include <QFontDatabase>

namespace vx {

QString bundledFontFamily()
{
    static const QString family = [] {
        const char *faces[] = {
            ":/fonts/JetBrainsMonoNL-Regular.ttf",
            ":/fonts/JetBrainsMonoNL-Medium.ttf",
            ":/fonts/JetBrainsMonoNL-SemiBold.ttf",
            ":/fonts/JetBrainsMonoNL-Bold.ttf",
            ":/fonts/JetBrainsMonoNL-Italic.ttf",
            ":/fonts/JetBrainsMonoNL-BoldItalic.ttf",
        };
        QString resolved;
        for (const char *face : faces) {
            const int id = QFontDatabase::addApplicationFont(QString::fromLatin1(face));
            if (id < 0)
                continue;
            const QStringList families = QFontDatabase::applicationFontFamilies(id);
            if (resolved.isEmpty() && !families.isEmpty())
                resolved = families.first();
        }
        return resolved;
    }();
    return family;
}

} // namespace vx
