#pragma once

#include <QString>

namespace vx {

// Stores session passwords. Windows uses DPAPI, macOS the Keychain CLI, Linux
// libsecret (secret-tool). When no keyring is available the value is kept in a
// local file with a machine key: weak, but better than plain text.
class CredentialStore
{
public:
    // True when a real keyring backs the store; the UI warns otherwise.
    static bool hasSecureBackend();
    static QString backendName();

    static bool save(const QString &key, const QString &secret);
    static QString load(const QString &key);
    static void remove(const QString &key);
};

} // namespace vx
