// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QStringList>

// Start menu state in ~/.local/share/meadow/start-menu.json: launch
// frequency, pinned items, entries removed from the frequent list, and when
// each application was first seen (for "newly installed" highlighting).
// Written atomically after each change. Holds desktop ids only.
class UsageStore : public QObject
{
    Q_OBJECT

public:
    explicit UsageStore(QObject *parent = nullptr);
    // For tests: an explicit file and clock.
    explicit UsageStore(const QString &file, QObject *parent = nullptr);

    static QString defaultFile();

    // Frequency score with a one-week half-life: recent use counts more.
    static constexpr double kHalfLifeDays = 7.0;

    void recordLaunch(const QString &id);
    double score(const QString &id) const;
    // Ids by descending score (only ids with a score).
    QStringList byScore() const;

    // Pinned items; hasPinned() is false until the user changes them, so the
    // packaged defaults apply.
    bool hasPinned() const { return m_pinnedSet; }
    QStringList pinned() const { return m_pinned; }
    void setPinned(const QStringList &ids);

    void removeFromFrequent(const QString &id);
    bool isRemoved(const QString &id) const { return m_removed.contains(id); }

    // First-seen bookkeeping. On the very first run every installed app is
    // recorded as old, so nothing is highlighted as new.
    void noteInstalled(const QStringList &ids);
    bool isNew(const QString &id, int days) const;

    void setNow(const QDateTime &now) { m_now = now; } // tests
    QDateTime now() const { return m_now.isValid() ? m_now : QDateTime::currentDateTimeUtc(); }

Q_SIGNALS:
    void changed();

private:
    void load();
    void save();

    struct Launches {
        double score = 0;
        qint64 last = 0; // seconds since epoch
    };
    QString m_file;
    QHash<QString, Launches> m_launches;
    QStringList m_pinned;
    bool m_pinnedSet = false;
    QStringList m_removed;
    QHash<QString, qint64> m_seen;
    bool m_initialized = false;
    QDateTime m_now;
};
