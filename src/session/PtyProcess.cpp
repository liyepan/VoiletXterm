#include "PtyProcess.h"

#include <QSocketNotifier>

#ifndef Q_OS_UNIX
#include <QDebug>
#endif

#ifdef Q_OS_UNIX
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <pty.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#endif

namespace vx {

PtyProcess::PtyProcess(QObject *parent)
    : QObject(parent)
{
}

PtyProcess::~PtyProcess()
{
    stop();
}

bool PtyProcess::start(const QString &program, const QStringList &args,
                       const QHash<QString, QString> &environment, int cols, int rows)
{
#ifdef Q_OS_UNIX
    if (m_fd >= 0)
        return false;

    struct winsize ws;
    std::memset(&ws, 0, sizeof(ws));
    ws.ws_col = ushort(cols);
    ws.ws_row = ushort(rows);

    int master = -1;
    const pid_t pid = forkpty(&master, nullptr, nullptr, &ws);
    if (pid < 0)
        return false;

    if (pid == 0) {
        setenv("TERM", "xterm-256color", 1);
        setenv("COLORTERM", "truecolor", 1);
        setenv("TERM_PROGRAM", "VoiletXterm", 1);
        for (auto it = environment.constBegin(); it != environment.constEnd(); ++it)
            setenv(it.key().toLocal8Bit().constData(), it.value().toLocal8Bit().constData(), 1);

        std::vector<char *> argv;
        QByteArray programBytes = program.toLocal8Bit();
        argv.push_back(programBytes.data());
        for (const QString &arg : args)
            argv.push_back(strdup(arg.toLocal8Bit().constData()));
        argv.push_back(nullptr);

        execvp(programBytes.constData(), argv.data());
        _exit(127);
    }

    m_fd = master;
    m_pid = pid;
    fcntl(m_fd, F_SETFL, fcntl(m_fd, F_GETFL) | O_NONBLOCK);

    m_readNotifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
    connect(m_readNotifier, &QSocketNotifier::activated, this, &PtyProcess::onReadable);
    return true;
#else
    Q_UNUSED(program)
    Q_UNUSED(args)
    Q_UNUSED(environment)
    Q_UNUSED(cols)
    Q_UNUSED(rows)
    return false;
#endif
}

void PtyProcess::onReadable()
{
#ifdef Q_OS_UNIX
    char buffer[65536];
    for (;;) {
        const ssize_t n = ::read(m_fd, buffer, sizeof(buffer));
        if (n > 0) {
            emit dataRead(QByteArray(buffer, int(n)));
        } else if (n == 0) {
            finish(0, QStringLiteral("会话已结束"));
            return;
        } else {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            finish(-1, QStringLiteral("连接已断开"));
            return;
        }
    }
#endif
}

void PtyProcess::write(const QByteArray &data)
{
    if (m_fd < 0 || data.isEmpty())
        return;
    m_outBuffer.append(data);
    flushOutput();
}

void PtyProcess::flushOutput()
{
#ifdef Q_OS_UNIX
    while (!m_outBuffer.isEmpty() && m_fd >= 0) {
        const ssize_t n = ::write(m_fd, m_outBuffer.constData(), size_t(m_outBuffer.size()));
        if (n > 0) {
            m_outBuffer.remove(0, int(n));
        } else if (n < 0 && errno == EINTR) {
            continue;
        } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            if (!m_writeNotifier) {
                m_writeNotifier = new QSocketNotifier(m_fd, QSocketNotifier::Write, this);
                connect(m_writeNotifier, &QSocketNotifier::activated, this, &PtyProcess::onWritable);
            }
            return;
        } else {
            break;
        }
    }
    if (m_writeNotifier && m_outBuffer.isEmpty()) {
        m_writeNotifier->setEnabled(false);
    }
#endif
}

void PtyProcess::onWritable()
{
    flushOutput();
}

void PtyProcess::setWindowSize(int cols, int rows)
{
#ifdef Q_OS_UNIX
    if (m_fd < 0)
        return;
    struct winsize ws;
    std::memset(&ws, 0, sizeof(ws));
    ws.ws_col = ushort(cols);
    ws.ws_row = ushort(rows);
    ioctl(m_fd, TIOCSWINSZ, &ws);
#else
    Q_UNUSED(cols)
    Q_UNUSED(rows)
#endif
}

void PtyProcess::closeFd()
{
    if (m_readNotifier) {
        m_readNotifier->setEnabled(false);
        delete m_readNotifier;
        m_readNotifier = nullptr;
    }
    if (m_writeNotifier) {
        m_writeNotifier->setEnabled(false);
        delete m_writeNotifier;
        m_writeNotifier = nullptr;
    }
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

void PtyProcess::finish(int exitCode, const QString &reason)
{
    if (m_finished)
        return;
    m_finished = true;
    closeFd();

#ifdef Q_OS_UNIX
    if (m_pid > 0) {
        int status = 0;
        for (int i = 0; i < 20; ++i) {
            const pid_t r = waitpid(pid_t(m_pid), &status, WNOHANG);
            if (r == pid_t(m_pid) || (r < 0 && errno == ECHILD))
                break;
            usleep(10000);
        }
        if (WIFEXITED(status) && exitCode < 0)
            exitCode = WEXITSTATUS(status);
    }
#endif
    emit closed(exitCode, reason);
}

void PtyProcess::stop()
{
    if (m_fd < 0 && m_pid <= 0)
        return;
    closeFd();
#ifdef Q_OS_UNIX
    if (m_pid > 0) {
        ::kill(pid_t(m_pid), SIGHUP);
        int status = 0;
        const pid_t r = waitpid(pid_t(m_pid), &status, 0);
        if (r < 0) {
            ::kill(pid_t(m_pid), SIGKILL);
            waitpid(pid_t(m_pid), &status, 0);
        }
    }
#endif
    m_pid = -1;
    m_finished = true;
}

} // namespace vx
