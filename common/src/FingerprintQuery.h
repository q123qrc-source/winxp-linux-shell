// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Logging.h"

#include <QObject>
#include <QtQml/qqmlregistration.h>

// Read-only fprintd query (net.reactivated.Fprint on the system bus): is a
// reader present, and does a user have enrolled fingers?
//
// It never claims the device, verifies or enrolls. Listing *another* user's
// fingers needs fprintd's setusername permission, which the unprivileged
// greeter does not have by default; the answer is then Unknown. An optional
// polkit rule (see docs/PAM_FINGERPRINT.md) grants it.
//
// For tests and previews, MEADOW_FINGERPRINT_SIMULATE replaces D-Bus:
//   "no-reader" | "unknown" | "enrolled:alice,bart" (others: not enrolled)
class MEADOW_COMMON_EXPORT FingerprintQuery : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString userName READ userName WRITE setUserName NOTIFY userNameChanged)
    Q_PROPERTY(bool readerPresent READ readerPresent NOTIFY changed)
    Q_PROPERTY(Enrollment enrolled READ enrolled NOTIFY changed)
    Q_PROPERTY(bool finished READ finished NOTIFY changed)

public:
    enum Enrollment { Unknown, Enrolled, NotEnrolled };
    Q_ENUM(Enrollment)

    explicit FingerprintQuery(QObject *parent = nullptr);

    QString userName() const { return m_userName; }
    void setUserName(const QString &name);
    bool readerPresent() const { return m_readerPresent; }
    Enrollment enrolled() const { return m_enrolled; }
    bool finished() const { return m_finished; }

    // Re-run the query (e.g. after a reader was plugged in).
    Q_INVOKABLE void refresh();

Q_SIGNALS:
    void userNameChanged();
    void changed();

private:
    void finish(bool reader, Enrollment enrolled, quint64 generation);
    bool simulate();
    void queryDevice(quint64 generation);
    void queryFingers(const QString &devicePath, quint64 generation);

    QString m_userName;
    bool m_readerPresent = false;
    Enrollment m_enrolled = Unknown;
    bool m_finished = false;
    quint64 m_generation = 0;
};
