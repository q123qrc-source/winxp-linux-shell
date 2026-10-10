// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QPointer>
#include <QQmlParserStatus>
#include <QWindow>
#include <QtQml/qqmlregistration.h>

namespace LayerShellQt { class Window; }

// Turns a QML Window into a wlr-layer-shell surface (taskbar, popups,
// balloons). Declare it inside the Window, before the window is shown:
//
//   Window {
//       id: bar
//       ShellSurface {
//           window: bar
//           enabled: !Shell.preview
//           layer: ShellSurface.LayerTop
//           edges: ShellSurface.AnchorBottom | ShellSurface.AnchorLeft | ShellSurface.AnchorRight
//           exclusiveZone: bar.height
//       }
//   }
//
// When disabled (preview mode, or not running on Wayland) the window stays an
// ordinary toplevel, so the same QML runs anywhere.
class ShellSurface : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    QML_ELEMENT

    Q_PROPERTY(QWindow *window READ window WRITE setWindow NOTIFY windowChanged)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool layerShell READ isLayerShell NOTIFY layerShellChanged)
    Q_PROPERTY(Layer layer READ layer WRITE setLayer NOTIFY changed)
    Q_PROPERTY(int edges READ edges WRITE setEdges NOTIFY changed)
    Q_PROPERTY(int exclusiveZone READ exclusiveZone WRITE setExclusiveZone NOTIFY changed)
    Q_PROPERTY(int marginLeft MEMBER m_marginLeft WRITE setMarginLeft NOTIFY changed)
    Q_PROPERTY(int marginRight MEMBER m_marginRight WRITE setMarginRight NOTIFY changed)
    Q_PROPERTY(int marginBottom MEMBER m_marginBottom WRITE setMarginBottom NOTIFY changed)
    Q_PROPERTY(int marginTop MEMBER m_marginTop WRITE setMarginTop NOTIFY changed)
    Q_PROPERTY(Keyboard keyboard READ keyboard WRITE setKeyboard NOTIFY changed)
    Q_PROPERTY(QString scope MEMBER m_scope NOTIFY changed)

public:
    // Values match LayerShellQt::Window.
    enum Layer { LayerBackground, LayerBottom, LayerTop, LayerOverlay };
    Q_ENUM(Layer)
    enum Anchor { AnchorTop = 1, AnchorBottom = 2, AnchorLeft = 4, AnchorRight = 8 };
    Q_ENUM(Anchor)
    enum Keyboard { KeyboardNone, KeyboardExclusive, KeyboardOnDemand };
    Q_ENUM(Keyboard)

    explicit ShellSurface(QObject *parent = nullptr);

    QWindow *window() const { return m_window; }
    void setWindow(QWindow *window);
    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    bool isLayerShell() const { return m_layer != nullptr; }
    Layer layer() const { return m_layerValue; }
    void setLayer(Layer layer);
    int edges() const { return m_edges; }
    void setEdges(int edges);
    int exclusiveZone() const { return m_exclusiveZone; }
    void setExclusiveZone(int zone);
    void setMarginLeft(int v);
    void setMarginRight(int v);
    void setMarginBottom(int v);
    void setMarginTop(int v);
    Keyboard keyboard() const { return m_keyboard; }
    void setKeyboard(Keyboard keyboard);

    void classBegin() override {}
    void componentComplete() override;

Q_SIGNALS:
    void windowChanged();
    void enabledChanged();
    void layerShellChanged();
    void changed();

private:
    void attach();
    void push();

    QPointer<QWindow> m_window;
    LayerShellQt::Window *m_layer = nullptr;
    bool m_complete = false;
    bool m_enabled = true;
    Layer m_layerValue = LayerTop;
    int m_edges = 0;
    int m_exclusiveZone = 0;
    int m_marginLeft = 0, m_marginRight = 0, m_marginBottom = 0, m_marginTop = 0;
    Keyboard m_keyboard = KeyboardNone;
    QString m_scope = QStringLiteral("meadow-shell");
};
