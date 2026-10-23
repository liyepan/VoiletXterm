#pragma once

#include "../session/SessionSpec.h"

#include <QVector>

namespace vx {

class SessionStore
{
public:
    static QVector<SessionSpec> load();
    static void saveAll(const QVector<SessionSpec> &sessions);
    static void upsert(const SessionSpec &spec);
    static void removeByName(const QString &name);
};

} // namespace vx
