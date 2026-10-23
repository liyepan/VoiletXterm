#pragma once

#include "SessionSpec.h"
#include "Transport.h"

#include <QSerialPort>

namespace vx {

class SerialTransport : public Transport
{
    Q_OBJECT
public:
    explicit SerialTransport(const SessionSpec &spec, QObject *parent = nullptr);

    bool start(int cols, int rows) override;
    void write(const QByteArray &data) override;
    QString description() const override { return m_description; }

    static QStringList availablePorts();

private:
    SessionSpec m_spec;
    QString m_description;
    QSerialPort *m_port = nullptr;
};

} // namespace vx
