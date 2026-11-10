#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace vx {

struct SessionSpec {
    enum Type { Ssh = 0, Telnet = 1, Serial = 2 };

    Type type = Ssh;
    QString name;

    // SSH / Telnet
    QString host;
    int port = 22;
    QString user;
    QString keyFile;
    QString jumpHost;
    QString remoteCommand;
    QStringList extraOptions;

    // Never serialised: the password lives in the credential store.
    QString password;
    bool savePassword = false;

    // serial
    QString serialPort;
    int baudRate = 115200;
    int dataBits = 8;
    int stopBits = 1;
    QString parity = QStringLiteral("none");
    QString flowControl = QStringLiteral("none");
    bool localEcho = false;

    // Stable key used by the credential store, e.g. "ssh://user@host:22".
    QString credentialKey() const;
    QString displayName() const;
    QString summary() const;
    QJsonObject toJson() const;
    static SessionSpec fromJson(const QJsonObject &object);
};

} // namespace vx
