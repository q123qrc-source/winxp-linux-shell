// SPDX-License-Identifier: GPL-3.0-or-later
#include "SessionService.h"

#include "Launcher.h"

#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QLoggingCategory>

#include <unistd.h>

Q_LOGGING_CATEGORY(lcSession, "meadow.shell.session")

namespace {
const QString kDisplayManager = QStringLiteral("org.freedesktop.DisplayManager");
const QString kLogin1 = QStringLiteral("org.freedesktop.login1");
const QString kLogin1Path = QStringLiteral("/org/freedesktop/login1");
const QString kManager = QStringLiteral("org.freedesktop.login1.Manager");
const QString kSessionIface = QStringLiteral("org.freedesktop.login1.Session");
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
    if (m_simulate || !m_bus.isConnected())
        return;
    watchLogind();
    if (m_seatPath.isEmpty())
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

void SessionService::watchLogind()
{
    m_bus.connect(kLogin1, kLogin1Path, kManager, QStringLiteral("PrepareForSleep"), this, SLOT(onPrepareForSleep(bool)));
    // Our session's object: by id when the session exported it, else by PID.
    const QString id = qEnvironmentVariable("XDG_SESSION_ID");
    QDBusMessage msg = id.isEmpty()
        ? QDBusMessage::createMethodCall(kLogin1, kLogin1Path, kManager, QStringLiteral("GetSessionByPID")) << quint32(getpid())
        : QDBusMessage::createMethodCall(kLogin1, kLogin1Path, kManager, QStringLiteral("GetSession")) << id;
    auto *w = new QDBusPendingCallWatcher(m_bus.asyncCall(msg), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<QDBusObjectPath> reply = *w;
        if (reply.isError()) {
            qCInfo(lcSession) << "no logind session for this shell; loginctl lock-session will not reach it";
            return;
        }
        m_sessionPath = reply.value().path();
        m_bus.connect(kLogin1, m_sessionPath, kSessionIface, QStringLiteral("Lock"), this, SLOT(onLock()));
        m_bus.connect(kLogin1, m_sessionPath, kSessionIface, QStringLiteral("Unlock"), this, SLOT(onUnlock()));
        qCDebug(lcSession) << "following logind session" << m_sessionPath;
    });
}

void SessionService::setLockBeforeSleep(bool enabled)
{
    if (m_lockBeforeSleep == enabled)
        return;
    m_lockBeforeSleep = enabled;
    if (enabled)
        takeSleepInhibitor();
    else
        sleepReady();
}

void SessionService::takeSleepInhibitor()
{
    if (m_simulate || !m_bus.isConnected() || m_sleepInhibitor.isValid())
        return;
    QDBusMessage msg = QDBusMessage::createMethodCall(kLogin1, kLogin1Path, kManager, QStringLiteral("Inhibit"));
    msg << QStringLiteral("sleep") << QStringLiteral("Meadow") << tr("Locking the screen before sleeping") << QStringLiteral("delay");
    auto *w = new QDBusPendingCallWatcher(m_bus.asyncCall(msg), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<QDBusUnixFileDescriptor> reply = *w;
        if (reply.isError()) {
            qCInfo(lcSession) << "cannot delay sleep for locking:" << reply.error().name();
            return;
        }
        if (m_lockBeforeSleep)
            m_sleepInhibitor = reply.value();
    });
}

void SessionService::sleepReady()
{
    // Dropping the descriptor releases the delay lock.
    m_sleepInhibitor = QDBusUnixFileDescriptor();
}

void SessionService::onPrepareForSleep(bool start)
{
    if (start) {
        qCInfo(lcSession) << "system is about to sleep";
        Q_EMIT aboutToSleep();
        if (!m_lockBeforeSleep)
            sleepReady();
    } else {
        qCInfo(lcSession) << "system resumed";
        Q_EMIT resumed();
        if (m_lockBeforeSleep)
            takeSleepInhibitor();
    }
}

void SessionService::setLockedHint(bool locked)
{
    if (m_simulate || m_sessionPath.isEmpty())
        return;
    QDBusMessage msg = QDBusMessage::createMethodCall(kLogin1, m_sessionPath, kSessionIface, QStringLiteral("SetLockedHint"));
    msg << locked;
    m_bus.asyncCall(msg);
}
