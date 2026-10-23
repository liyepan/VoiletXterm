#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

namespace vx {

// Abstract byte pipe between a terminal widget and a remote device.
class Transport : public QObject
{
    Q_OBJECT
public:
    explicit Transport(QObject *parent = nullptr) : QObject(parent) {}
    ~Transport() override = default;

    virtual bool start(int cols, int rows) = 0;
    virtual void write(const QByteArray &data) = 0;
    virtual void resizeTerminal(int cols, int rows) { Q_UNUSED(cols) Q_UNUSED(rows) }
    virtual QString description() const = 0;
    virtual bool isConnected() const { return m_connected; }

    void emitConnected() { m_connected = true; emit connected(); }
    void emitData(const QByteArray &data) { emit dataReceived(data); }
    void emitFinished(const QString &reason)
    {
        m_connected = false;
        emit finished(reason);
    }

signals:
    void dataReceived(const QByteArray &data);
    void connected();
    void finished(const QString &reason);

private:
    bool m_connected = false;
};

} // namespace vx
