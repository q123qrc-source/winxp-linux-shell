// SPDX-License-Identifier: GPL-3.0-or-later
#include "SessionLock.h"

#include "qwayland-ext-session-lock-v1.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QStandardPaths>
#include <QtWaylandClient/QWaylandClientExtensionTemplate>
#include <QtWaylandClient/private/qwaylandscreen_p.h>
#include <QtWaylandClient/private/qwaylandshellintegration_p.h>
#include <QtWaylandClient/private/qwaylandshellsurface_p.h>
#include <QtWaylandClient/private/qwaylandwindow_p.h>

Q_LOGGING_CATEGORY(lcLock, "meadow.shell.lock")

class SessionLockManager : public QWaylandClientExtensionTemplate<SessionLockManager>,
                           public QtWayland::ext_session_lock_manager_v1
{
public:
    SessionLockManager()
        : QWaylandClientExtensionTemplate<SessionLockManager>(1)
    {
        initialize();
    }
    ~SessionLockManager() override
    {
        if (isActive())
            destroy();
    }
};

class SessionLockObject : public QtWayland::ext_session_lock_v1
{
public:
    SessionLockObject(SessionLock *owner, ::ext_session_lock_v1 *object)
        : QtWayland::ext_session_lock_v1(object)
        , m_owner(owner)
    {
    }

protected:
    void ext_session_lock_v1_locked() override { m_owner->onLocked(); }
    void ext_session_lock_v1_finished() override { m_owner->onFinished(); }

private:
    SessionLock *m_owner;
};

// The role of a lock window's wl_surface: an ext_session_lock_surface_v1 on
// the output of the window's screen. Sized by the compositor (the output's
// size); the first configure exposes the window.
class LockShellSurface : public QtWaylandClient::QWaylandShellSurface,
                         public QtWayland::ext_session_lock_surface_v1
{
public:
    LockShellSurface(QtWayland::ext_session_lock_v1 *lock, QtWaylandClient::QWaylandWindow *window)
        : QtWaylandClient::QWaylandShellSurface(window)
    {
        QtWaylandClient::QWaylandScreen *screen = window->waylandScreen();
        ::wl_output *output = screen ? screen->output() : nullptr;
        if (!output) {
            qCWarning(lcLock) << "lock window has no output; it cannot be shown";
            return;
        }
        init(lock->get_lock_surface(wlSurface(), output));
    }
    ~LockShellSurface() override
    {
        if (isInitialized())
            destroy();
    }

    bool isExposed() const override { return m_configured; }
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    bool commitSurfaceRole() const override { return false; }
#endif
    void applyConfigure() override { window()->resizeFromApplyConfigure(m_size); }

protected:
    void ext_session_lock_surface_v1_configure(uint32_t serial, uint32_t width, uint32_t height) override
    {
        ack_configure(serial);
        m_size = QSize(int(width), int(height));
        if (!m_configured) {
            m_configured = true;
            window()->resizeFromApplyConfigure(m_size);
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
            window()->updateExposure();
#else
            window()->sendRecursiveExposeEvent();
#endif
        } else {
            window()->applyConfigureWhenPossible();
        }
    }

private:
    QSize m_size;
    bool m_configured = false;
};

class LockShellIntegration : public QtWaylandClient::QWaylandShellIntegration
{
public:
    explicit LockShellIntegration(SessionLock *owner) : m_owner(owner) {}
    bool initialize(QtWaylandClient::QWaylandDisplay *) override { return true; }
    QtWaylandClient::QWaylandShellSurface *createShellSurface(QtWaylandClient::QWaylandWindow *window) override;

    SessionLock *m_owner;
};

struct SessionLock::Wayland
{
    std::unique_ptr<SessionLockManager> manager;
    std::unique_ptr<SessionLockObject> lock;
    std::unique_ptr<LockShellIntegration> integration;
};

QtWaylandClient::QWaylandShellSurface *LockShellIntegration::createShellSurface(QtWaylandClient::QWaylandWindow *window)
{
    SessionLockObject *lock = m_owner->m_wl ? m_owner->m_wl->lock.get() : nullptr;
    if (!lock) {
        qCWarning(lcLock) << "lock window shown without an active lock";
        return nullptr;
    }
    return new LockShellSurface(lock, window);
}

SessionLock::SessionLock(bool simulate, QObject *parent)
    : QObject(parent)
    , m_simulate(simulate || QGuiApplication::platformName() != QLatin1String("wayland"))
{
    if (!m_simulate) {
        m_wl = std::make_unique<Wayland>();
        m_wl->manager = std::make_unique<SessionLockManager>();
        m_wl->integration = std::make_unique<LockShellIntegration>(this);
    }
}

SessionLock::~SessionLock()
{
    // A shell that exits while locked leaves the session locked (the
    // compositor keeps it so); the flag tells the next shell to lock again.
    if (m_wl)
        m_wl->lock.reset();
}

bool SessionLock::isAvailable() const
{
    return m_simulate || (m_wl && m_wl->manager->isActive());
}

QString SessionLock::flagFilePath()
{
    const QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    return runtime.isEmpty() ? QString() : runtime + QStringLiteral("/meadow/locked");
}

void SessionLock::addSurface(QWindow *window)
{
    if (!window || m_windows.contains(window))
        return;
    m_windows.append(window);
    if (m_state != Unlocked && !m_simulate) {
        // A screen added while locked needs its own lock surface.
        window->create();
        if (auto *w = dynamic_cast<QtWaylandClient::QWaylandWindow *>(window->handle()))
            w->setShellIntegration(m_wl->integration.get());
        window->show();
    }
}

void SessionLock::removeSurface(QWindow *window)
{
    m_windows.removeAll(window);
}

bool SessionLock::unlockPossible()
{
    // A custom helper (tests, MEADOW_AUTH_HELPER) brings its own PAM setup.
    if (qEnvironmentVariableIsSet("MEADOW_AUTH_HELPER"))
        return true;
    const QString service = qEnvironmentVariableIsSet("MEADOW_PAM_SERVICE")
        ? qEnvironmentVariable("MEADOW_PAM_SERVICE") : QStringLiteral("meadow-lock");
    for (const char *dir : {"/etc/pam.d/", "/usr/lib/pam.d/", "/lib/pam.d/", "/usr/share/pam.d/"}) {
        if (QFileInfo::exists(QLatin1String(dir) + service))
            return true;
    }
    return false;
}

bool SessionLock::lock()
{
    if (m_state != Unlocked)
        return true;
    if (m_simulate) {
        qCInfo(lcLock) << "locking (simulated)";
        setState(Locking);
        setState(Locked);
        return true;
    }
    if (!m_wl->manager->isActive()) {
        qCWarning(lcLock) << "the compositor does not support ext-session-lock-v1; cannot lock";
        Q_EMIT lockFailed();
        return false;
    }
    if (!unlockPossible()) {
        // A lock the user could never leave is worse than none.
        qCWarning(lcLock) << "no meadow-lock PAM service is installed; not locking (sudo meadow-setup enable-lock)";
        Q_EMIT lockFailed();
        return false;
    }
    qCInfo(lcLock) << "locking" << m_windows.size() << "screen(s)";
    m_wl->lock = std::make_unique<SessionLockObject>(this, m_wl->manager->lock());
    setState(Locking);
    showSurfaces();
    return true;
}

void SessionLock::showSurfaces()
{
    for (const QPointer<QWindow> &window : std::as_const(m_windows)) {
        if (!window)
            continue;
        window->create();
        auto *w = dynamic_cast<QtWaylandClient::QWaylandWindow *>(window->handle());
        if (!w) {
            qCWarning(lcLock) << "lock window is not a Wayland window";
            continue;
        }
        w->setShellIntegration(m_wl->integration.get());
        window->show();
    }
}

void SessionLock::hideSurfaces()
{
    for (const QPointer<QWindow> &window : std::as_const(m_windows)) {
        if (window)
            window->hide();
    }
}

void SessionLock::onLocked()
{
    qCInfo(lcLock) << "session locked";
    writeFlag(true);
    setState(Locked);
}

void SessionLock::onFinished()
{
    // Before `locked`: another client holds the lock or the compositor
    // refused. After it: the compositor ended the lock on its own.
    const bool wasLocked = m_state == Locked;
    qCWarning(lcLock) << (wasLocked ? "the compositor ended the lock" : "the compositor refused the lock");
    hideSurfaces();
    if (m_wl->lock) {
        m_wl->lock->destroy();
        m_wl->lock.reset();
    }
    writeFlag(false);
    setState(Unlocked);
    if (!wasLocked)
        Q_EMIT lockFailed();
}

void SessionLock::unlock()
{
    if (m_state == Unlocked || m_unlocking)
        return;
    // Hiding the surfaces can end the lock screen's fade, which calls
    // unlock() again from inside the roundtrip below.
    m_unlocking = true;
    qCInfo(lcLock) << "unlocking";
    if (!m_simulate && m_wl->lock) {
        if (m_state == Locked)
            m_wl->lock->unlock_and_destroy();
        else
            m_wl->lock->destroy(); // still locking: abandon the request
        m_wl->lock.reset();
        hideSurfaces();
        // Make sure the compositor has processed the unlock before the
        // flag goes; otherwise a crash right now could leave it unlocked
        // with a stale flag (harmless) rather than the reverse.
        if (auto *native = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>())
            wl_display_roundtrip(native->display());
    }
    writeFlag(false);
    m_unlocking = false;
    setState(Unlocked);
}

void SessionLock::relockIfNeeded()
{
    const QString flag = flagFilePath();
    if (!flag.isEmpty() && QFile::exists(flag)) {
        qCInfo(lcLock) << "the previous shell left the session locked; locking again";
        lock();
    }
}

void SessionLock::writeFlag(bool locked)
{
    if (m_simulate)
        return;
    const QString flag = flagFilePath();
    if (flag.isEmpty())
        return;
    if (!locked) {
        QFile::remove(flag);
        return;
    }
    QDir().mkpath(QFileInfo(flag).absolutePath());
    QSaveFile f(flag);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QByteArray::number(QCoreApplication::applicationPid()) + '\n');
        f.commit();
    }
}

void SessionLock::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    Q_EMIT stateChanged();
}
