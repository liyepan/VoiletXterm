#include "ThemeManager.h"

#include "Style.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QPalette>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

namespace vx {

namespace {

const char *kModeKey = "theme/mode";

// gdbus prints variants like (<<uint32 1>>,); pull the number out.
int parseUint32(const QString &text)
{
    static const QRegularExpression pattern(QStringLiteral("uint32\\s*(\\d+)"));
    const QRegularExpressionMatch match = pattern.match(text);
    return match.hasMatch() ? match.captured(1).toInt() : -1;
}

// Detection runs on the GUI thread, so the timeouts stay short: a missing tool
// fails fast and a working one answers in a few milliseconds.
QString runCommand(const QString &program, const QStringList &arguments)
{
    static QHash<QString, bool> available;
    if (!available.contains(program))
        available.insert(program, !QStandardPaths::findExecutable(program).isEmpty());
    if (!available.value(program))
        return QString();

    QProcess process;
    process.start(program, arguments);
    if (!process.waitForStarted(120))
        return QString();
    if (!process.waitForFinished(150)) {
        process.kill();
        process.waitForFinished(50);
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
    // The UI follows the desktop by default. The terminal keeps its own dark
    // palette, so a light desktop no longer turns the terminal white.
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

    if (m_mode == System) {
        static QString logged;
        if (m_systemSource != logged) {
            logged = m_systemSource;
            qInfo().noquote() << "[theme] system prefers" << (dark ? "dark" : "light")
                              << "via" << m_systemSource;
        }
    }

    m_dark = dark;
    applyStyle(m_dark);
    m_applied = true;
    emit schemeChanged(m_dark);
}

bool ThemeManager::detectSystemDark()
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
    // 1) XDG desktop portal: the cross-desktop way GTK4, KDE and Flatpak apps
    //    publish the user's preference (0 default, 1 prefer dark, 2 prefer light).
    const QString portal = runCommand(
        QStringLiteral("gdbus"),
        {QStringLiteral("call"), QStringLiteral("--session"),
         QStringLiteral("--dest"), QStringLiteral("org.freedesktop.portal.Desktop"),
         QStringLiteral("--object-path"), QStringLiteral("/org/freedesktop/portal/desktop"),
         QStringLiteral("--method"), QStringLiteral("org.freedesktop.portal.Settings.Read"),
         QStringLiteral("org.freedesktop.appearance"), QStringLiteral("color-scheme")});
    const int portalValue = parseUint32(portal);
    if (portalValue == 1 || portalValue == 2) {
        m_systemSource = QStringLiteral("xdg-desktop-portal");
        return portalValue == 1;
    }

    // 2) GTK_THEME=Adwaita:dark wins over the settings for GTK applications.
    const QString envTheme = qEnvironmentVariable("GTK_THEME");
    if (!envTheme.isEmpty()) {
        m_systemSource = QStringLiteral("GTK_THEME");
        return envTheme.contains(QStringLiteral("dark"), Qt::CaseInsensitive);
    }

    // 3) KDE publishes its colours in kdeglobals; on a KDE session the GTK
    //    settings below are meaningless (they just report Adwaita).
    const bool kdeSession =
        qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains(QStringLiteral("KDE"), Qt::CaseInsensitive)
        || !qEnvironmentVariable("KDE_FULL_SESSION").isEmpty();
    // Parse kdeglobals by hand: QSettings chokes on the "[Colors:Window]" group.
    const auto kdePreference = [](bool &ok) {
        ok = false;
        QFile file(QDir::homePath() + QStringLiteral("/.config/kdeglobals"));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return false;
        bool inWindowSection = false;
        while (!file.atEnd()) {
            const QString line = QString::fromUtf8(file.readLine()).trimmed();
            if (line.startsWith(QLatin1Char('['))) {
                inWindowSection = line == QLatin1String("[Colors:Window]");
                continue;
            }
            if (!inWindowSection || !line.startsWith(QLatin1String("BackgroundNormal=")))
                continue;
            const QStringList channels =
                line.mid(int(qstrlen("BackgroundNormal="))).split(QLatin1Char(','));
            if (channels.size() < 3)
                return false;
            const QColor background(channels.at(0).trimmed().toInt(),
                                    channels.at(1).trimmed().toInt(),
                                    channels.at(2).trimmed().toInt());
            ok = background.isValid();
            return ok && background.lightness() < 128;
        }
        return false;
    };

    bool kdeOk = false;
    const bool kdeDark = kdePreference(kdeOk);
    if (kdeSession && kdeOk) {
        m_systemSource = QStringLiteral("kdeglobals");
        return kdeDark;
    }

    // 4) GTK settings, shared by GTK3 and GTK4.
    const QString scheme = runCommand(QStringLiteral("gsettings"),
                                      {QStringLiteral("get"), QStringLiteral("org.gnome.desktop.interface"),
                                       QStringLiteral("color-scheme")});
    if (scheme.contains(QStringLiteral("dark"), Qt::CaseInsensitive)) {
        m_systemSource = QStringLiteral("gsettings color-scheme");
        return true;
    }
    if (scheme.contains(QStringLiteral("light"), Qt::CaseInsensitive)) {
        m_systemSource = QStringLiteral("gsettings color-scheme");
        return false;
    }
    const QString gtkTheme = runCommand(QStringLiteral("gsettings"),
                                        {QStringLiteral("get"), QStringLiteral("org.gnome.desktop.interface"),
                                         QStringLiteral("gtk-theme")});
    if (!gtkTheme.isEmpty()) {
        m_systemSource = QStringLiteral("gsettings gtk-theme");
        return gtkTheme.contains(QStringLiteral("dark"), Qt::CaseInsensitive);
    }

    // 5) Last resort before the palette: a kdeglobals from an earlier session.
    if (kdeOk) {
        m_systemSource = QStringLiteral("kdeglobals");
        return kdeDark;
    }
#endif

    m_systemSource = QStringLiteral("qt palette");
    return m_systemFallbackDark;
}

} // namespace vx
