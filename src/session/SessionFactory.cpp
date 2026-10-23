#include "SessionFactory.h"

#include "SerialTransport.h"
#include "SshTransport.h"
#include "TelnetTransport.h"
#include "Transport.h"

namespace vx {

Transport *createTransport(const SessionSpec &spec, QObject *parent)
{
    switch (spec.type) {
    case SessionSpec::Ssh:
        return new SshTransport(spec, parent);
    case SessionSpec::Telnet:
        return new TelnetTransport(spec, parent);
    case SessionSpec::Serial:
        return new SerialTransport(spec, parent);
    }
    return nullptr;
}

} // namespace vx
