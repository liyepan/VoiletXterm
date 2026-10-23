#pragma once

#include "SessionSpec.h"
#include "Transport.h"

namespace vx {

class PtyProcess;

// SSH is delegated to the system OpenSSH client running on a pseudo terminal.
// This keeps host key handling, agent, ProxyJump and keyboard-interactive auth
// exactly as users expect, without reimplementing the protocol in-tree.
class SshTransport : public Transport
{
    Q_OBJECT
public:
    explicit SshTransport(const SessionSpec &spec, QObject *parent = nullptr);
    ~SshTransport() override;

    bool start(int cols, int rows) override;
    void write(const QByteArray &data) override;
    void resizeTerminal(int cols, int rows) override;
    QString description() const override { return m_description; }

private:
    void buildCommand(QString &program, QStringList &args, QHash<QString, QString> &env) const;

    SessionSpec m_spec;
    QString m_description;
    PtyProcess *m_pty = nullptr;
};

} // namespace vx
