#include "SessionStore.h"

#include <QJsonDocument>
#include <QSettings>

namespace vx {

namespace {
const char *kArrayKey = "sessions";
}

QVector<SessionSpec> SessionStore::load()
{
    QVector<SessionSpec> sessions;
    QSettings settings;
    const int count = settings.beginReadArray(QLatin1String(kArrayKey));
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        const QByteArray json = settings.value(QStringLiteral("json")).toByteArray();
        if (json.isEmpty())
            continue;
        const QJsonDocument doc = QJsonDocument::fromJson(json);
        if (doc.isObject())
            sessions.append(SessionSpec::fromJson(doc.object()));
    }
    settings.endArray();
    return sessions;
}

void SessionStore::saveAll(const QVector<SessionSpec> &sessions)
{
    QSettings settings;
    settings.beginWriteArray(QLatin1String(kArrayKey), sessions.size());
    for (int i = 0; i < sessions.size(); ++i) {
        settings.setArrayIndex(i);
        const QJsonDocument doc(sessions.at(i).toJson());
        settings.setValue(QStringLiteral("json"), doc.toJson(QJsonDocument::Compact));
    }
    settings.endArray();
    settings.sync();
}

void SessionStore::upsert(const SessionSpec &spec)
{
    QVector<SessionSpec> sessions = load();
    const QString key = spec.name.isEmpty() ? spec.displayName() : spec.name;
    bool replaced = false;
    for (SessionSpec &existing : sessions) {
        const QString existingKey = existing.name.isEmpty() ? existing.displayName() : existing.name;
        if (existingKey == key) {
            existing = spec;
            existing.name = key;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        SessionSpec copy = spec;
        copy.name = key;
        sessions.append(copy);
    }
    saveAll(sessions);
}

void SessionStore::removeByName(const QString &name)
{
    QVector<SessionSpec> sessions = load();
    for (int i = sessions.size() - 1; i >= 0; --i) {
        const QString key = sessions.at(i).name.isEmpty() ? sessions.at(i).displayName() : sessions.at(i).name;
        if (key == name)
            sessions.remove(i);
    }
    saveAll(sessions);
}

} // namespace vx
