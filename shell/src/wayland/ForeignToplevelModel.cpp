// SPDX-License-Identifier: GPL-3.0-or-later
#include "ForeignToplevelModel.h"

#include "qwayland-wlr-foreign-toplevel-management-unstable-v1.h"

#include <QGuiApplication>
#include <QLoggingCategory>
#include <QtWaylandClient/QWaylandClientExtensionTemplate>

Q_LOGGING_CATEGORY(lcToplevel, "meadow.shell.toplevel")

class ForeignToplevelHandle : public QObject, public QtWayland::zwlr_foreign_toplevel_handle_v1
{
    Q_OBJECT

public:
    ForeignToplevelHandle(::zwlr_foreign_toplevel_handle_v1 *object, quint64 id)
        : QtWayland::zwlr_foreign_toplevel_handle_v1(object)
    {
        current.id = id;
        pending.id = id;
    }
    ~ForeignToplevelHandle() override { destroy(); }

    ToplevelModel::Window current;
    ToplevelModel::Window pending;
    bool announced = false;

Q_SIGNALS:
    void done();
    void closed();

protected:
    void zwlr_foreign_toplevel_handle_v1_title(const QString &title) override { pending.title = title; }
    void zwlr_foreign_toplevel_handle_v1_app_id(const QString &appId) override { pending.appId = appId; }
    void zwlr_foreign_toplevel_handle_v1_state(wl_array *state) override
    {
        pending.active = pending.minimized = pending.maximized = pending.fullscreen = false;
        const auto *states = static_cast<const uint32_t *>(state->data);
        for (size_t i = 0; i < state->size / sizeof(uint32_t); ++i) {
            switch (states[i]) {
            case state_activated: pending.active = true; break;
            case state_minimized: pending.minimized = true; break;
            case state_maximized: pending.maximized = true; break;
            case state_fullscreen: pending.fullscreen = true; break;
            default: break;
            }
        }
        // Activation answers any attention request.
        if (pending.active)
            pending.needsAttention = false;
    }
    void zwlr_foreign_toplevel_handle_v1_done() override
    {
        current = pending;
        Q_EMIT done();
    }
    void zwlr_foreign_toplevel_handle_v1_closed() override { Q_EMIT closed(); }
};

class ForeignToplevelManager : public QWaylandClientExtensionTemplate<ForeignToplevelManager>,
                               public QtWayland::zwlr_foreign_toplevel_manager_v1
{
    Q_OBJECT

public:
    ForeignToplevelManager()
        : QWaylandClientExtensionTemplate<ForeignToplevelManager>(3)
    {
        initialize();
    }
    ~ForeignToplevelManager() override
    {
        if (isActive())
            stop();
    }

Q_SIGNALS:
    void toplevel(::zwlr_foreign_toplevel_handle_v1 *handle);

protected:
    void zwlr_foreign_toplevel_manager_v1_toplevel(::zwlr_foreign_toplevel_handle_v1 *handle) override
    {
        Q_EMIT toplevel(handle);
    }
    void zwlr_foreign_toplevel_manager_v1_finished() override
    {
        qCWarning(lcToplevel) << "compositor stopped sending the window list";
    }
};

ForeignToplevelModel::ForeignToplevelModel(QObject *parent)
    : ToplevelModel(parent)
    , m_manager(std::make_unique<ForeignToplevelManager>())
{
    connect(m_manager.get(), &ForeignToplevelManager::toplevel, this, [this](::zwlr_foreign_toplevel_handle_v1 *object) {
        handleAdded(new ForeignToplevelHandle(object, m_nextId++));
    });
    connect(m_manager.get(), &ForeignToplevelManager::activeChanged, this, [this] {
        qCInfo(lcToplevel) << "wlr-foreign-toplevel-management" << (m_manager->isActive() ? "available" : "unavailable");
        Q_EMIT availableChanged();
    });
}

ForeignToplevelModel::~ForeignToplevelModel()
{
    qDeleteAll(m_handles);
}

bool ForeignToplevelModel::available() const
{
    return m_manager && m_manager->isActive();
}

void ForeignToplevelModel::handleAdded(ForeignToplevelHandle *handle)
{
    const quint64 id = handle->current.id;
    m_handles.insert(id, handle);
    connect(handle, &ForeignToplevelHandle::done, this, [this, handle] {
        if (!handle->announced) {
            handle->announced = true;
            addWindow(handle->current);
        } else {
            updateWindow(handle->current);
        }
    });
    connect(handle, &ForeignToplevelHandle::closed, this, [this, handle, id] {
        removeWindow(id);
        m_handles.remove(id);
        handle->deleteLater();
    });
}

namespace {
wl_seat *shellSeat()
{
    auto *wl = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    if (!wl)
        return nullptr;
    return wl->lastInputSeat() ? wl->lastInputSeat() : wl->seat();
}
} // namespace

void ForeignToplevelModel::requestActivate(quint64 id)
{
    if (auto *h = m_handles.value(id); h && shellSeat())
        h->activate(shellSeat());
}

void ForeignToplevelModel::requestMinimized(quint64 id, bool minimized)
{
    if (auto *h = m_handles.value(id))
        minimized ? h->set_minimized() : h->unset_minimized();
}

void ForeignToplevelModel::requestMaximized(quint64 id, bool maximized)
{
    if (auto *h = m_handles.value(id))
        maximized ? h->set_maximized() : h->unset_maximized();
}

void ForeignToplevelModel::requestClose(quint64 id)
{
    if (auto *h = m_handles.value(id))
        h->close();
}

#include "ForeignToplevelModel.moc"
