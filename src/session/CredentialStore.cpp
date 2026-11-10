#include "CredentialStore.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRandomGenerator>
#include <QSettings>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincrypt.h>
#endif

namespace vx {

namespace {

const char *kService = "voiletxterm";

QString obfuscationKey()
{
    // Machine-local key file, 0600. Not real encryption; used only when the
    // platform has no keyring available.
    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
        + QStringLiteral("/vault.key");
    QFile file(path);
    if (!file.exists()) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QByteArray random(32, Qt::Uninitialized);
        for (int i = 0; i < random.size(); ++i)
            random[i] = char(QRandomGenerator::global()->bounded(256));
        if (!file.open(QIODevice::WriteOnly))
            return QString();
        file.write(random.toBase64());
        file.close();
        QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);
    }
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromLatin1(file.readAll().trimmed());
}

QByteArray xorWith(const QByteArray &data, const QByteArray &key)
{
    if (key.isEmpty())
        return QByteArray();
    QByteArray out = data;
    for (int i = 0; i < out.size(); ++i)
        out[i] = char(out.at(i) ^ key.at(i % key.size()));
    return out;
}

QString settingsKey(const QString &key)
{
    return QStringLiteral("vault/") + QString::fromLatin1(key.toUtf8().toBase64(QByteArray::Base64UrlEncoding));
}

#ifdef Q_OS_WIN
QByteArray protectDpapi(const QByteArray &plain, bool encrypt)
{
    DATA_BLOB in;
    in.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(plain.constData()));
    in.cbData = DWORD(plain.size());
    DATA_BLOB out{};
    const DWORD flags = CRYPTPROTECT_UI_FORBIDDEN;
    const BOOL ok = encrypt ? CryptProtectData(&in, L"VoiletXterm", nullptr, nullptr, nullptr, flags, &out)
                            : CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, flags, &out);
    if (!ok)
        return QByteArray();
    QByteArray result(reinterpret_cast<const char *>(out.pbData), int(out.cbData));
    LocalFree(out.pbData);
    return result;
}
#endif

QString runTool(const QString &program, const QStringList &args, const QByteArray &input = QByteArray())
{
    QProcess process;
    process.start(program, args);
    if (!process.waitForStarted(600))
        return QString();
    if (!input.isEmpty())
        process.write(input);
    process.closeWriteChannel();
    if (!process.waitForFinished(1500)) {
        process.kill();
        return QString();
    }
    if (process.exitStatus() != QProcess::NormalExit)
        return QString();
    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

} // namespace

bool CredentialStore::hasSecureBackend()
{
#ifdef Q_OS_WIN
    return true;
#elif defined(Q_OS_MACOS)
    return true;
#else
    return !QStandardPaths::findExecutable(QStringLiteral("secret-tool")).isEmpty();
#endif
}

QString CredentialStore::backendName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("Windows DPAPI");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("macOS Keychain");
#else
    if (!QStandardPaths::findExecutable(QStringLiteral("secret-tool")).isEmpty())
        return QStringLiteral("libsecret");
    return QStringLiteral("本地弱加密");
#endif
}

bool CredentialStore::save(const QString &key, const QString &secret)
{
    if (key.isEmpty() || secret.isEmpty())
        return false;

#ifdef Q_OS_WIN
    const QByteArray cipher = protectDpapi(secret.toUtf8(), true);
    if (cipher.isEmpty())
        return false;
    QSettings().setValue(settingsKey(key), cipher.toBase64());
    return true;
#elif defined(Q_OS_MACOS)
    runTool(QStringLiteral("security"),
            {QStringLiteral("add-generic-password"), QStringLiteral("-U"),
             QStringLiteral("-s"), QLatin1String(kService), QStringLiteral("-a"), key,
             QStringLiteral("-w"), secret});
    return true;
#else
    if (hasSecureBackend()) {
        runTool(QStringLiteral("secret-tool"),
                {QStringLiteral("store"), QStringLiteral("--label"), QStringLiteral("VoiletXterm"),
                 QStringLiteral("service"), QLatin1String(kService),
                 QStringLiteral("account"), key},
                secret.toUtf8());
        return true;
    }
    const QString localKey = obfuscationKey();
    if (localKey.isEmpty())
        return false;
    const QByteArray cipher = xorWith(secret.toUtf8(), localKey.toUtf8());
    QSettings().setValue(settingsKey(key), cipher.toBase64());
    return true;
#endif
}

QString CredentialStore::load(const QString &key)
{
    if (key.isEmpty())
        return QString();

#ifdef Q_OS_WIN
    const QByteArray stored = QSettings().value(settingsKey(key)).toByteArray();
    if (stored.isEmpty())
        return QString();
    const QByteArray plain = protectDpapi(QByteArray::fromBase64(stored), false);
    return QString::fromUtf8(plain);
#elif defined(Q_OS_MACOS)
    return runTool(QStringLiteral("security"),
                   {QStringLiteral("find-generic-password"), QStringLiteral("-s"),
                    QLatin1String(kService), QStringLiteral("-a"), key, QStringLiteral("-w")});
#else
    if (hasSecureBackend()) {
        return runTool(QStringLiteral("secret-tool"),
                       {QStringLiteral("lookup"), QStringLiteral("service"), QLatin1String(kService),
                        QStringLiteral("account"), key});
    }
    const QByteArray stored = QSettings().value(settingsKey(key)).toByteArray();
    if (stored.isEmpty())
        return QString();
    const QString localKey = obfuscationKey();
    return QString::fromUtf8(xorWith(QByteArray::fromBase64(stored), localKey.toUtf8()));
#endif
}

void CredentialStore::remove(const QString &key)
{
    if (key.isEmpty())
        return;

#ifdef Q_OS_WIN
    QSettings settings;
    settings.remove(settingsKey(key));
#elif defined(Q_OS_MACOS)
    runTool(QStringLiteral("security"),
            {QStringLiteral("delete-generic-password"), QStringLiteral("-s"), QLatin1String(kService),
             QStringLiteral("-a"), key});
#else
    if (hasSecureBackend()) {
        runTool(QStringLiteral("secret-tool"),
                {QStringLiteral("clear"), QStringLiteral("service"), QLatin1String(kService),
                 QStringLiteral("account"), key});
    } else {
        QSettings().remove(settingsKey(key));
    }
#endif
}

} // namespace vx
