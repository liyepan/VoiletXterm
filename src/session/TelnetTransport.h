#pragma once

#include "SessionSpec.h"
#include "Transport.h"

#include <QSet>

class QTcpSocket;

namespace vx {

class TelnetTransport : public Transport
{
    Q_OBJECT
public:
    explicit TelnetTransport(const SessionSpec &spec, QObject *parent = nullptr);

    bool start(int cols, int rows) override;
    void write(const QByteArray &data) override;
    void resizeTerminal(int cols, int rows) override;
    QString description() const override { return m_description; }

private:
    enum State { Data, Iac, Will, Wont, Do, Dont, Sub, SubIac };
    enum Option { OptEcho = 1, OptSga = 3, OptTtype = 24, OptNaws = 31 };

    void onReadyRead();
    void sendCommand(char command, char option);
    void sendWindowSize();

    SessionSpec m_spec;
    QString m_description;
    QTcpSocket *m_socket = nullptr;
    State m_state = Data;
    int m_columns = 80;
    int m_rows = 24;
    bool m_naws = false;
    QSet<int> m_negotiated;
    QByteArray m_pendingOut;
};

} // namespace vx
