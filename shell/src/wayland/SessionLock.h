// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QWindow>
#include <memory>

// Locks the session with ext-session-lock-v1. While locked the compositor
// shows nothing but the lock surfaces (one per output) and sends all input to
// them; if the shell dies while locked, the session stays locked until a new
// client locks and unlocks it.
//
// QML owns the lock windows (LockScreen.qml, one per screen) and hands them
// to addSurface(); SessionLock turns each into an ext_session_lock_surface_v1
// for its screen's output when a lock starts, and hides them on unlock.
//
// Crash safety: once the compositor confirms the lock, a flag file
// $XDG_RUNTIME_DIR/meadow/locked is written; it is removed after unlocking.
// A shell started while the flag exists (the autostart loop restarting a
// crashed shell) locks again straight away, see relockIfNeeded().
//
// Preview mode (or no compositor support) simulates the protocol so the lock
// screen can be shown and tested in an ordinary window.
class SessionLock : public QObject
{
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool locked READ isLocked NOTIFY stateChanged)
    // True from lock() until unlock() completes (Locking, Locked).
    Q_PROPERTY(bool active READ isActive NOTIFY stateChanged)
    Q_PROPERTY(bool available READ isAvailable CONSTANT)
    Q_PROPERTY(bool simulated READ isSimulated CONSTANT)

public:
    enum State { Unlocked, Locking, Locked };
    Q_ENUM(State)

    // simulate: never talk to the compositor (preview, tests).
    explicit SessionLock(bool simulate, QObject *parent = nullptr);
    ~SessionLock() override;

    State state() const { return m_state; }
    bool isLocked() const { return m_state == Locked; }
    bool isActive() const { return m_state != Unlocked; }
    bool isAvailable() const;
    bool isSimulated() const { return m_simulate; }

    // Lock windows; each gets a lock surface on its own screen.
    Q_INVOKABLE void addSurface(QWindow *window);
    Q_INVOKABLE void removeSurface(QWindow *window);

    // Starts locking (no-op while locking or locked). Returns false if the
    // compositor cannot lock.
    Q_INVOKABLE bool lock();
    // False when no PAM service for the lock screen is installed (install.sh
    // --no-pam): locking would leave the user no way back in.
    static bool unlockPossible();
    // Unlocks. Only called after authentication succeeded.
    Q_INVOKABLE void unlock();

    // Locks if the previous shell left the session locked (flag file).
    void relockIfNeeded();

    static QString flagFilePath();

Q_SIGNALS:
    void stateChanged();
    // The compositor refused or ended the lock before it took effect.
    void lockFailed();

private:
    friend class SessionLockObject;
    friend class LockShellIntegration;
    struct Wayland;

    void setState(State state);
    void showSurfaces();
    void hideSurfaces();
    void onLocked();
    void onFinished();
    void writeFlag(bool locked);

    bool m_simulate = false;
    State m_state = Unlocked;
    bool m_unlocking = false; // unlock() re-entered from the roundtrip
    QList<QPointer<QWindow>> m_windows;
    std::unique_ptr<Wayland> m_wl;
};
