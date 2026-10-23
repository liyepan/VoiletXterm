#include "SessionSpec.h"

#include <QJsonValue>

namespace vx {

QString SessionSpec::displayName() const
{
    if (!name.isEmpty())
        return name;
    if (type == Serial)
        return serialPort.isEmpty() ? QStringLiteral("串口") : serialPort;
    const QString hostPart = user.isEmpty() ? host : QStringLiteral("%1@%2").arg(user, host);
    if (type == Telnet)
        return QStringLiteral("telnet://%1:%2").arg(host).arg(port);
    return hostPart.isEmpty() ? QStringLiteral("SSH") : hostPart;
}

QString SessionSpec::summary() const
{
    switch (type) {
    case Ssh:
        return QStringLiteral("SSH %1:%2").arg(host).arg(port);
    case Telnet:
        return QStringLiteral("Telnet %1:%2").arg(host).arg(port);
    case Serial: {
        QString p = serialPort.isEmpty() ? QStringLiteral("?") : serialPort;
        return QStringLiteral("串口 %1 @ %2").arg(p).arg(baudRate);
    }
    }
    return QString();
}

QJsonObject SessionSpec::toJson() const
{
    QJsonObject o;
    o.insert(QStringLiteral("type"), int(type));
    o.insert(QStringLiteral("name"), name);
    o.insert(QStringLiteral("host"), host);
    o.insert(QStringLiteral("port"), port);
    o.insert(QStringLiteral("user"), user);
    o.insert(QStringLiteral("keyFile"), keyFile);
    o.insert(QStringLiteral("jumpHost"), jumpHost);
    o.insert(QStringLiteral("remoteCommand"), remoteCommand);
    o.insert(QStringLiteral("extraOptions"), extraOptions.join(QLatin1Char(' ')));
    o.insert(QStringLiteral("serialPort"), serialPort);
    o.insert(QStringLiteral("baudRate"), baudRate);
    o.insert(QStringLiteral("dataBits"), dataBits);
    o.insert(QStringLiteral("stopBits"), stopBits);
    o.insert(QStringLiteral("parity"), parity);
    o.insert(QStringLiteral("flowControl"), flowControl);
    o.insert(QStringLiteral("localEcho"), localEcho);
    return o;
}

SessionSpec SessionSpec::fromJson(const QJsonObject &o)
{
    SessionSpec s;
    s.type = Type(qBound(0, o.value(QStringLiteral("type")).toInt(), 2));
    s.name = o.value(QStringLiteral("name")).toString();
    s.host = o.value(QStringLiteral("host")).toString();
    s.port = o.value(QStringLiteral("port")).toInt(s.type == Telnet ? 23 : 22);
    s.user = o.value(QStringLiteral("user")).toString();
    s.keyFile = o.value(QStringLiteral("keyFile")).toString();
    s.jumpHost = o.value(QStringLiteral("jumpHost")).toString();
    s.remoteCommand = o.value(QStringLiteral("remoteCommand")).toString();
    const QString extras = o.value(QStringLiteral("extraOptions")).toString();
    if (!extras.isEmpty())
        s.extraOptions = extras.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    s.serialPort = o.value(QStringLiteral("serialPort")).toString();
    s.baudRate = o.value(QStringLiteral("baudRate")).toInt(115200);
    s.dataBits = o.value(QStringLiteral("dataBits")).toInt(8);
    s.stopBits = o.value(QStringLiteral("stopBits")).toInt(1);
    s.parity = o.value(QStringLiteral("parity")).toString(QStringLiteral("none"));
    s.flowControl = o.value(QStringLiteral("flowControl")).toString(QStringLiteral("none"));
    s.localEcho = o.value(QStringLiteral("localEcho")).toBool(false);
    return s;
}

} // namespace vx
