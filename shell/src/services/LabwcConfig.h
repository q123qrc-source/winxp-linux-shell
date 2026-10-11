// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

class Settings;
class QTimer;

// Applies the shell's compositor-side settings to labwc: the decoration
// theme that matches the color scheme ([Appearance] scheme), key repeat
// ([Keyboard]), double-click time, pointer speed and left-handed buttons
// ([Mouse]).
//
// meadow-session runs labwc on a per-session overlay directory of symlinks
// (MEADOW_LABWC_OVERLAY) to the real configuration (MEADOW_LABWC_SOURCE).
// This class replaces the overlay's rc.xml symlink with an edited copy of the
// real rc.xml (atomically) and asks labwc to reconfigure. Neither the user's
// nor the packaged configuration is ever modified; without an overlay the
// settings are only saved and apply from the next session.
class LabwcConfig : public QObject
{
    Q_OBJECT

public:
    explicit LabwcConfig(Settings *settings, QObject *parent = nullptr);
    // For tests: explicit directories and no `labwc --reconfigure`.
    LabwcConfig(Settings *settings, const QString &overlayDir, const QString &sourceDir, bool reconfigure,
                QObject *parent = nullptr);

    bool hasOverlay() const { return !m_overlay.isEmpty(); }
    // labwc theme name for a Meadow color scheme id.
    static QString themeForScheme(const QString &schemeId);
    // The real rc.xml with the settings applied (exposed for tests).
    QByteArray renderRcXml() const;

public Q_SLOTS:
    // Writes overlay/rc.xml if it would differ from what labwc runs with;
    // debounced, so several settings changed together reconfigure once.
    void scheduleApply();
    // Applies now; returns false when there is no overlay or writing failed.
    bool apply();

Q_SIGNALS:
    void applied();

private:
    Settings *m_settings;
    QString m_overlay;
    QString m_source;
    bool m_reconfigure = true;
    QByteArray m_lastWritten;
    QTimer *m_timer;
};
