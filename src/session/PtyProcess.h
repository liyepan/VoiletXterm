#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QStringList>

class QSocketNotifier;

namespace vx {

// Runs a child process attached to a pseudo terminal, so window size changes
// are propagated and interactive programs (ssh, login shells) behave normally.
//
// POSIX uses forkpty(); Windows uses the ConPTY API (Windows 10 1809+), which
// is resolved at runtime so the binary still starts on older systems.
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
    bool isRunning() const;
    qint64 processId() const;

    QString errorString() const { return m_error; }

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

#ifdef Q_OS_WIN
    struct Private;
    Private *d = nullptr;
#endif

    int m_fd = -1;
    qint64 m_pid = -1;
    QSocketNotifier *m_readNotifier = nullptr;
    QSocketNotifier *m_writeNotifier = nullptr;
    QByteArray m_outBuffer;
    QString m_error;
    bool m_finished = false;
};

} // namespace vx
