// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <memory>

// Tells the screen saver when the user has been idle for `timeoutMs`, via
// ext-idle-notify-v1 (get_idle_notification, so idle inhibitors such as a
// playing video keep it from starting). `idle` goes back to false on the
// next input.
//
// Test hook: MEADOW_IDLE_TIMEOUT_MS overrides the timeout (seconds-long
// screen saver tests). In --preview, idle is only set through setIdle().
class IdleMonitor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ isAvailable NOTIFY availableChanged)
    Q_PROPERTY(bool idle READ isIdle NOTIFY idleChanged)
    Q_PROPERTY(int timeoutMs READ timeoutMs WRITE setTimeoutMs NOTIFY timeoutChanged)

public:
    explicit IdleMonitor(bool simulate, QObject *parent = nullptr);
    ~IdleMonitor() override;

    bool isAvailable() const;
    bool isIdle() const { return m_idle; }
    int timeoutMs() const { return m_timeoutMs; }
    // 0 turns idle notifications off.
    void setTimeoutMs(int ms);

    // Preview and tests: pretend the user went idle or came back.
    Q_INVOKABLE void setIdle(bool idle);

    struct Wayland;
    void notifyIdle(bool idle);

Q_SIGNALS:
    void availableChanged();
    void idleChanged();
    void timeoutChanged();

private:
    void rearm();

    bool m_simulate;
    bool m_idle = false;
    int m_timeoutMs = 0;
    std::unique_ptr<Wayland> m_wl;
};
