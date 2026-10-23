#include "TelnetTransport.h"

#include <QTcpSocket>

namespace vx {

namespace {
constexpr char IAC = char(0xFF);
constexpr char DONT = char(0xFE);
constexpr char DO = char(0xFD);
constexpr char WONT = char(0xFC);
constexpr char WILL = char(0xFB);
constexpr char SB = char(0xFA);
constexpr char SE = char(0xF0);
} // namespace

TelnetTransport::TelnetTransport(const SessionSpec &spec, QObject *parent)
    : Transport(parent)
    , m_spec(spec)
{
    m_description = spec.summary();
}

bool TelnetTransport::start(int cols, int rows)
{
    if (m_spec.host.trimmed().isEmpty()) {
        emitFinished(tr("未填写主机地址"));
        return false;
    }
    m_columns = cols;
    m_rows = rows;

    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected, this, [this] { emitConnected(); });
    connect(m_socket, &QTcpSocket::readyRead, this, &TelnetTransport::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, [this] {
        emitFinished(tr("连接已关闭"));
    });
    connect(m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        emitFinished(m_socket->errorString());
    });

    const int port = m_spec.port > 0 ? m_spec.port : 23;
    m_socket->connectToHost(m_spec.host, quint16(port));
    return true;
}

void TelnetTransport::sendCommand(char command, char option)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState)
        return;
    QByteArray data;
    data.append(IAC);
    data.append(command);
    data.append(option);
    m_socket->write(data);
}

void TelnetTransport::sendWindowSize()
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState)
        return;
    QByteArray data;
    data.append(IAC);
    data.append(SB);
    data.append(char(OptNaws));
    data.append(char((m_columns >> 8) & 0xFF));
    data.append(char(m_columns & 0xFF));
    data.append(char((m_rows >> 8) & 0xFF));
    data.append(char(m_rows & 0xFF));
    data.append(IAC);
    data.append(SE);
    m_socket->write(data);
}

void TelnetTransport::onReadyRead()
{
    const QByteArray chunk = m_socket->readAll();
    QByteArray out;
    out.reserve(chunk.size());

    for (char raw : chunk) {
        const unsigned char b = static_cast<unsigned char>(raw);
        switch (m_state) {
        case Data:
            if (b == 0xFF)
                m_state = Iac;
            else if (b == 0x00)
                ; // NUL after CR is padding
            else
                out.append(raw);
            break;
        case Iac:
            switch (b) {
            case 0xFF:
                out.append(char(0xFF));
                m_state = Data;
                break;
            case 0xFB:
                m_state = Will;
                break;
            case 0xFC:
                m_state = Wont;
                break;
            case 0xFD:
                m_state = Do;
                break;
            case 0xFE:
                m_state = Dont;
                break;
            case 0xFA:
                m_state = Sub;
                break;
            default:
                m_state = Data; // NOP, GA, AYT, DM, BRK ...
                break;
            }
            break;
        case Will: {
            const int option = b;
            if (m_negotiated.contains(option)) {
                m_state = Data;
                break;
            }
            m_negotiated.insert(option);
            if (option == OptEcho || option == OptSga) {
                sendCommand(DO, char(option));
            } else if (option == OptNaws) {
                sendCommand(DO, char(option));
                m_naws = true;
                sendWindowSize();
            } else {
                sendCommand(DONT, char(option));
            }
            m_state = Data;
            break;
        }
        case Do: {
            const int option = b;
            if (m_negotiated.contains(option)) {
                m_state = Data;
                break;
            }
            m_negotiated.insert(option);
            if (option == OptNaws) {
                sendCommand(WILL, char(option));
                m_naws = true;
                sendWindowSize();
            } else if (option == OptSga || option == OptTtype) {
                sendCommand(WILL, char(option));
            } else {
                sendCommand(WONT, char(option));
            }
            m_state = Data;
            break;
        }
        case Wont:
        case Dont:
            m_state = Data;
            break;
        case Sub:
            if (b == 0xFF)
                m_state = SubIac;
            break;
        case SubIac:
            m_state = (b == 0xF0) ? Data : Sub;
            break;
        }
    }

    if (!out.isEmpty())
        emitData(out);
}

void TelnetTransport::write(const QByteArray &data)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState)
        return;
    QByteArray escaped;
    escaped.reserve(data.size());
    for (char c : data) {
        escaped.append(c);
        if (static_cast<unsigned char>(c) == 0xFF)
            escaped.append(IAC);
    }
    m_socket->write(escaped);
}

void TelnetTransport::resizeTerminal(int cols, int rows)
{
    m_columns = cols;
    m_rows = rows;
    if (m_naws && m_socket && m_socket->state() == QAbstractSocket::ConnectedState)
        sendWindowSize();
}

} // namespace vx
