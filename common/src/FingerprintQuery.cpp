// SPDX-License-Identifier: GPL-3.0-or-later
#include "FingerprintQuery.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QTimer>

namespace {
const QString kService = QStringLiteral("net.reactivated.Fprint");
const QString kManagerPath = QStringLiteral("/net/reactivated/Fprint/Manager");
const QString kManager = QStringLiteral("net.reactivated.Fprint.Manager");
const QString kDevice = QStringLiteral("net.reactivated.Fprint.Device");
constexpr int kTimeoutMs = 3000;
}

FingerprintQuery::FingerprintQuery(QObject *parent)
    : QObject(parent)
{
}

void FingerprintQuery::setUserName(const QString &name)
{
    if (m_userName == name)
        return;
    m_userName = name;
    Q_EMIT userNameChanged();
    refresh();
}

void FingerprintQuery::finish(bool reader, Enrollment enrolled, quint64 generation)
{
    if (generation != m_generation)
        return; // a newer query superseded this one
    m_readerPresent = reader;
    m_enrolled = enrolled;
    m_finished = true;
    qCDebug(lcTheme) << "fingerprint query: reader" << reader << "enrollment" << enrolled;
    Q_EMIT changed();
}

void FingerprintQuery::refresh()
{
    const quint64 generation = ++m_generation;
    const bool notify = m_finished || m_readerPresent || m_enrolled != Unknown;
    m_readerPresent = false;
    m_enrolled = Unknown;
    m_finished = false;
    if (notify)
        Q_EMIT changed();
    if (simulate())
        return;
    queryDevice(generation);
}

bool FingerprintQuery::simulate()
{
    const QString spec = qEnvironmentVariable("MEADOW_FINGERPRINT_SIMULATE");
    if (spec.isEmpty())
        return false;
    const quint64 generation = m_generation;
    bool reader = true;
    Enrollment enrolled = Unknown;
    if (spec == QLatin1String("no-reader")) {
        reader = false;
    } else if (spec.startsWith(QLatin1String("enrolled:"))) {
        const QStringList users = spec.mid(9).split(QLatin1Char(','), Qt::SkipEmptyParts);
        enrolled = users.contains(m_userName) ? Enrolled : NotEnrolled;
    }
    // Asynchronous like the real query.
    QTimer::singleShot(0, this, [this, reader, enrolled, generation] { finish(reader, enrolled, generation); });
    return true;
}

void FingerprintQuery::queryDevice(quint64 generation)
{
    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        QTimer::singleShot(0, this, [this, generation] { finish(false, Unknown, generation); });
        return;
    }
    // GetDefaultDevice needs no authorization.
    const QDBusMessage msg = QDBusMessage::createMethodCall(kService, kManagerPath, kManager, QStringLiteral("GetDefaultDevice"));
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(msg, kTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, generation](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<QDBusObjectPath> reply = *w;
        if (reply.isError()) {
            // NoSuchDevice, or fprintd not installed / not activatable.
            finish(false, Unknown, generation);
            return;
        }
        if (m_userName.isEmpty()) {
            finish(true, Unknown, generation);
            return;
        }
        queryFingers(reply.value().path(), generation);
    });
}

void FingerprintQuery::queryFingers(const QString &devicePath, quint64 generation)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(kService, devicePath, kDevice, QStringLiteral("ListEnrolledFingers"));
    msg << m_userName;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(msg, kTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, generation](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<QStringList> reply = *w;
        if (!reply.isError()) {
            finish(true, reply.value().isEmpty() ? NotEnrolled : Enrolled, generation);
            return;
        }
        const QString error = reply.error().name();
        if (error == QLatin1String("net.reactivated.Fprint.Error.NoEnrolledPrints"))
            finish(true, NotEnrolled, generation);
        else // PermissionDenied (no polkit grant) or anything else: unknown
            finish(true, Unknown, generation);
    });
}
