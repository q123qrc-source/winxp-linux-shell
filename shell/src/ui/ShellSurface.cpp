// SPDX-License-Identifier: GPL-3.0-or-later
#include "ShellSurface.h"

#include <LayerShellQt/Window>
#include <QGuiApplication>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcSurface, "meadow.shell.surface")

ShellSurface::ShellSurface(QObject *parent)
    : QObject(parent)
{
}

void ShellSurface::setWindow(QWindow *window)
{
    if (window == m_window)
        return;
    m_window = window;
    Q_EMIT windowChanged();
    attach();
}

void ShellSurface::setEnabled(bool enabled)
{
    if (enabled == m_enabled)
        return;
    m_enabled = enabled;
    Q_EMIT enabledChanged();
    attach();
}

void ShellSurface::componentComplete()
{
    m_complete = true;
    attach();
}

void ShellSurface::attach()
{
    if (!m_complete || !m_enabled || !m_window || m_layer)
        return;
    if (!QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
        qCWarning(lcSurface) << "not a Wayland session; using a normal window for" << m_window->title();
        return;
    }
    // The shell integration is chosen when the platform window is created.
    if (m_window->handle())
        qCWarning(lcSurface) << "window" << m_window->title() << "was shown before its ShellSurface was set up";
    m_layer = LayerShellQt::Window::get(m_window);
    push();
    Q_EMIT layerShellChanged();
}

void ShellSurface::push()
{
    if (!m_layer)
        return;
    m_layer->setScope(m_scope);
    m_layer->setLayer(LayerShellQt::Window::Layer(m_layerValue));
    m_layer->setAnchors(LayerShellQt::Window::Anchors(m_edges));
    m_layer->setExclusiveZone(m_exclusiveZone);
    m_layer->setMargins(QMargins(m_marginLeft, m_marginTop, m_marginRight, m_marginBottom));
    m_layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivity(m_keyboard));
    // A taskbar or balloon must not steal focus when it appears (LayerShellQt
    // 6.4 and newer; older versions never activate layer surfaces on show).
#if MEADOW_LAYERSHELLQT_ACTIVATE_ON_SHOW
    m_layer->setActivateOnShow(m_keyboard != KeyboardNone);
#endif
}

#define MEADOW_SURFACE_SETTER(name, member, type) \
    void ShellSurface::name(type v)               \
    {                                             \
        if (member == v)                          \
            return;                               \
        member = v;                               \
        push();                                   \
        Q_EMIT changed();                         \
    }

MEADOW_SURFACE_SETTER(setLayer, m_layerValue, Layer)
MEADOW_SURFACE_SETTER(setEdges, m_edges, int)
MEADOW_SURFACE_SETTER(setExclusiveZone, m_exclusiveZone, int)
MEADOW_SURFACE_SETTER(setMarginLeft, m_marginLeft, int)
MEADOW_SURFACE_SETTER(setMarginRight, m_marginRight, int)
MEADOW_SURFACE_SETTER(setMarginBottom, m_marginBottom, int)
MEADOW_SURFACE_SETTER(setMarginTop, m_marginTop, int)
MEADOW_SURFACE_SETTER(setKeyboard, m_keyboard, Keyboard)

#undef MEADOW_SURFACE_SETTER
