// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Logging.h"

#include <QObject>
#include <QtQml/qqmlregistration.h>

// Read-only view of an AccountsService user (org.freedesktop.Accounts on the
// system bus): real name, password hint and icon file. Used by the greeter for
// the "Did you forget your password?" hint and by the shell's Start menu.
// Reads only public properties, so it works as the unprivileged sddm user.
// When AccountsService is unavailable, loaded stays false and fields stay empty.
class MEADOW_COMMON_EXPORT UserAccount : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString userName READ userName WRITE setUserName NOTIFY userNameChanged)
    Q_PROPERTY(QString realName READ realName NOTIFY loadedChanged)
    Q_PROPERTY(QString passwordHint READ passwordHint NOTIFY loadedChanged)
    Q_PROPERTY(QString iconFile READ iconFile NOTIFY loadedChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)

public:
    explicit UserAccount(QObject *parent = nullptr);

    QString userName() const { return m_userName; }
    void setUserName(const QString &name);
    QString realName() const { return m_realName; }
    QString passwordHint() const { return m_passwordHint; }
    QString iconFile() const { return m_iconFile; }
    bool loaded() const { return m_loaded; }

Q_SIGNALS:
    void userNameChanged();
    void loadedChanged();

private:
    void fetch();
    void reset();

    QString m_userName;
    QString m_realName;
    QString m_passwordHint;
    QString m_iconFile;
    bool m_loaded = false;
    quint64 m_generation = 0;
};
