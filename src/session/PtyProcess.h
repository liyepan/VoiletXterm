#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QStringList>

class QProcess;
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

    // Windows: ConPTY does not hand the child's standard handles over to the
    // pseudo console on every system, so the default backend uses plain pipes
    // and ssh -tt. Opt into ConPTY explicitly when experimenting.
    static void setUseConPty(bool enabled);
    static bool useConPty();

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

    // ConPTY teardown (Windows only, no-op elsewhere).
    void teardownConPty();
    // Pipe based backend. It is compiled on every platform so the code is
    // covered by the normal builds; only Windows selects it by default.
    void startPipeFallback();

    QProcess *m_pipeProcess = nullptr;
    QString m_program;
    QStringList m_args;
    QHash<QString, QString> m_environment;

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
    bool m_loggedFirstData = false;
    qint64 m_totalBytes = 0;
};

} // namespace vx
