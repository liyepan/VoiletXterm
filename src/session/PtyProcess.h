#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QStringList>

class QSocketNotifier;

namespace vx {

// Runs a child process attached to a pseudo terminal, so window size changes
// are propagated and interactive programs (ssh, login shells) behave normally.
class PtyProcess : public QObject
{
    Q_OBJECT
public:
    explicit PtyProcess(QObject *parent = nullptr);
    ~PtyProcess() override;

    bool start(const QString &program, const QStringList &args,
               const QHash<QString, QString> &environment, int cols, int rows);
    void write(const QByteArray &data);
    void setWindowSize(int cols, int rows);
    void stop();
    bool isRunning() const { return m_fd >= 0; }
    qint64 processId() const { return m_pid; }

signals:
    void dataRead(const QByteArray &data);
    void closed(int exitCode, const QString &reason);

private slots:
    void onReadable();
    void onWritable();

private:
    void flushOutput();
    void finish(int exitCode, const QString &reason);
    void closeFd();

    int m_fd = -1;
    qint64 m_pid = -1;
    QSocketNotifier *m_readNotifier = nullptr;
    QSocketNotifier *m_writeNotifier = nullptr;
    QByteArray m_outBuffer;
    bool m_finished = false;
};

} // namespace vx
