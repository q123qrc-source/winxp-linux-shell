// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QSettings>
#include <QStringList>
#include <memory>

// Shell settings: ~/.config/meadow/shell.conf over the packaged defaults in
// /usr/share/meadow/shell.conf (MEADOW_SHELL_DEFAULTS overrides that path).
// Only the user file is ever written, and only keys the user changed.
class Settings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool taskbarLocked READ taskbarLocked WRITE setTaskbarLocked NOTIFY changed)
    Q_PROPERTY(bool autoHide READ autoHide WRITE setAutoHide NOTIFY changed)
    Q_PROPERTY(bool groupButtons READ groupButtons WRITE setGroupButtons NOTIFY changed)
    Q_PROPERTY(bool showQuickLaunch READ showQuickLaunch WRITE setShowQuickLaunch NOTIFY changed)
    Q_PROPERTY(bool showClock READ showClock WRITE setShowClock NOTIFY changed)
    Q_PROPERTY(bool hideInactiveIcons READ hideInactiveIcons WRITE setHideInactiveIcons NOTIFY changed)
    Q_PROPERTY(QStringList quickLaunch READ quickLaunch WRITE setQuickLaunch NOTIFY changed)

public:
    explicit Settings(QObject *parent = nullptr);
    // For tests: explicit files instead of the XDG locations.
    Settings(const QString &userFile, const QString &defaultsFile, QObject *parent = nullptr);
    ~Settings() override;

    static QString defaultUserFile();
    static QString defaultDefaultsFile();
    QString userFile() const;

    bool taskbarLocked() const { return boolValue("Taskbar/locked", true); }
    void setTaskbarLocked(bool v) { setValue("Taskbar/locked", v); }
    bool autoHide() const { return boolValue("Taskbar/autoHide", false); }
    void setAutoHide(bool v) { setValue("Taskbar/autoHide", v); }
    bool groupButtons() const { return boolValue("Taskbar/groupButtons", true); }
    void setGroupButtons(bool v) { setValue("Taskbar/groupButtons", v); }
    bool showQuickLaunch() const { return boolValue("Taskbar/showQuickLaunch", true); }
    void setShowQuickLaunch(bool v) { setValue("Taskbar/showQuickLaunch", v); }
    bool showClock() const { return boolValue("Taskbar/showClock", true); }
    void setShowClock(bool v) { setValue("Taskbar/showClock", v); }
    bool hideInactiveIcons() const { return boolValue("Taskbar/hideInactiveIcons", true); }
    void setHideInactiveIcons(bool v) { setValue("Taskbar/hideInactiveIcons", v); }
    QStringList quickLaunch() const;
    void setQuickLaunch(const QStringList &ids) { setValue("Taskbar/quickLaunch", ids); }

    // Forget the user's value for a key (the packaged default applies again).
    Q_INVOKABLE void reset(const QString &key);

Q_SIGNALS:
    void changed();

private:
    QVariant value(const char *key, const QVariant &fallback) const;
    bool boolValue(const char *key, bool fallback) const;
    void setValue(const char *key, const QVariant &value);

    std::unique_ptr<QSettings> m_user;
    std::unique_ptr<QSettings> m_defaults;
};
