// SPDX-License-Identifier: GPL-3.0-or-later
#include "TimeDateService.h"

#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QLoggingCategory>
#include <QTimeZone>

Q_LOGGING_CATEGORY(lcTimeDate, "meadow.shell.timedate")

namespace {
const QString kService = QStringLiteral("org.freedesktop.timedate1");
const QString kPath = QStringLiteral("/org/freedesktop/timedate1");
const QString kIface = QStringLiteral("org.freedesktop.timedate1");
}

TimeDateService::TimeDateService(const QDBusConnection &bus, bool simulate, QObject *parent)
    : QObject(parent)
    , m_bus(bus)
    , m_simulate(simulate)
{
    if (m_simulate) {
        m_available = true;
        m_timezone = QStringLiteral("Europe/London");
        m_ntp = true;
        m_canNtp = true;
        return;
    }
    m_bus.connect(kService, kPath, QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"),
                  this, SLOT(refresh()));
    refresh();
}

QStringList TimeDateService::timezones() const
{
    QStringList out;
    for (const QByteArray &id : QTimeZone::availableTimeZoneIds()) {
        // Region/City ids only (skip legacy aliases such as "EST5EDT").
        if (id.contains('/') && !id.startsWith("Etc/"))
            out << QString::fromLatin1(id);
    }
    out.sort();
    return out;
}

void TimeDateService::refresh()
{
    if (m_simulate || !m_bus.isConnected())
        return;
    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, QStringLiteral("org.freedesktop.DBus.Properties"),
                                                      QStringLiteral("GetAll"));
    msg << kIface;
    auto *w = new QDBusPendingCallWatcher(m_bus.asyncCall(msg), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<QVariantMap> reply = *w;
        m_available = !reply.isError();
        if (m_available) {
            const QVariantMap p = reply.value();
            m_timezone = p.value(QStringLiteral("Timezone")).toString();
            m_ntp = p.value(QStringLiteral("NTP")).toBool();
            m_canNtp = p.value(QStringLiteral("CanNTP")).toBool();
        }
        Q_EMIT changed();
    });
}

void TimeDateService::call(const QString &method, const QVariantList &args)
{
    qCInfo(lcTimeDate) << method << (m_simulate ? "(simulated)" : "");
    if (m_simulate) {
        if (method == QLatin1String("SetTimezone"))
            m_timezone = args.value(0).toString();
        else if (method == QLatin1String("SetNTP"))
            m_ntp = args.value(0).toBool();
        Q_EMIT changed();
        return;
    }
    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, kIface, method);
    msg.setArguments(args);
    // Let polkit ask for a password (through the session's agent).
    msg.setInteractiveAuthorizationAllowed(true);
    auto *w = new QDBusPendingCallWatcher(m_bus.asyncCall(msg, 5 * 60 * 1000), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, method](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        if (w->isError()) {
            qCWarning(lcTimeDate) << method << "failed:" << w->error().name();
            const bool denied = w->error().type() == QDBusError::AccessDenied
                || w->error().name() == QLatin1String("org.freedesktop.DBus.Error.InteractiveAuthorizationRequired");
            Q_EMIT failed(denied ? tr("You do not have permission to change the date and time.")
                                 : tr("The date and time could not be changed."));
        }
        refresh();
    });
}

void TimeDateService::setTimezone(const QString &zone)
{
    if (zone != m_timezone && QTimeZone::isTimeZoneIdAvailable(zone.toLatin1()))
        call(QStringLiteral("SetTimezone"), {zone, true});
}

void TimeDateService::setNtp(bool on)
{
    if (on != m_ntp)
        call(QStringLiteral("SetNTP"), {on, true});
}

void TimeDateService::setTime(const QDateTime &time)
{
    if (m_ntp || !time.isValid())
        return;
    call(QStringLiteral("SetTime"), {qint64(time.toMSecsSinceEpoch()) * 1000, false, true});
}
