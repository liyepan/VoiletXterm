// MinGW defaults _WIN32_WINNT to 0x0502 (Windows XP), which hides every
// Vista+ declaration in <windows.h> - including STARTUPINFOEXW and the process
// attribute list API used below. This must happen before any header is pulled
// in, otherwise <windows.h> is already parsed with the old gate.
#if defined(_WIN32) || defined(__MINGW32__)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef WINVER
#define WINVER 0x0601
#endif
#endif

#include "PtyProcess.h"

#include <QDebug>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSocketNotifier>
#include <QTimer>

#include <algorithm>

#ifdef Q_OS_WIN

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <QDebug>

#include <atomic>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

// ConPTY attributes are Windows 10 only; older SDK headers never declare them.
#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE ((DWORD_PTR)0x00020016)
#endif

// Belt and braces: if a toolchain still ends up with _WIN32_WINNT < 0x0600,
// declare the process attribute list API ourselves instead of failing to build.
#if !defined(_WIN32_WINNT) || (_WIN32_WINNT < 0x0600)
typedef struct _PROC_THREAD_ATTRIBUTE_LIST *PPROC_THREAD_ATTRIBUTE_LIST;
typedef PPROC_THREAD_ATTRIBUTE_LIST LPPROC_THREAD_ATTRIBUTE_LIST;
typedef struct _STARTUPINFOEXW {
    STARTUPINFOW StartupInfo;
    LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList;
} STARTUPINFOEXW, *LPSTARTUPINFOEXW;
#ifndef EXTENDED_STARTUPINFO_PRESENT
#define EXTENDED_STARTUPINFO_PRESENT 0x00080000
#endif
extern "C" {
WINBASEAPI BOOL WINAPI InitializeProcThreadAttributeList(LPPROC_THREAD_ATTRIBUTE_LIST, DWORD, DWORD, PSIZE_T);
WINBASEAPI BOOL WINAPI UpdateProcThreadAttribute(LPPROC_THREAD_ATTRIBUTE_LIST, DWORD, DWORD_PTR, PVOID, SIZE_T, PVOID, PSIZE_T);
WINBASEAPI VOID WINAPI DeleteProcThreadAttributeList(LPPROC_THREAD_ATTRIBUTE_LIST);
}
#endif

#else // Q_OS_UNIX

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>

#ifdef Q_OS_MACOS
#include <util.h>
#else
#include <pty.h>
#endif

#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#endif

namespace vx {

#ifdef Q_OS_WIN

namespace {

// ConPTY entry points. They only exist on Windows 10 1809+, so they are
// resolved at runtime instead of being imported at link time.
using CreatePseudoConsoleFn = HRESULT(WINAPI *)(COORD, HANDLE, HANDLE, DWORD, void **);
using ResizePseudoConsoleFn = HRESULT(WINAPI *)(void *, COORD);
using ClosePseudoConsoleFn = void(WINAPI *)(void *);

CreatePseudoConsoleFn g_createPseudoConsole = nullptr;
ResizePseudoConsoleFn g_resizePseudoConsole = nullptr;
ClosePseudoConsoleFn g_closePseudoConsole = nullptr;
bool g_conPtyChecked = false;

// Function pointer <-> data pointer casts are not portable in C++; round trip
// them through memcpy so the code stays warning clean.
template <typename FunctionPointer>
FunctionPointer resolveSymbol(HMODULE module, const char *name)
{
    FARPROC symbol = GetProcAddress(module, name);
    FunctionPointer result = nullptr;
    std::memcpy(&result, &symbol, sizeof(result));
    return result;
}

bool loadConPty()
{
    if (g_conPtyChecked)
        return g_createPseudoConsole != nullptr;
    g_conPtyChecked = true;

    HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
    if (!kernel)
        return false;

    g_createPseudoConsole = resolveSymbol<CreatePseudoConsoleFn>(kernel, "CreatePseudoConsole");
    g_resizePseudoConsole = resolveSymbol<ResizePseudoConsoleFn>(kernel, "ResizePseudoConsole");
    g_closePseudoConsole = resolveSymbol<ClosePseudoConsoleFn>(kernel, "ClosePseudoConsole");

    if (!g_createPseudoConsole || !g_resizePseudoConsole || !g_closePseudoConsole) {
        g_createPseudoConsole = nullptr;
        g_resizePseudoConsole = nullptr;
        g_closePseudoConsole = nullptr;
        return false;
    }
    return true;
}

// Quote one argument following the CommandLineToArgvW rules.
QString quoteArgument(const QString &argument)
{
    if (!argument.isEmpty() && !argument.contains(QLatin1Char(' '))
        && !argument.contains(QLatin1Char('\t')) && !argument.contains(QLatin1Char('"'))
        && !argument.contains(QLatin1Char('\n'))) {
        return argument;
    }

    QString quoted;
    quoted.reserve(argument.size() + 2);
    quoted += QLatin1Char('"');

    int backslashes = 0;
    for (const QChar ch : argument) {
        if (ch == QLatin1Char('\\')) {
            ++backslashes;
            continue;
        }
        if (ch == QLatin1Char('"')) {
            quoted += QString(backslashes * 2 + 1, QChar('\\'));
            quoted += QChar('"');
            backslashes = 0;
            continue;
        }
        if (backslashes > 0) {
            quoted += QString(backslashes, QChar('\\'));
            backslashes = 0;
        }
        quoted += ch;
    }
    if (backslashes > 0)
        quoted += QString(backslashes * 2, QChar('\\'));
    quoted += QChar('"');
    return quoted;
}

QByteArray buildEnvironmentBlock(const QHash<QString, QString> &extra)
{
    QHash<QString, QString> values;
    LPWCH block = GetEnvironmentStringsW();
    if (block) {
        for (LPWCH cursor = block; *cursor; cursor += wcslen(cursor) + 1) {
            const QString entry = QString::fromWCharArray(cursor);
            const int eq = entry.indexOf(QLatin1Char('='));
            if (eq <= 0)
                continue;
            values.insert(entry.left(eq).toUpper(), entry.mid(eq + 1));
        }
        FreeEnvironmentStringsW(block);
    }
    for (auto it = extra.constBegin(); it != extra.constEnd(); ++it)
        values.insert(it.key().toUpper(), it.value());

    QString joined;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        joined += it.key();
        joined += QLatin1Char('=');
        joined += it.value();
        joined += QChar(ushort(0));
    }
    joined += QChar(ushort(0));

    return QByteArray(reinterpret_cast<const char *>(joined.utf16()),
                      joined.size() * int(sizeof(ushort)));
}

} // namespace

struct PtyProcess::Private {
    void *pty = nullptr;
    HANDLE process = nullptr;
    HANDLE inputWrite = nullptr;
    HANDLE outputRead = nullptr;
    DWORD pid = 0;
    std::atomic<bool> stop{false};
    std::thread reader;
};

#else // ------------------------------------------------------------- POSIX

namespace {

bool isExecutableOnPath(const QString &program)
{
    const QByteArray name = program.toLocal8Bit();
    const QByteArray path = qgetenv("PATH");
    for (const QByteArray &dir : path.split(':')) {
        if (dir.isEmpty())
            continue;
        const QByteArray candidate = dir + '/' + name;
        if (::access(candidate.constData(), X_OK) == 0)
            return true;
    }
    return false;
}

} // namespace

#endif

namespace {
bool g_useConPty = false;
}

void PtyProcess::setUseConPty(bool enabled)
{
    g_useConPty = enabled;
}

bool PtyProcess::useConPty()
{
    return g_useConPty;
}

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
#ifdef Q_OS_WIN

    if (d || m_pipeProcess)
        return false;

    m_program = program;
    m_args = args;
    m_environment = environment;

    // Default on Windows: plain pipes plus "ssh -tt". ConPTY attaches a
    // console but does not reliably give the child our pipe handles, which
    // leaves ssh writing into the parent's stdout.
    if (!g_useConPty) {
        startPipeFallback();
        return true;
    }

    if (!loadConPty()) {
        m_error = tr("当前系统不支持 ConPTY，需要 Windows 10 1809 或更新版本");
        return false;
    }

    d = new Private;

    HANDLE inputRead = nullptr;
    HANDLE inputWrite = nullptr;
    HANDLE outputRead = nullptr;
    HANDLE outputWrite = nullptr;
    if (!CreatePipe(&inputRead, &inputWrite, nullptr, 0)
        || !CreatePipe(&outputRead, &outputWrite, nullptr, 0)) {
        m_error = tr("创建管道失败");
        if (inputRead)
            CloseHandle(inputRead);
        if (inputWrite)
            CloseHandle(inputWrite);
        delete d;
        d = nullptr;
        return false;
    }

    COORD size;
    size.X = static_cast<SHORT>(std::max(1, cols));
    size.Y = static_cast<SHORT>(std::max(1, rows));

    const HRESULT created = g_createPseudoConsole(size, inputRead, outputWrite, 0, &d->pty);
    qInfo().noquote() << "[pty] CreatePseudoConsole hr=" << Qt::hex << quint32(created)
                      << "handle=" << (quintptr)d->pty;
    if (FAILED(created)) {
        m_error = tr("创建 ConPTY 失败（错误码 %1）").arg(qint32(created), 0, 16);
        CloseHandle(inputRead);
        CloseHandle(inputWrite);
        CloseHandle(outputRead);
        CloseHandle(outputWrite);
        delete d;
        d = nullptr;
        return false;
    }

    d->inputWrite = inputWrite;
    d->outputRead = outputRead;

    qInfo().noquote() << "[pty] conpty start |" << program << args.join(QLatin1Char(' '));

    STARTUPINFOEXW startup;
    ZeroMemory(&startup, sizeof(startup));
    startup.StartupInfo.cb = sizeof(STARTUPINFOEXW);

    // Any failure below has to release everything that was allocated so far.
    const auto abortStart = [&](const QString &message) {
        m_error = message;
        if (startup.lpAttributeList) {
            DeleteProcThreadAttributeList(startup.lpAttributeList);
            HeapFree(GetProcessHeap(), 0, startup.lpAttributeList);
        }
        CloseHandle(inputRead);
        CloseHandle(outputWrite);
        CloseHandle(inputWrite);
        CloseHandle(outputRead);
        if (d->pty)
            g_closePseudoConsole(d->pty);
        d->inputWrite = nullptr;
        d->outputRead = nullptr;
        d->pty = nullptr;
        delete d;
        d = nullptr;
        return false;
    };

    SIZE_T attributeSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeSize);
    startup.lpAttributeList =
        static_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(HeapAlloc(GetProcessHeap(), 0, attributeSize));
    if (!startup.lpAttributeList
        || !InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0, &attributeSize)) {
        return abortStart(tr("初始化进程属性失败（错误码 %1）").arg(int(GetLastError())));
    }

    if (!UpdateProcThreadAttribute(startup.lpAttributeList, 0,
                                   PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, d->pty,
                                   sizeof(d->pty), nullptr, nullptr)) {
        const DWORD error = GetLastError();
        qWarning().noquote() << "[pty] UpdateProcThreadAttribute failed, error=" << error;
        return abortStart(tr("把伪终端挂到子进程失败（错误码 %1）").arg(int(error)));
    }
    qInfo().noquote() << "[pty] attribute list ready";

    QString commandLine = quoteArgument(program);
    for (const QString &argument : args)
        commandLine += QLatin1Char(' ') + quoteArgument(argument);
    std::wstring mutableCommandLine = commandLine.toStdWString();
    QByteArray environmentBlock = buildEnvironmentBlock(environment);

    PROCESS_INFORMATION processInfo;
    ZeroMemory(&processInfo, sizeof(processInfo));
    const BOOL created2 = CreateProcessW(
        nullptr, mutableCommandLine.empty() ? nullptr : &mutableCommandLine[0],
        nullptr, nullptr, FALSE,
        EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
        environmentBlock.isEmpty() ? nullptr : environmentBlock.data(),
        nullptr, &startup.StartupInfo, &processInfo);

    DeleteProcThreadAttributeList(startup.lpAttributeList);
    HeapFree(GetProcessHeap(), 0, startup.lpAttributeList);
    startup.lpAttributeList = nullptr;

    if (!created2) {
        const DWORD error = GetLastError();
        return abortStart(tr("无法启动 %1（错误码 %2）").arg(program).arg(int(error)));
    }

    // The client owns its own copy of the console ends now, so the ones handed
    // to CreatePseudoConsole are no longer needed. Closing them earlier made
    // Windows fall back to the parent console ("stdin is not a terminal").
    CloseHandle(inputRead);
    CloseHandle(outputWrite);

    qInfo().noquote() << "[pty] CreateProcess ok, pid=" << processInfo.dwProcessId;

    CloseHandle(processInfo.hThread);
    d->process = processInfo.hProcess;
    d->pid = processInfo.dwProcessId;

    // Diagnostics: if the child shows up in the parent console's process list,
    // the pseudo console was not attached and ssh is talking to the console.
    QTimer::singleShot(1500, this, [this] {
        if (!d)
            return;
        DWORD pids[128];
        const DWORD attached = GetConsoleProcessList(pids, sizeof(pids) / sizeof(pids[0]));
        bool childOnParentConsole = false;
        for (DWORD i = 0; i < attached && i < 128; ++i) {
            if (pids[i] == d->pid)
                childOnParentConsole = true;
        }
        qInfo().noquote() << "[pty] 1.5s: bytes=" << m_totalBytes
                          << "console procs=" << int(attached)
                          << "child on parent console=" << childOnParentConsole;
    });

    QTimer::singleShot(2500, this, [this] {
        if (m_finished || !d || m_totalBytes > 64)
            return;
        qWarning().noquote() << "[pty] ConPTY delivered nothing, switching to pipes";
        teardownConPty();
        startPipeFallback();
    });

    // A blocking ReadFile cannot be cancelled cleanly, so the reader polls the
    // pipe instead; 4 ms is well below the perceptible latency of a terminal.
    d->reader = std::thread([this] {
        Private *state = d;
        if (!state)
            return;
        char buffer[65536];
        for (;;) {
            if (state->stop)
                return;
            DWORD available = 0;
            if (!PeekNamedPipe(state->outputRead, nullptr, 0, nullptr, &available, nullptr))
                break;
            if (available == 0) {
                Sleep(4);
                continue;
            }
            const DWORD want = available > sizeof(buffer) ? DWORD(sizeof(buffer)) : available;
            DWORD read = 0;
            if (!ReadFile(state->outputRead, buffer, want, &read, nullptr) || read == 0)
                break;
            m_totalBytes += qint64(read);
            if (!m_loggedFirstData) {
                m_loggedFirstData = true;
                qInfo().noquote() << "[pty] first data" << int(read) << "bytes:"
                                  << QByteArray(buffer, int(qMin<DWORD>(read, 120u))).toPercentEncoding();
            }
            const QByteArray chunk(buffer, int(read));
            QMetaObject::invokeMethod(this, [this, chunk] { emit dataRead(chunk); },
                                      Qt::QueuedConnection);
        }
        if (state->stop)
            return;
        QMetaObject::invokeMethod(this, [this] { finish(0, tr("会话已结束")); },
                                  Qt::QueuedConnection);
    });
    return true;

#else // ------------------------------------------------------------- POSIX

    if (m_fd >= 0)
        return false;

    if (program.isEmpty()) {
        m_error = tr("未指定要执行的程序");
        return false;
    }
    if (program.contains(QLatin1Char('/'))) {
        if (::access(program.toLocal8Bit().constData(), X_OK) != 0) {
            m_error = tr("找不到可执行文件 %1").arg(program);
            return false;
        }
    } else if (!isExecutableOnPath(program)) {
        m_error = tr("在 PATH 中找不到 %1").arg(program);
        return false;
    }

    struct winsize ws;
    std::memset(&ws, 0, sizeof(ws));
    ws.ws_col = ushort(std::max(1, cols));
    ws.ws_row = ushort(std::max(1, rows));

    int master = -1;
    const pid_t pid = forkpty(&master, nullptr, nullptr, &ws);
    if (pid < 0) {
        m_error = tr("创建伪终端失败");
        return false;
    }

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
    qInfo().noquote() << "[pty] forkpty pid=" << qint64(pid) << program << args.join(QLatin1Char(' '));
    fcntl(m_fd, F_SETFL, fcntl(m_fd, F_GETFL) | O_NONBLOCK);

    m_readNotifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
    connect(m_readNotifier, &QSocketNotifier::activated, this, &PtyProcess::onReadable);
    return true;

#endif
}

void PtyProcess::onReadable()
{
#ifndef Q_OS_WIN
    char buffer[65536];
    for (;;) {
        const ssize_t n = ::read(m_fd, buffer, sizeof(buffer));
        if (n > 0) {
            if (!m_loggedFirstData) {
                m_loggedFirstData = true;
                qInfo().noquote() << "[pty] first data" << int(n) << "bytes";
            }
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

void PtyProcess::teardownConPty()
{
#ifdef Q_OS_WIN
    if (!d)
        return;
    d->stop = true;
    if (d->reader.joinable())
        d->reader.join();
    if (d->inputWrite) {
        CloseHandle(d->inputWrite);
        d->inputWrite = nullptr;
    }
    if (d->outputRead) {
        CloseHandle(d->outputRead);
        d->outputRead = nullptr;
    }
    if (d->process) {
        TerminateProcess(d->process, 0);
        CloseHandle(d->process);
        d->process = nullptr;
    }
    if (d->pty) {
        g_closePseudoConsole(d->pty);
        d->pty = nullptr;
    }
    delete d;
    d = nullptr;
#endif
}

void PtyProcess::startPipeFallback()
{
#ifdef Q_OS_WIN
    if (m_pipeProcess)
        return;

    m_pipeProcess = new QProcess(this);
    m_pipeProcess->setProcessChannelMode(QProcess::MergedChannels);

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("TERM"), QStringLiteral("xterm-256color"));
    environment.insert(QStringLiteral("COLORTERM"), QStringLiteral("truecolor"));
    environment.insert(QStringLiteral("TERM_PROGRAM"), QStringLiteral("VoiletXterm"));
    for (auto it = m_environment.constBegin(); it != m_environment.constEnd(); ++it)
        environment.insert(it.key(), it.value());
    m_pipeProcess->setEnvironment(environment);

    connect(m_pipeProcess, &QProcess::readyReadStandardOutput, this, [this] {
        const QByteArray chunk = m_pipeProcess->readAllStandardOutput();
        if (chunk.isEmpty())
            return;
        m_totalBytes += chunk.size();
        if (!m_loggedFirstData) {
            m_loggedFirstData = true;
            qInfo().noquote() << "[pty] first data" << chunk.size() << "bytes:"
                              << chunk.left(120).toPercentEncoding();
        }
        emit dataRead(chunk);
    });
    connect(m_pipeProcess, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus) {
        finish(exitCode, tr("会话已结束"));
    });
    connect(m_pipeProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            finish(-1, tr("无法启动 %1").arg(m_program));
    });

    qInfo().noquote() << "[pty] pipe backend |" << m_program << m_args.join(QLatin1Char(' '));
    m_pipeProcess->start(m_program, m_args);
#endif
}

void PtyProcess::write(const QByteArray &data)
{
    if (data.isEmpty())
        return;

#ifdef Q_OS_WIN

    if (m_pipeProcess) {
        if (m_pipeProcess->state() == QProcess::Running)
            m_pipeProcess->write(data);
        return;
    }
    if (!d || !d->inputWrite)
        return;
    const char *cursor = data.constData();
    int remaining = data.size();
    while (remaining > 0) {
        DWORD written = 0;
        if (!WriteFile(d->inputWrite, cursor, DWORD(remaining), &written, nullptr) || written == 0)
            break;
        cursor += written;
        remaining -= int(written);
    }

#else

    if (m_fd < 0)
        return;
    m_outBuffer.append(data);
    flushOutput();

#endif
}

void PtyProcess::flushOutput()
{
#ifndef Q_OS_WIN
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
    if (m_writeNotifier && m_outBuffer.isEmpty())
        m_writeNotifier->setEnabled(false);
#endif
}

void PtyProcess::onWritable()
{
    flushOutput();
}

void PtyProcess::setWindowSize(int cols, int rows)
{
#ifdef Q_OS_WIN
    if (m_pipeProcess)
        return;
    if (d && d->pty) {
        COORD size;
        size.X = static_cast<SHORT>(std::max(1, cols));
        size.Y = static_cast<SHORT>(std::max(1, rows));
        g_resizePseudoConsole(d->pty, size);
    }
#else
    if (m_fd < 0)
        return;
    struct winsize ws;
    std::memset(&ws, 0, sizeof(ws));
    ws.ws_col = ushort(std::max(1, cols));
    ws.ws_row = ushort(std::max(1, rows));
    ioctl(m_fd, TIOCSWINSZ, &ws);
#endif
}

void PtyProcess::closeFd()
{
#ifndef Q_OS_WIN
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
#endif
}

void PtyProcess::finish(int exitCode, const QString &reason)
{
    if (m_finished)
        return;
    m_finished = true;

#ifdef Q_OS_WIN

    if (m_pipeProcess) {
        m_pipeProcess->disconnect(this);
        if (m_pipeProcess->state() != QProcess::NotRunning)
            m_pipeProcess->kill();
        m_pipeProcess->deleteLater();
        m_pipeProcess = nullptr;
        emit closed(exitCode, reason);
        return;
    }

    if (d) {
        // Ask the reader to exit before any handle is closed under its feet.
        d->stop = true;
        if (d->reader.joinable())
            d->reader.join();
        if (d->process) {
            DWORD code = 0;
            WaitForSingleObject(d->process, 500);
            if (GetExitCodeProcess(d->process, &code) && exitCode == 0
                && code != STILL_ACTIVE && code != 0) {
                exitCode = int(code);
            }
            CloseHandle(d->process);
            d->process = nullptr;
        }
        if (d->outputRead) {
            CloseHandle(d->outputRead);
            d->outputRead = nullptr;
        }
        if (d->inputWrite) {
            CloseHandle(d->inputWrite);
            d->inputWrite = nullptr;
        }
        if (d->pty) {
            g_closePseudoConsole(d->pty);
            d->pty = nullptr;
        }
    }
    emit closed(exitCode, reason);

#else

    closeFd();
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
    emit closed(exitCode, reason);

#endif
}

void PtyProcess::stop()
{
#ifdef Q_OS_WIN

    if (m_pipeProcess) {
        m_pipeProcess->disconnect(this);
        if (m_pipeProcess->state() != QProcess::NotRunning) {
            m_pipeProcess->kill();
            m_pipeProcess->waitForFinished(500);
        }
        m_pipeProcess->deleteLater();
        m_pipeProcess = nullptr;
    }

    if (!d) {
        m_finished = true;
        return;
    }
    d->stop = true;
    if (d->reader.joinable())
        d->reader.join();
    if (d->inputWrite) {
        CloseHandle(d->inputWrite);
        d->inputWrite = nullptr;
    }
    if (d->outputRead) {
        CloseHandle(d->outputRead);
        d->outputRead = nullptr;
    }
    if (d->process) {
        TerminateProcess(d->process, 0);
        CloseHandle(d->process);
        d->process = nullptr;
    }
    if (d->pty) {
        g_closePseudoConsole(d->pty);
        d->pty = nullptr;
    }
    delete d;
    d = nullptr;
    m_finished = true;

#else

    if (m_fd < 0 && m_pid <= 0)
        return;
    closeFd();
    if (m_pid > 0) {
        ::kill(pid_t(m_pid), SIGHUP);
        int status = 0;
        const pid_t r = waitpid(pid_t(m_pid), &status, 0);
        if (r < 0) {
            ::kill(pid_t(m_pid), SIGKILL);
            waitpid(pid_t(m_pid), &status, 0);
        }
    }
    m_pid = -1;
    m_finished = true;

#endif
}

bool PtyProcess::isRunning() const
{
#ifdef Q_OS_WIN
    if (m_pipeProcess)
        return m_pipeProcess->state() == QProcess::Running;
    return d != nullptr && d->process != nullptr;
#else
    return m_fd >= 0;
#endif
}

qint64 PtyProcess::processId() const
{
#ifdef Q_OS_WIN
    if (m_pipeProcess)
        return qint64(m_pipeProcess->processId());
    return d ? qint64(d->pid) : -1;
#else
    return m_pid;
#endif
}

} // namespace vx
