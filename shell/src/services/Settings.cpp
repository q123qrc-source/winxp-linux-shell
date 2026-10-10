// SPDX-License-Identifier: GPL-3.0-or-later
#include "Settings.h"

#include <QStandardPaths>

Settings::Settings(QObject *parent)
    : Settings(defaultUserFile(), defaultDefaultsFile(), parent)
{
}

Settings::Settings(const QString &userFile, const QString &defaultsFile, QObject *parent)
    : QObject(parent)
    , m_user(std::make_unique<QSettings>(userFile, QSettings::IniFormat))
    , m_defaults(std::make_unique<QSettings>(defaultsFile, QSettings::IniFormat))
{
}

Settings::~Settings() = default;

QString Settings::defaultUserFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QLatin1String("/meadow/shell.conf");
}

QString Settings::defaultDefaultsFile()
{
    const QString env = qEnvironmentVariable("MEADOW_SHELL_DEFAULTS");
    return env.isEmpty() ? QStringLiteral(MEADOW_DATADIR "/shell.conf") : env;
}

QString Settings::userFile() const
{
    return m_user->fileName();
}

QVariant Settings::value(const char *key, const QVariant &fallback) const
{
    const QString k = QString::fromLatin1(key);
    if (m_user->contains(k))
        return m_user->value(k);
    return m_defaults->value(k, fallback);
}

bool Settings::boolValue(const char *key, bool fallback) const
{
    const QVariant v = value(key, fallback);
    // INI values come back as strings.
    if (v.typeId() == QMetaType::QString)
        return v.toString() == QLatin1String("true") || v.toString() == QLatin1String("1");
    return v.toBool();
}

QStringList Settings::quickLaunch() const
{
    const QVariant v = value("Taskbar/quickLaunch", QStringList{});
    QStringList list = v.typeId() == QMetaType::QString ? v.toString().split(QLatin1Char(','), Qt::SkipEmptyParts) : v.toStringList();
    for (QString &s : list)
        s = s.trimmed();
    list.removeAll(QString());
    return list;
}

void Settings::setValue(const char *key, const QVariant &v)
{
    const QString k = QString::fromLatin1(key);
    if (m_user->contains(k) && m_user->value(k) == v)
        return;
    m_user->setValue(k, v);
    m_user->sync();
    Q_EMIT changed();
}

void Settings::reset(const QString &key)
{
    if (!m_user->contains(key))
        return;
    m_user->remove(key);
    m_user->sync();
    Q_EMIT changed();
}
