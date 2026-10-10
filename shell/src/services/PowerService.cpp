// SPDX-License-Identifier: GPL-3.0-or-later
#include "PowerService.h"

#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcPower, "meadow.shell.power")

namespace {
const QString kService = QStringLiteral("org.freedesktop.login1");
const QString kPath = QStringLiteral("/org/freedesktop/login1");
const QString kInterface = QStringLiteral("org.freedesktop.login1.Manager");
} // namespace

PowerService::PowerService(bool simulate, QObject *parent)
    : PowerService(QDBusConnection::systemBus(), simulate, parent)
{
}

PowerService::PowerService(const QDBusConnection &bus, bool simulate, QObject *parent)
    : QObject(parent)
    , m_bus(bus)
    , m_simulate(simulate)
{
    if (!m_simulate)
        refresh();
}

void PowerService::refresh()
{
    const QList<QPair<QString, bool *>> checks{
        {QStringLiteral("CanPowerOff"), &m_canPowerOff},
        {QStringLiteral("CanReboot"), &m_canReboot},
        {QStringLiteral("CanSuspend"), &m_canSuspend},
        {QStringLiteral("CanHibernate"), &m_canHibernate},
    };
    for (const auto &[method, flag] : checks) {
        const QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, kInterface, method);
        auto *w = new QDBusPendingCallWatcher(m_bus.asyncCall(msg), this);
        connect(w, &QDBusPendingCallWatcher::finished, this, [this, flag, method](QDBusPendingCallWatcher *w) {
            w->deleteLater();
            QDBusPendingReply<QString> reply = *w;
            // "yes", "challenge" (polkit asks), "no" or "na".
            const bool can = !reply.isError() && (reply.value() == QLatin1String("yes") || reply.value() == QLatin1String("challenge"));
            qCDebug(lcPower) << method << (reply.isError() ? reply.error().name() : reply.value());
            if (can != *flag) {
                *flag = can;
                Q_EMIT changed();
            }
        });
    }
}

void PowerService::request(const QString &method)
{
    qCInfo(lcPower) << "requested" << method << (m_simulate ? "(simulated)" : "");
    if (m_simulate) {
        Q_EMIT simulated(method);
        return;
    }
    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, kInterface, method);
    msg << true; // interactive: let polkit authenticate
    auto *w = new QDBusPendingCallWatcher(m_bus.asyncCall(msg, 120000), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, method](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        if (w->isError()) {
            qCWarning(lcPower) << method << "failed:" << w->error().name();
            Q_EMIT failed(method, w->error().message());
        }
    });
}
