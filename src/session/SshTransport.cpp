#include "SshTransport.h"

#include "PtyProcess.h"

#include <QDir>
#include <QFileInfo>

namespace vx {

SshTransport::SshTransport(const SessionSpec &spec, QObject *parent)
    : Transport(parent)
    , m_spec(spec)
{
    m_description = spec.summary();
}

SshTransport::~SshTransport() = default;

void SshTransport::buildCommand(QString &program, QStringList &args, QHash<QString, QString> &env) const
{
    program = QStringLiteral("ssh");
    args.clear();

    const int port = m_spec.port > 0 ? m_spec.port : 22;
    args << QStringLiteral("-p") << QString::number(port);

    if (!m_spec.keyFile.isEmpty()) {
        QString key = m_spec.keyFile;
        if (key.startsWith(QLatin1Char('~')))
            key = QDir::homePath() + key.mid(1);
        args << QStringLiteral("-i") << key;
    }
    if (!m_spec.jumpHost.isEmpty()) {
        args << QStringLiteral("-J") << m_spec.jumpHost;
    }

    args << QStringLiteral("-o") << QStringLiteral("ServerAliveInterval=30")
         << QStringLiteral("-o") << QStringLiteral("ServerAliveCountMax=3")
         << QStringLiteral("-o") << QStringLiteral("TCPKeepAlive=yes")
         << QStringLiteral("-o") << QStringLiteral("NumberOfPasswordPrompts=3")
         << QStringLiteral("-o") << QStringLiteral("EscapeChar=~");

    for (const QString &option : m_spec.extraOptions)
        args << QStringLiteral("-o") << option;

    const QString target = m_spec.user.isEmpty()
        ? m_spec.host
        : QStringLiteral("%1@%2").arg(m_spec.user, m_spec.host);
    args << target;

    if (!m_spec.remoteCommand.trimmed().isEmpty()) {
        args << QStringLiteral("-t") << m_spec.remoteCommand;
    }

    env.insert(QStringLiteral("LANG"), QStringLiteral("en_US.UTF-8"));
}

bool SshTransport::start(int cols, int rows)
{
    if (m_spec.host.trimmed().isEmpty()) {
        emitFinished(tr("未填写主机地址"));
        return false;
    }

    QString program;
    QStringList args;
    QHash<QString, QString> env;
    buildCommand(program, args, env);

    m_pty = new PtyProcess(this);
    connect(m_pty, &PtyProcess::dataRead, this, &Transport::emitData);
    connect(m_pty, &PtyProcess::closed, this, [this](int, const QString &reason) {
        emitFinished(reason);
    });

    if (!m_pty->start(program, args, env, cols, rows)) {
        const QString detail = m_pty->errorString();
        emitFinished(detail.isEmpty() ? tr("无法启动 ssh 进程") : detail);
        return false;
    }

    emitConnected();
    return true;
}

void SshTransport::write(const QByteArray &data)
{
    if (m_pty)
        m_pty->write(data);
}

void SshTransport::resizeTerminal(int cols, int rows)
{
    if (m_pty)
        m_pty->setWindowSize(cols, rows);
}

} // namespace vx
