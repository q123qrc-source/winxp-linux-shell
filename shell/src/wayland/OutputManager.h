// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QObject>
#include <QVariantList>
#include <memory>

// Display Properties > Settings: the outputs (monitors) and their modes via
// wlr-output-management-unstable-v1, and changing resolution, refresh rate
// and scale.
//
// `outputs`: [{name, description, enabled, x, y, width, height, refresh
// (mHz), scale, currentMode, modes: [{width, height, refresh, preferred}]}].
// apply() first asks the compositor to test the configuration, then applies
// it; the previous configuration is kept so revert() can restore it (the
// "Keep these settings?" countdown). Outputs without a mode list (headless,
// some virtual displays) take custom modes.
//
// Simulated in --preview: one 1024x768 display with a few modes.
class OutputManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ isAvailable NOTIFY availableChanged)
    Q_PROPERTY(QVariantList outputs READ outputs NOTIFY outputsChanged)
    Q_PROPERTY(bool canRevert READ canRevert NOTIFY outputsChanged)

public:
    struct Mode {
        int width = 0;
        int height = 0;
        int refresh = 0; // mHz
        bool preferred = false;
        bool operator==(const Mode &o) const { return width == o.width && height == o.height && refresh == o.refresh; }
    };
    struct Output {
        QString name;
        QString description;
        bool enabled = true;
        int x = 0;
        int y = 0;
        Mode current;
        double scale = 1.0;
        QList<Mode> modes;
    };

    explicit OutputManager(bool simulate, QObject *parent = nullptr);
    ~OutputManager() override;

    bool isAvailable() const;
    QVariantList outputs() const;
    bool canRevert() const { return !m_previous.isEmpty(); }

    // Changes `name` to the given size/refresh (refresh 0: the mode's best)
    // and scale. Emits applied(ok, message) when the compositor answers.
    Q_INVOKABLE void apply(const QString &name, int width, int height, int refresh, double scale);
    // Restores the configuration from before the last apply().
    Q_INVOKABLE void revert();
    // Forgets the previous configuration (the user kept the new one).
    Q_INVOKABLE void keep();

    // Internal (Wayland objects).
    struct Wayland;
    void setOutputs(const QList<Output> &outputs);

Q_SIGNALS:
    void availableChanged();
    void outputsChanged();
    void applied(bool ok, const QString &message);

private:
    void applyConfiguration(const QList<Output> &target, bool remember);

    bool m_simulate;
    QList<Output> m_outputs;
    QList<Output> m_previous;
    std::unique_ptr<Wayland> m_wl;
};
