#pragma once

#include <QObject>
#include <QTime>

class QTimer;

namespace vx {

// Owns the application colour scheme: follow the platform, follow a schedule
// (light between 06:00 and 18:00) or force one of the two.
class ThemeManager : public QObject
{
    Q_OBJECT
public:
    enum Mode {
        System = 0,    // follow the desktop / OS setting
        Schedule = 1,  // light 06:00-18:00, dark otherwise
        Light = 2,
        Dark = 3,
    };
    Q_ENUM(Mode)

    static ThemeManager *instance();

    void initialize();
    Mode mode() const { return m_mode; }
    void setMode(Mode mode, bool persist = true);
    bool isDark() const { return m_dark; }

    static bool schedulePrefersDark(const QTime &now = QTime::currentTime());

signals:
    void schemeChanged(bool dark);

private:
    explicit ThemeManager(QObject *parent = nullptr);
    void reevaluate(bool force);
    bool detectSystemDark() const;

    Mode m_mode = System;
    bool m_dark = true;
    bool m_applied = false;
    bool m_systemFallbackDark = true;
    QTimer *m_timer = nullptr;
};

} // namespace vx
