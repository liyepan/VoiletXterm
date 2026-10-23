#include "SerialTransport.h"

#include <QSerialPortInfo>

namespace vx {

SerialTransport::SerialTransport(const SessionSpec &spec, QObject *parent)
    : Transport(parent)
    , m_spec(spec)
{
    m_description = spec.summary();
}

QStringList SerialTransport::availablePorts()
{
    QStringList ports;
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        QString label = info.portName();
        if (!info.description().isEmpty())
            label += QStringLiteral(" (%1)").arg(info.description());
        ports << label;
    }
    return ports;
}

bool SerialTransport::start(int cols, int rows)
{
    Q_UNUSED(cols)
    Q_UNUSED(rows)

    if (m_spec.serialPort.trimmed().isEmpty()) {
        emitFinished(tr("未选择串口"));
        return false;
    }

    m_port = new QSerialPort(this);
    m_port->setPortName(m_spec.serialPort);
    m_port->setBaudRate(m_spec.baudRate);

    switch (m_spec.dataBits) {
    case 5: m_port->setDataBits(QSerialPort::Data5); break;
    case 6: m_port->setDataBits(QSerialPort::Data6); break;
    case 7: m_port->setDataBits(QSerialPort::Data7); break;
    default: m_port->setDataBits(QSerialPort::Data8); break;
    }

    if (m_spec.parity == QLatin1String("even"))
        m_port->setParity(QSerialPort::EvenParity);
    else if (m_spec.parity == QLatin1String("odd"))
        m_port->setParity(QSerialPort::OddParity);
    else if (m_spec.parity == QLatin1String("space"))
        m_port->setParity(QSerialPort::SpaceParity);
    else if (m_spec.parity == QLatin1String("mark"))
        m_port->setParity(QSerialPort::MarkParity);
    else
        m_port->setParity(QSerialPort::NoParity);

    if (m_spec.stopBits == 2)
        m_port->setStopBits(QSerialPort::TwoStop);
    else
        m_port->setStopBits(QSerialPort::OneStop);

    if (m_spec.flowControl == QLatin1String("hardware"))
        m_port->setFlowControl(QSerialPort::HardwareControl);
    else if (m_spec.flowControl == QLatin1String("software"))
        m_port->setFlowControl(QSerialPort::SoftwareControl);
    else
        m_port->setFlowControl(QSerialPort::NoFlowControl);

    connect(m_port, &QSerialPort::readyRead, this, [this] {
        emitData(m_port->readAll());
    });
    connect(m_port, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
        if (error == QSerialPort::NoError || error == QSerialPort::TimeoutError)
            return;
        emitFinished(m_port->errorString());
    });

    if (!m_port->open(QIODevice::ReadWrite)) {
        emitFinished(tr("无法打开串口 %1: %2").arg(m_spec.serialPort, m_port->errorString()));
        return false;
    }

    emitConnected();
    return true;
}

void SerialTransport::write(const QByteArray &data)
{
    if (!m_port || !m_port->isOpen())
        return;
    m_port->write(data);
    if (m_spec.localEcho)
        emitData(data);
}

} // namespace vx
