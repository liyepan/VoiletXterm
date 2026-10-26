#include "session/PtyProcess.h"
#include "session/SessionSpec.h"
#include "ui/MainWindow.h"
#include "ui/ThemeManager.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>

namespace {

// Accepts "user@host:port", "telnet://host:port" or "serial:/dev/ttyUSB0@115200".
bool parseTarget(const QString &target, vx::SessionSpec &spec)
{
    if (target.startsWith(QStringLiteral("telnet://"))) {
        QString rest = target.mid(9);
        const int colon = rest.lastIndexOf(QLatin1Char(':'));
        spec.type = vx::SessionSpec::Telnet;
        spec.port = 23;
        if (colon > 0) {
            spec.port = rest.mid(colon + 1).toInt();
            rest = rest.left(colon);
        }
        spec.host = rest;
    } else if (target.startsWith(QStringLiteral("serial:"))) {
        QString rest = target.mid(7);
        const int at = rest.lastIndexOf(QLatin1Char('@'));
        spec.type = vx::SessionSpec::Serial;
        if (at > 0) {
            spec.baudRate = rest.mid(at + 1).toInt();
            rest = rest.left(at);
        }
        spec.serialPort = rest;
    } else {
        QString rest = target;
        spec.type = vx::SessionSpec::Ssh;
        spec.port = 22;
        const int at = rest.indexOf(QLatin1Char('@'));
        if (at > 0) {
            spec.user = rest.left(at);
            rest = rest.mid(at + 1);
        }
        // An explicit ":port" only counts when the host is not IPv6.
        const int colon = rest.lastIndexOf(QLatin1Char(':'));
        if (colon > 0 && rest.count(QLatin1Char(':')) == 1) {
            spec.port = rest.mid(colon + 1).toInt();
            rest = rest.left(colon);
        }
        spec.host = rest;
    }
    if (spec.host.isEmpty() && spec.serialPort.isEmpty())
        return false;
    spec.name = spec.displayName();
    return true;
}

} // namespace

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("VoiletXterm"));
    QCoreApplication::setApplicationName(QStringLiteral("VoiletXterm"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("VoiletXterm - SSH / Telnet / serial terminal"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption themeOption(
        QStringLiteral("theme"),
        QStringLiteral("Colour scheme for this run: system, auto, light or dark."),
        QStringLiteral("scheme"));
    parser.addOption(themeOption);

    QCommandLineOption conPtyOption(
        QStringLiteral("conpty"),
        QStringLiteral("Windows only: force the ConPTY backend (default)."));
    parser.addOption(conPtyOption);
    QCommandLineOption pipesOption(
        QStringLiteral("pipes"),
        QStringLiteral("Windows only: force the pipe backend instead of ConPTY."));
    parser.addOption(pipesOption);

    parser.process(app);

#ifdef Q_OS_WIN
    if (parser.isSet(conPtyOption))
        vx::PtyProcess::setUseConPty(true);
    if (parser.isSet(pipesOption))
        vx::PtyProcess::setUseConPty(false);
    const char *ptyBackend = vx::PtyProcess::useConPty() ? "conpty" : "pipes (-tt)";
#else
    Q_UNUSED(conPtyOption)
    Q_UNUSED(pipesOption)
    const char *ptyBackend = "forkpty";
#endif
    qInfo().noquote() << "VoiletXterm" << QCoreApplication::applicationVersion()
                      << "| pty backend:" << ptyBackend;

    vx::ThemeManager::instance()->initialize();

    // --theme only affects the current run; the saved preference stays intact.
    if (parser.isSet(themeOption)) {
        const QString value = parser.value(themeOption).toLower();
        vx::ThemeManager::Mode mode = vx::ThemeManager::System;
        bool known = true;
        if (value == QLatin1String("auto") || value == QLatin1String("schedule"))
            mode = vx::ThemeManager::Schedule;
        else if (value == QLatin1String("light"))
            mode = vx::ThemeManager::Light;
        else if (value == QLatin1String("dark"))
            mode = vx::ThemeManager::Dark;
        else if (value != QLatin1String("system"))
            known = false;
        if (known)
            vx::ThemeManager::instance()->setMode(mode, false);
    }

    vx::MainWindow window;
    window.show();

    const QStringList targets = parser.positionalArguments();
    if (!targets.isEmpty()) {
        vx::SessionSpec spec;
        if (parseTarget(targets.first(), spec))
            window.openSpec(spec);
    }

    return app.exec();
}
