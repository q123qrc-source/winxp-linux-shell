// SPDX-License-Identifier: GPL-3.0-or-later
#include "IdleMonitor.h"

#include "qwayland-ext-idle-notify-v1.h"

#include <QGuiApplication>
#include <QLoggingCategory>
#include <QtWaylandClient/QWaylandClientExtensionTemplate>

Q_LOGGING_CATEGORY(lcIdle, "meadow.shell.idle")

class IdleNotifier : public QWaylandClientExtensionTemplate<IdleNotifier>, public QtWayland::ext_idle_notifier_v1
{
public:
    IdleNotifier() : QWaylandClientExtensionTemplate<IdleNotifier>(1) { initialize(); }
    ~IdleNotifier() override
    {
        if (isActive())
            destroy();
    }
};

class IdleNotification : public QtWayland::ext_idle_notification_v1
{
public:
    IdleNotification(IdleMonitor *owner, ::ext_idle_notification_v1 *object)
        : QtWayland::ext_idle_notification_v1(object)
        , m_owner(owner)
    {
    }
    ~IdleNotification() override { destroy(); }

protected:
    void ext_idle_notification_v1_idled() override { m_owner->notifyIdle(true); }
    void ext_idle_notification_v1_resumed() override { m_owner->notifyIdle(false); }

private:
    IdleMonitor *m_owner;
};

struct IdleMonitor::Wayland
{
    std::unique_ptr<IdleNotifier> notifier;
    std::unique_ptr<IdleNotification> notification;
};

IdleMonitor::IdleMonitor(bool simulate, QObject *parent)
    : QObject(parent)
    , m_simulate(simulate || QGuiApplication::platformName() != QLatin1String("wayland"))
{
    if (m_simulate)
        return;
    m_wl = std::make_unique<Wayland>();
    m_wl->notifier = std::make_unique<IdleNotifier>();
    connect(m_wl->notifier.get(), &QWaylandClientExtension::activeChanged, this, [this] {
        Q_EMIT availableChanged();
        rearm();
    });
}

IdleMonitor::~IdleMonitor() = default;

bool IdleMonitor::isAvailable() const
{
    return m_simulate || (m_wl && m_wl->notifier->isActive());
}

void IdleMonitor::setTimeoutMs(int ms)
{
    const int hook = qEnvironmentVariableIntValue("MEADOW_IDLE_TIMEOUT_MS");
    if (hook > 0 && ms > 0)
        ms = hook;
    if (ms == m_timeoutMs)
        return;
    m_timeoutMs = ms;
    Q_EMIT timeoutChanged();
    rearm();
}

void IdleMonitor::rearm()
{
    if (m_simulate || !m_wl)
        return;
    m_wl->notification.reset();
    notifyIdle(false);
    if (m_timeoutMs <= 0 || !m_wl->notifier->isActive())
        return;
    auto *native = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    ::wl_seat *seat = native ? native->seat() : nullptr;
    if (!seat) {
        qCWarning(lcIdle) << "no seat; idle detection unavailable";
        return;
    }
    m_wl->notification = std::make_unique<IdleNotification>(
        this, m_wl->notifier->get_idle_notification(uint32_t(m_timeoutMs), seat));
    qCInfo(lcIdle) << "idle after" << m_timeoutMs << "ms";
}

void IdleMonitor::setIdle(bool idle)
{
    notifyIdle(idle);
}

void IdleMonitor::notifyIdle(bool idle)
{
    if (idle == m_idle)
        return;
    m_idle = idle;
    qCInfo(lcIdle) << (idle ? "idle" : "active again");
    Q_EMIT idleChanged();
}
