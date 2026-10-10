// SPDX-License-Identifier: GPL-3.0-or-later
#include "SessionService.h"

#include "Launcher.h"

#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcSession, "meadow.shell.session")

namespace {
const QString kDisplayManager = QStringLiteral("org.freedesktop.DisplayManager");
}

SessionService::SessionService(bool simulate, QObject *parent)
    : SessionService(QDBusConnection::systemBus(), simulate, parent)
{
}

SessionService::SessionService(const QDBusConnection &bus, bool simulate, QObject *parent)
    : QObject(parent)
    , m_bus(bus)
    , m_simulate(simulate)
    , m_seatPath(qEnvironmentVariable("XDG_SEAT_PATH"))
{
    if (m_simulate || m_seatPath.isEmpty() || !m_bus.isConnected())
        return;
    // Switch User needs a display manager that implements the seat API.
    const QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
                                                            QStringLiteral("org.freedesktop.DBus"), QStringLiteral("NameHasOwner"))
                             << kDisplayManager;
    auto *w = new QDBusPendingCallWatcher(m_bus.asyncCall(msg), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<bool> reply = *w;
        m_canSwitchUser = !reply.isError() && reply.value();
        Q_EMIT changed();
    });
}

void SessionService::logOff()
{
    qCInfo(lcSession) << "log off requested" << (m_simulate ? "(simulated)" : "");
    if (m_simulate) {
        Q_EMIT simulated(QStringLiteral("LogOff"));
        return;
    }
    if (qEnvironmentVariableIsSet("LABWC_PID")) {
        if (!Launcher::start(QStringLiteral("labwc"), {QStringLiteral("--exit")}))
            Q_EMIT failed(QStringLiteral("LogOff"), tr("Could not ask labwc to exit."));
        return;
    }
    const QString session = qEnvironmentVariable("XDG_SESSION_ID");
    if (session.isEmpty()) {
        Q_EMIT failed(QStringLiteral("LogOff"), tr("No session to end."));
        return;
    }
    QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.login1"), QStringLiteral("/org/freedesktop/login1"),
                                                      QStringLiteral("org.freedesktop.login1.Manager"), QStringLiteral("TerminateSession"));
    msg << session;
    m_bus.asyncCall(msg);
}

void SessionService::switchUser()
{
    qCInfo(lcSession) << "switch user requested" << (m_simulate ? "(simulated)" : "");
    if (m_simulate) {
        Q_EMIT simulated(QStringLiteral("SwitchToGreeter"));
        return;
    }
    if (m_seatPath.isEmpty()) {
        Q_EMIT failed(QStringLiteral("SwitchToGreeter"), tr("The display manager does not support switching users."));
        return;
    }
    const QDBusMessage msg = QDBusMessage::createMethodCall(kDisplayManager, m_seatPath, QStringLiteral("org.freedesktop.DisplayManager.Seat"),
                                                            QStringLiteral("SwitchToGreeter"));
    auto *w = new QDBusPendingCallWatcher(m_bus.asyncCall(msg), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        if (w->isError()) {
            qCWarning(lcSession) << "SwitchToGreeter failed:" << w->error().name();
            Q_EMIT failed(QStringLiteral("SwitchToGreeter"), w->error().message());
        }
    });
}
