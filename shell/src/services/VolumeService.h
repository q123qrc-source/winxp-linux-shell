// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QTimer>

// Default audio output volume through WirePlumber's `wpctl` (PipeWire).
// Polls every few seconds and right after its own changes; unavailable when
// wpctl is missing or no sink exists. MEADOW_WPCTL overrides the program
// (tests).
class VolumeService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY changed)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY changed)

public:
    explicit VolumeService(QObject *parent = nullptr);

    bool available() const { return m_available; }
    qreal volume() const { return m_volume; }
    bool muted() const { return m_muted; }
    void setVolume(qreal volume);
    void setMuted(bool muted);

    Q_INVOKABLE void refresh();

    // Parses `wpctl get-volume` output ("Volume: 0.40 [MUTED]").
    static bool parse(const QString &output, qreal *volume, bool *muted);

Q_SIGNALS:
    void changed();

private:
    QString program() const;
    void run(const QStringList &args, bool refreshAfter);

    QTimer m_poll;
    bool m_available = false;
    qreal m_volume = 0;
    bool m_muted = false;
};
