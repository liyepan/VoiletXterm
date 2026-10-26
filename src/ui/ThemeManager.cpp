#include "ThemeManager.h"

#include "Style.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QPalette>
#include <QProcess>
#include <QSettings>
#include <QTimer>

namespace vx {

namespace {

const char *kModeKey = "theme/mode";

QString runCommand(const QString &program, const QStringList &arguments)
{
    QProcess process;
    process.start(program, arguments);
    if (!process.waitForStarted(300))
        return QString();
    if (!process.waitForFinished(400)) {
        process.kill();
        process.waitForFinished(100);
        return QString();
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        return QString();
    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

} // namespace

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
{
    // Remember the platform palette before the first applyStyle() overwrites it.
    m_systemFallbackDark = QGuiApplication::palette().color(QPalette::Window).lightness() < 128;

    m_timer = new QTimer(this);
    m_timer->setInterval(30 * 1000);
    connect(m_timer, &QTimer::timeout, this, [this] { reevaluate(false); });
    m_timer->start();
}

ThemeManager *ThemeManager::instance()
{
    static ThemeManager *manager = new ThemeManager;
    return manager;
}

void ThemeManager::initialize()
{
    QSettings settings;
    const int stored = settings.value(QLatin1String(kModeKey), int(System)).toInt();
    m_mode = Mode(qBound(int(System), stored, int(Dark)));
    reevaluate(true);
}

void ThemeManager::setMode(Mode mode, bool persist)
{
    if (mode < System || mode > Dark)
        return;
    if (m_mode == mode && m_applied)
        return;
    m_mode = mode;
    if (persist) {
        QSettings settings;
        settings.setValue(QLatin1String(kModeKey), int(mode));
    }
    reevaluate(true);
}

bool ThemeManager::schedulePrefersDark(const QTime &now)
{
    const int hour = now.hour();
    return hour < 6 || hour >= 18;
}

void ThemeManager::reevaluate(bool force)
{
    bool dark = true;
    switch (m_mode) {
    case Light:
        dark = false;
        break;
    case Dark:
        dark = true;
        break;
    case Schedule:
        dark = schedulePrefersDark();
        break;
    case System:
        dark = detectSystemDark();
        break;
    }

    if (!force && m_applied && dark == m_dark)
        return;

    m_dark = dark;
    applyStyle(m_dark);
    m_applied = true;
    emit schemeChanged(m_dark);
}

bool ThemeManager::detectSystemDark() const
{
#ifdef Q_OS_WIN
    QSettings personalize(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
        QSettings::NativeFormat);
    const QVariant appsUseLightTheme = personalize.value(QStringLiteral("AppsUseLightTheme"));
    if (appsUseLightTheme.isValid())
        return appsUseLightTheme.toInt() == 0;
#endif

#ifdef Q_OS_MACOS
    // "defaults" exits non-zero when the key is absent, which means light mode.
    QProcess defaults;
    defaults.start(QStringLiteral("defaults"),
                   {QStringLiteral("read"), QStringLiteral("-g"), QStringLiteral("AppleInterfaceStyle")});
    if (defaults.waitForFinished(500)) {
        const QString appearance = QString::fromUtf8(defaults.readAllStandardOutput()).trimmed();
        return appearance.compare(QStringLiteral("Dark"), Qt::CaseInsensitive) == 0;
    }
#endif

#ifdef Q_OS_LINUX
    const QString scheme = runCommand(QStringLiteral("gsettings"),
                                      {QStringLiteral("get"), QStringLiteral("org.gnome.desktop.interface"),
                                       QStringLiteral("color-scheme")});
    if (scheme.contains(QStringLiteral("dark"), Qt::CaseInsensitive))
        return true;
    if (scheme.contains(QStringLiteral("light"), Qt::CaseInsensitive))
        return false;
    // "default" (or no gsettings at all) means the GTK theme name decides.
    const QString gtkTheme = runCommand(QStringLiteral("gsettings"),
                                        {QStringLiteral("get"), QStringLiteral("org.gnome.desktop.interface"),
                                         QStringLiteral("gtk-theme")});
    if (gtkTheme.contains(QStringLiteral("dark"), Qt::CaseInsensitive))
        return true;
#endif

    return m_systemFallbackDark;
}

} // namespace vx
